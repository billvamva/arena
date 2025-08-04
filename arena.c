#include "arena.h"
#include <assert.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

arena_t new_arena(size_t cap, mem_profile_t* profile)
{
    arena_t a = { 0 };
    a.start = malloc(cap);
    assert(a.start != NULL);
    a.beg = a.start;
    a.end = a.beg ? a.beg + cap : 0;
    a.profile = profile;
    return a;
}

void free_arena(arena_t* a)
{
    if (a->start) {
        free(a->start);
        a->beg = 0;
        a->end = 0;
    }
    if (a->profile) {
        free(a->profile);
    }
}

static uint8_t record_call_stack(uint64_t* dst, uint64_t cap)
{
    uintptr_t* rbp = __builtin_frame_address(0);
    uint64_t len = 0;

    while (rbp != 0 && ((uint64_t)rbp & 7) == 0 && *rbp != 0) {
        const uintptr_t rip = *(rbp + 1);
        rbp = (uintptr_t*)*rbp;

        // should get call xxx instruction
        dst[len++] = rip - 1;
        if (len >= cap)
            return len;
    }
    return len;
}

static mem_record_t create_record(arena_t* arena,
    uint64_t objects_count,
    uint64_t bytes_count,
    uint64_t* call_stack,
    uint64_t call_stack_len)
{

    mem_record_t record = {
        .alloc_objects = objects_count,
        .alloc_space = bytes_count,
        .in_use_objects = objects_count,
        .in_use_space = bytes_count,
    };
    record.call_stack = new (arena, uint64_t, call_stack_len);
    memcpy(record.call_stack, call_stack, call_stack_len * sizeof(uint64_t));
    record.call_stack_len = call_stack_len;

    return record;
}

static void insert_record(mem_record_t record, mem_profile_t* profile)
{

    if (profile->records_len >= profile->records_cap) {
        uint64_t new_cap = profile->records_cap * 2;
        // Grow the array.
        mem_record_t* new_records = alloc(
            profile->arena, sizeof(mem_record_t), _Alignof(mem_record_t), new_cap);
        memcpy(new_records, profile->records,
            profile->records_len * sizeof(mem_record_t));
        profile->records_cap = new_cap;
        profile->records = new_records;
    }
    profile->records[profile->records_len++] = record;
}

bool upsert_record(mem_profile_t* profile, uint64_t* call_stack, uint64_t call_stack_len,
    uint64_t objects_count,
    uint64_t bytes_count)
{
    // look for identical callstack
    for (uint64_t i = 0; i < profile->records_len; i++) {
        mem_record_t* r = &profile->records[i];

        if (r->call_stack_len == call_stack_len && memcmp(r->call_stack, call_stack, call_stack_len * sizeof(uint64_t)) == 0) {
            // Found an existing record, update it.
            r->alloc_objects += objects_count;
            r->alloc_space += bytes_count;
            r->in_use_objects += objects_count;
            r->in_use_space += bytes_count;
            return true;
        }
    }
    return false;
}

static void mem_profile_record_alloc(mem_profile_t* profile,
    uint64_t objects_count,
    uint64_t bytes_count)
{
    // Record the call stack by stack walking.
    uint64_t call_stack[64] = { 0 };
    uint64_t call_stack_len = record_call_stack(call_stack, sizeof(call_stack) / sizeof(call_stack[0]));

    // Update the sums.
    profile->alloc_objects += objects_count;
    profile->alloc_space += bytes_count;
    profile->in_use_objects += objects_count;
    profile->in_use_space += bytes_count;

    // Upsert the record.
    if (upsert_record(profile, call_stack, call_stack_len, objects_count, bytes_count)) {
        return;
    }

    // Not found, insert a new record
    mem_record_t record = create_record(profile->arena, objects_count, bytes_count, call_stack, call_stack_len);

    insert_record(record, profile);
}

void* alloc(arena_t* a, size_t size, size_t align, size_t count)
{
    if (count == 0) {
        printf("COUNT IS 0");
    }
    uint8_t* old_beg = a->beg;

    size_t padding = -(uintptr_t)a->beg & (align - 1);
    size_t available = a->end - a->beg - padding;
    if (available < 0 || count > available / size) {
        abort(); // one possible out-of-memory policy
    }
    void* p = a->beg + padding;
    size_t offset = padding + count * size;
    a->beg += offset;
    uint8_t* result = old_beg + padding;

    if (a->profile) {
        mem_profile_record_alloc(a->profile, count, offset);
    }
    return memset(p, 0, count * size);
}

mem_profile_t new_mem_profile(arena_t* a)
{
    mem_profile_t mem_profile = {
        .arena = a,
        .records_cap = 16, // Non-zero initial capacity
        .records_len = 0,
    };

    mem_profile.records = new (a, mem_record_t, mem_profile.records_cap);

    return mem_profile;
}

static void process_vmmap_line(char* buffer, bool* parsing_regions, char* binary_path, FILE* out)
{
    // Remove trailing newline
    char* newline = strchr(buffer, '\n');
    if (newline)
        *newline = '\0';

    // Skip until we reach the regions section
    if (strstr(buffer, "REGION TYPE")) {
        *parsing_regions = true;
        return;
    }

    if (!(*parsing_regions))
        return;

    // Skip separator lines and empty lines
    if (strstr(buffer, "====") || strlen(buffer) < 20) {
        return;
    }

    // Look for lines that start with region types and have address ranges
    // Format: REGION_TYPE    START-END    [SIZE] PERMS ...
    char* first_space = strchr(buffer, ' ');
    if (!first_space)
        return;

    // Find the address range (looks like "1047bc000-1047c0000")
    char* addr_start = first_space;
    while (*addr_start == ' ')
        addr_start++; // skip spaces

    char* addr_end = strchr(addr_start, ' ');
    if (!addr_end)
        return;

    // Extract address range
    size_t addr_len = addr_end - addr_start;
    char addr_range[64];
    strncpy(addr_range, addr_start, addr_len);
    addr_range[addr_len] = '\0';

    // Check if it looks like an address range
    if (!strchr(addr_range, '-'))
        return;

    // Split the address range
    char* dash = strchr(addr_range, '-');
    if (!dash)
        return;
    *dash = '\0';
    char* start_addr = addr_range;
    char* end_addr = dash + 1;

    // Find permissions - look for pattern like "r-x/r-x"
    char perms[8] = "---p";
    char* perm_pos = strstr(buffer, "/");
    if (perm_pos) {
        // Look backwards from the / to find the start of permissions
        char* perm_start = perm_pos;
        while (perm_start > buffer && *(perm_start - 1) != ' ') {
            perm_start--;
        }

        // Extract permissions before the /
        if (perm_start[0] == 'r')
            perms[0] = 'r';
        if (perm_start[1] == 'w')
            perms[1] = 'w';
        if (perm_start[2] == 'x')
            perms[2] = 'x';
    }

    // Find pathname - look for something starting with /
    char* pathname = "";
    char* path_start = strrchr(buffer, '/');
    if (path_start) {
        // For the main binary, use the full path we got earlier
        if (strstr(path_start, "main") && !strstr(buffer, ".dylib")) {
            pathname = binary_path; // Use full path like /Users/.../build/main
        } else {
            pathname = path_start; // Use library paths as-is
        }
    } else {
        // Check for special regions
        if (strstr(buffer, "MALLOC") || strstr(buffer, "__DATA")) {
            pathname = "[heap]";
        } else if (strstr(buffer, "Stack")) {
            pathname = "[stack]";
        }
    }

    // Output in /proc/maps format
    fprintf(out, "%s-%s %s 00000000 00:00 0 %s\n",
        start_addr, end_addr, perms, pathname);
}

void mem_profile_write(mem_profile_t* profile, FILE* out)
{
    fprintf(out, "heap profile: %llu: %llu [     %llu:    %llu] @ heapprofile\n",
        profile->in_use_objects, profile->in_use_space,
        profile->alloc_objects, profile->alloc_space);

    for (uint64_t i = 0; i < profile->records_len; i++) {
        mem_record_t r = profile->records[i];

        fprintf(out, "%llu: %llu [%llu: %llu] @ ", r.in_use_objects, r.in_use_space,
            r.alloc_objects, r.alloc_space);

        for (uint64_t j = 0; j < r.call_stack_len; j++) {
            fprintf(out, "%#llx ", r.call_stack[j]);
        }
        fputc('\n', out);
    }

    fputs("\nMAPPED_LIBRARIES:\n", out);
    // Get the actual binary path
    char binary_path[1024];
    uint32_t size = sizeof(binary_path);
    if (_NSGetExecutablePath(binary_path, &size) != 0) {
        strcpy(binary_path, "main"); // fallback
    }

    // Use vmmap and parse the output manually
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "vmmap %d", getpid());

    FILE* vmmap_pipe = popen(cmd, "r");
    if (vmmap_pipe) {
        char buffer[4096];
        bool parsing_regions = false;

        while (fgets(buffer, sizeof(buffer), vmmap_pipe)) {
            process_vmmap_line(buffer, &parsing_regions, binary_path, out);
        }
        pclose(vmmap_pipe);
    }
    fflush(out);
}
