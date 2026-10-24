#pragma once

#include <stdint.h>

enum {
    COLOR_ALPHA_OPAQUE = 15,
    COLOR_LIGHT_SHIFT = 8,
    COLOR_LIGHT_FULL = 1 << COLOR_LIGHT_SHIFT
};

typedef uint16_t Color;

#define COLOR_RGB(red, green, blue)                                                                \
    ((Color)((((red) & 0xF8) << 8) | (((green) & 0xFC) << 3) | ((blue) >> 3)))

Color color_from_rgb(uint8_t red, uint8_t green, uint8_t blue);
Color color_blend(Color background, Color foreground, uint8_t alpha);
Color color_scale(Color color, unsigned light);
Color color_between(Color from, Color to, int step, int steps);
