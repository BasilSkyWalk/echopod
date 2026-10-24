#include "image/image_arena.h"

#include <stddef.h>

enum {
    ALIGNMENT = 8
};

ImageArena image_arena_make(void *memory, uint32_t size)
{
    uintptr_t address = (uintptr_t)memory;
    uint32_t skip = (uint32_t)((ALIGNMENT - address % ALIGNMENT) % ALIGNMENT);
    if (memory == NULL || size < skip) {
        return (ImageArena){.base = NULL, .size = 0, .used = 0};
    }
    return (ImageArena){.base = (uint8_t *)memory + skip, .size = size - skip, .used = 0};
}

void *image_arena_take(ImageArena *arena, uint32_t bytes)
{
    uint32_t rounded = (bytes + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
    if (arena->base == NULL || rounded < bytes || rounded > arena->size - arena->used) {
        return NULL;
    }
    void *block = arena->base + arena->used;
    arena->used += rounded;
    return block;
}
