#pragma once

#include <stdint.h>

typedef struct ImageArena {
    uint8_t *base;
    uint32_t size;
    uint32_t used;
} ImageArena;

ImageArena image_arena_make(void *memory, uint32_t size);
void *image_arena_take(ImageArena *arena, uint32_t bytes);
