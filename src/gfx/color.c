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
    SIX_BIT_MASK = 0x3F,
    SPREAD_SHIFT = 16,
    SPREAD_LIGHT_BITS = 5,
    SPREAD_LIGHT_DROP = COLOR_LIGHT_SHIFT - SPREAD_LIGHT_BITS
};

static const uint32_t SPREAD_MASK = 0x07E0F81Fu;

enum {
    LUMINANCE_RED_WEIGHT = 2126,
    LUMINANCE_GREEN_WEIGHT = 7152,
    LUMINANCE_BLUE_WEIGHT = 722,
    LUMINANCE_WEIGHT_TOTAL = 10000
};

static const uint16_t FIVE_BIT_LIGHT[] = {0,    24,   52,   91,   152,  222,  307,  409,
                                          545,  685,  844,  1022, 1248, 1470, 1714, 1981,
                                          2307, 2623, 2961, 3325, 3763, 4179, 4621, 5089,
                                          5647, 6172, 6724, 7305, 7991, 8632, 9301, 10000};

static const uint16_t SIX_BIT_LIGHT[] = {
    0,    12,   24,   37,   52,   70,   91,   116,  144,  176,  212,  252,  296,
    343,  395,  452,  529,  595,  666,  742,  823,  908,  999,  1095, 1195, 1301,
    1413, 1529, 1651, 1779, 1912, 2051, 2232, 2384, 2542, 2705, 2874, 3050, 3231,
    3419, 3613, 3813, 4020, 4233, 4452, 4678, 4910, 5149, 5457, 5711, 5972, 6240,
    6514, 6795, 7084, 7379, 7682, 7991, 8308, 8632, 8963, 9301, 9647, 10000};

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

Color color_scale(Color color, unsigned light)
{
    if (light >= COLOR_LIGHT_FULL) {
        return color;
    }
    uint32_t spread = ((uint32_t)color | (uint32_t)color << SPREAD_SHIFT) & SPREAD_MASK;
    uint32_t scaled = (spread * (light >> SPREAD_LIGHT_DROP)) >> SPREAD_LIGHT_BITS & SPREAD_MASK;
    return (Color)(scaled | scaled >> SPREAD_SHIFT);
}

static unsigned channel_between(unsigned from, unsigned to, int step, int steps)
{
    int difference = (int)to - (int)from;
    return (unsigned)((int)from + difference * step / steps);
}

Color color_between(Color from, Color to, int step, int steps)
{
    if (steps <= 0) {
        return to;
    }
    Channels first = split(from);
    Channels last = split(to);
    return join((Channels){
        .red = channel_between(first.red, last.red, step, steps),
        .green = channel_between(first.green, last.green, step, steps),
        .blue = channel_between(first.blue, last.blue, step, steps),
    });
}

unsigned color_luminance(Color color)
{
    Channels channels = split(color);
    uint32_t weighted = (uint32_t)LUMINANCE_RED_WEIGHT * FIVE_BIT_LIGHT[channels.red] +
                        (uint32_t)LUMINANCE_GREEN_WEIGHT * SIX_BIT_LIGHT[channels.green] +
                        (uint32_t)LUMINANCE_BLUE_WEIGHT * FIVE_BIT_LIGHT[channels.blue];
    return (unsigned)((weighted + LUMINANCE_WEIGHT_TOTAL / 2) / LUMINANCE_WEIGHT_TOTAL);
}
