#pragma once

#include <stdint.h>

enum {
    PANIC_GLYPH_COLUMNS = 5,
    PANIC_GLYPH_ADVANCE = 6,
    PANIC_GLYPH_ROWS = 7
};

typedef struct PanicColors {
    uint16_t foreground;
    uint16_t background;
} PanicColors;

void panic_render_glyph(char character, int scale, PanicColors colors, uint16_t *pixels);
