#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct mem_profile_t mem_profile_t;

typedef struct arena_t {
    uint8_t* start;
    uint8_t* beg;
    uint8_t* end;
    mem_profile_t* profile;
    uint64_t flags;
} arena_t;

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
    arena_t* arena;
};

#define new(a, t, n) (t*)alloc(a, sizeof(t), _Alignof(t), n)

arena_t new_arena(size_t cap, mem_profile_t* profile);
void free_arena(arena_t* a);
void* alloc(arena_t* a, size_t size, size_t alignment, size_t count);
void mem_profile_write(mem_profile_t* profile, FILE* out);
mem_profile_t new_mem_profile(arena_t* a);

#endif
