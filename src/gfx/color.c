#include "gfx/color.h"

enum {
    RED_MASK = 0xF8,
    GREEN_MASK = 0xFC,
    RED_SHIFT = 11,
    GREEN_SHIFT = 5,
    RGB_RED_SHIFT = 8,
    RGB_GREEN_SHIFT = 3,
    RGB_BLUE_SHIFT = 3,
    FIVE_BIT_MASK = 0x1F,
    SIX_BIT_MASK = 0x3F
};

typedef struct Channels {
    unsigned red;
    unsigned green;
    unsigned blue;
} Channels;

static Channels split(Color color)
{
    return (Channels){
        .red = (color >> RED_SHIFT) & FIVE_BIT_MASK,
        .green = (color >> GREEN_SHIFT) & SIX_BIT_MASK,
        .blue = color & FIVE_BIT_MASK,
    };
}

static Color join(Channels channels)
{
    return (Color)((channels.red << RED_SHIFT) | (channels.green << GREEN_SHIFT) | channels.blue);
}

static unsigned mix(unsigned background, unsigned foreground, unsigned alpha)
{
    unsigned weighted = background * (COLOR_ALPHA_OPAQUE - alpha) + foreground * alpha;
    return (weighted + COLOR_ALPHA_OPAQUE / 2) / COLOR_ALPHA_OPAQUE;
}

Color color_from_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    unsigned red_bits = (unsigned)(red & RED_MASK) << RGB_RED_SHIFT;
    unsigned green_bits = (unsigned)(green & GREEN_MASK) << RGB_GREEN_SHIFT;
    unsigned blue_bits = (unsigned)blue >> RGB_BLUE_SHIFT;
    return (Color)(red_bits | green_bits | blue_bits);
}

Color color_blend(Color background, Color foreground, uint8_t alpha)
{
    unsigned weight = alpha < COLOR_ALPHA_OPAQUE ? alpha : COLOR_ALPHA_OPAQUE;
    Channels back = split(background);
    Channels fore = split(foreground);
    return join((Channels){
        .red = mix(back.red, fore.red, weight),
        .green = mix(back.green, fore.green, weight),
        .blue = mix(back.blue, fore.blue, weight),
    });
}
