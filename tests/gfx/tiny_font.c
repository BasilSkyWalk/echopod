#include "tiny_font.h"

static const uint8_t TINY_BITMAP[] = {0x80, 0xF0, 0x0F, 0x4F};

static const Glyph TINY_GLYPHS[] = {
    {'?', 0, 1, 1, 0, 1, 2},
    {'A', 1, 2, 2, 1, 2, 4},
    {'B', 3, 2, 1, 0, 1, 3},
};

const Font TINY_FONT = {
    .glyphs = TINY_GLYPHS,
    .bitmap = TINY_BITMAP,
    .glyph_count = 3,
    .bitmap_size = sizeof TINY_BITMAP,
    .ascent = 2,
    .descent = 1,
};

const Font TINY_FONT_WITHOUT_FALLBACK = {
    .glyphs = TINY_GLYPHS + 1,
    .bitmap = TINY_BITMAP,
    .glyph_count = 2,
    .bitmap_size = sizeof TINY_BITMAP,
    .ascent = 2,
    .descent = 1,
};
