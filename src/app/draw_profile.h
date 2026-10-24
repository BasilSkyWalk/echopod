#pragma once

#include <stdint.h>

typedef enum DrawPart {
    DRAW_PART_COVERS,
    DRAW_PART_REFLECTIONS,
    DRAW_PART_TEXT,
    DRAW_PART_BACKDROP,
    DRAW_PART_HASH,
    DRAW_PART_AUDIO,
    DRAW_PART_COUNT
} DrawPart;

void draw_profile_reset(void);
void draw_profile_add(DrawPart part, uint32_t microseconds);
void draw_profile_finish_frame(void);
uint32_t draw_profile_average_us(DrawPart part);
