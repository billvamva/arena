#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct arena {
    char* start;
    char* beg;
    char* end;
} arena;

#define new(a, t, n) (t*)alloc(a, sizeof(t), _Alignof(t), n)

arena new_arena(ptrdiff_t cap);
void free_arena(arena* a);
void* alloc(arena* a, ptrdiff_t size, ptrdiff_t alignment, ptrdiff_t count);

#endif
