#include "arena.h"
#include <stdio.h>
#include <string.h>

void b(int n, arena_t* arena)
{
    new (arena, int, n);
}

void a(int n, arena_t* arena)
{
    new (arena, int, n);
    b(n, arena);
}

void scratch_example()
{

    arena_t perm = new_arena(1024 * 1024, NULL); // 1MB permanent arena
    arena_t scratch = new_arena(64 * 1024, NULL); // 64KB scratch arena

    // Allocate some data
    int* numbers = new (&perm, int, 100);
    char* buffer = new (&scratch, char, 1000);
    double* values = new (&perm, double, 50);

    // Use the data
    for (int i = 0; i < 100; i++) {
        numbers[i] = i * i;
    }

    strcpy(buffer, "Hello, arena allocation!");
    printf("Buffer: %s\n", buffer);
    printf("First number: %d\n", numbers[0]);
    printf("Last number: %d\n", numbers[99]);

    free_arena(&perm);
    free_arena(&scratch);
}

// Example usage
int main()
{
    arena_t mem_profile_arena = new_arena(1 << 16, NULL);
    mem_profile_t mem_profile = new_mem_profile(&mem_profile_arena);

    arena_t arena = new_arena(1 << 28, &mem_profile);

    for (int i = 0; i < 2; i++)
        a(2 * 1024 * 1024, &arena);

    b(3 * 1024 * 1024, &arena);

    mem_profile_write(&mem_profile, stderr);

    return 0;
}
