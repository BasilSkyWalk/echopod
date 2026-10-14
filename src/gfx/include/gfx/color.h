#pragma once

#include <stdint.h>

enum {
    COLOR_ALPHA_OPAQUE = 15
};

typedef uint16_t Color;

#define COLOR_RGB(red, green, blue)                                                                \
    ((Color)((((red) & 0xF8) << 8) | (((green) & 0xFC) << 3) | ((blue) >> 3)))

Color color_from_rgb(uint8_t red, uint8_t green, uint8_t blue);
Color color_blend(Color background, Color foreground, uint8_t alpha);
