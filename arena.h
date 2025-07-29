#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct mem_profile_t mem_profile_t;

typedef struct arena {
    char* start;
    char* beg;
    char* end;
    mem_profile_t* profile;
} arena;

typedef struct mem_record_t {
    uint64_t in_use_space, in_use_objects, alloc_space, alloc_objects;
    uint64_t* call_stack;
    uint64_t call_stack_len;
} mem_record_t;

struct mem_profile_t {
    mem_record_t* records;
    uint64_t records_len;
    uint64_t records_cap;
    uint64_t in_use_space, in_use_objects, alloc_space, alloc_objects;
    arena* arena;
};

#define new(a, t, n) (t*)alloc(a, sizeof(t), _Alignof(t), n)

arena new_arena(ptrdiff_t cap, mem_profile_t* profile);
void free_arena(arena* a);
void* alloc(arena* a, ptrdiff_t size, ptrdiff_t alignment, ptrdiff_t count);

#endif
