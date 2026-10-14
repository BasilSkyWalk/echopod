#include "gfx/color.h"

enum {
    RED_MASK = 0xF8,
    GREEN_MASK = 0xFC,
    RED_SHIFT = 8,
    GREEN_SHIFT = 3,
    BLUE_SHIFT = 3
};

Color color_from_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    unsigned red_bits = (unsigned)(red & RED_MASK) << RED_SHIFT;
    unsigned green_bits = (unsigned)(green & GREEN_MASK) << GREEN_SHIFT;
    unsigned blue_bits = (unsigned)blue >> BLUE_SHIFT;
    return (Color)(red_bits | green_bits | blue_bits);
}
