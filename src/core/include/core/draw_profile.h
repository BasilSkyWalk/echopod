#pragma once

#include <stdint.h>

typedef enum DrawPart {
    DRAW_PART_COVERS,
    DRAW_PART_REFLECTIONS,
    DRAW_PART_TEXT,
    DRAW_PART_BACKDROP,
    DRAW_PART_BOXES,
    DRAW_PART_SHAPES,
    DRAW_PART_HASH,
    DRAW_PART_AUDIO,
    DRAW_PART_COUNT
} DrawPart;

enum {
    DRAW_PROFILE_WINDOW = 32
};

void draw_profile_reset(void);
void draw_profile_use_clock(uint32_t (*clock)(void));
uint32_t draw_profile_now(void);
void draw_profile_since(DrawPart part, uint32_t started_us);
void draw_profile_add(DrawPart part, uint32_t microseconds);
void draw_profile_finish_frame(void);
uint32_t draw_profile_average_us(DrawPart part);
