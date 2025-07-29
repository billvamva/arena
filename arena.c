#include "arena.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

arena new_arena(ptrdiff_t cap, mem_profile_t* profile)
{
    arena a = { 0 };
    a.start = malloc(cap);
    if (a.start == NULL) {
        abort();
    }
    a.beg = a.start;
    a.end = a.beg ? a.beg + cap : 0;
    a.profile = profile;
    return a;
}

void free_arena(arena* a)
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

void* alloc(arena* a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count)
{
    ptrdiff_t padding = -(uintptr_t)a->beg & (align - 1);
    ptrdiff_t available = a->end - a->beg - padding;
    if (available < 0 || count > available / size) {
        abort();
    }
    void* p = a->beg + padding;
    a->beg += padding + count * size;

    if (a->profile) {
        // here we need to record stuff
    }

    return memset(p, 0, count * size);
}

static uint8_t record_call_stack(uint64_t* dst, uint64_t cap)
{
    uintptr_t* rbp = __builtin_frame_address(0);

    uint64_t len = 0;

    while (rbp != 0 && ((uint64_t)rbp & 7) == 0 && *rbp != 0) {
        const uintptr_t rip = *(rbp + 1);
        rbp = (uintptr_t*)*rbp;

        // `rip` points to the return instruction in the caller, once this call is
        // done. But: We want the location of the call i.e. the `call xxx`
        // instruction, so we subtract one byte to point inside it, which is not
        // quite 'at' it, but good enough.
        dst[len++] = rip - 1;

        if (len >= cap)
            return len;
    }
    return len;
}
