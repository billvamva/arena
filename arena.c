#include "arena.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

arena new_arena(ptrdiff_t cap)
{
    arena a = { 0 };
    a.start = malloc(cap);
    if (a.start == NULL) {
        abort();
    }
    a.beg = a.start;
    a.end = a.beg ? a.beg + cap : 0;
    return a;
}

void free_arena(arena* a)
{
    if (a->start) {
        free(a->start);
        a->beg = 0;
        a->end = 0;
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
    return memset(p, 0, count * size);
}
