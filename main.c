#include "arena.h"
#include <stdio.h>
#include <string.h>

// Example usage
int main()
{
    arena perm = new_arena(1024 * 1024); // 1MB permanent arena
    arena scratch = new_arena(64 * 1024); // 64KB scratch arena

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

    return 0;
}
