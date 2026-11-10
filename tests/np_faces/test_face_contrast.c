#include <math.h>
#include <stdio.h>

#include "check.h"
#include "fonts/fonts.h"
#include "np_faces_test.h"
#include "tiny_font.h"

enum {

    LANDSCAPE = 0,
    PORTRAIT_CARD_HEIGHT = 217,
    LONG_SIDE = 320
};

typedef enum Base {
    ABSOLUTE,
    FROM_AREA,
    FROM_CARD
} Base;

typedef struct Spot {
    const char *name;
    NpTestState state;
    Base base;
    Rect box;
} Spot;

typedef struct Measure {
    double best;
    double worst_of_best_ink;
    int changed;
} Measure;

static const char WORD[] = "MMM";
static const double FLOOR = 4.5;

static Color text_pixels[NP_TEST_PIXELS];
static Color blank_pixels[NP_TEST_PIXELS];

static const Spot CASSETTE_LANDSCAPE[] = {
    {"title", NP_TEST_PLAYING, ABSOLUTE, {33, 33, 253, 20}},
    {"artist", NP_TEST_PLAYING, ABSOLUTE, {33, 56, 253, 20}},
    {"lyric", NP_TEST_LYRIC, ABSOLUTE, {33, 56, 253, 20}},
    {"feedback", NP_TEST_VOLUME, ABSOLUTE, {33, 56, 253, 20}},
    {"elapsed", NP_TEST_PLAYING, ABSOLUTE, {17, 129, 62, 16}},
    {"remaining", NP_TEST_PLAYING, ABSOLUTE, {241, 129, 62, 16}},
    {"codec", NP_TEST_PLAYING, ABSOLUTE, {254, 84, 40, 31}},
    {"letter", NP_TEST_PLAYING, ABSOLUTE, {25, 84, 40, 31}},
};

static const Spot CASSETTE_PORTRAIT[] = {
    {"title", NP_TEST_PLAYING, FROM_CARD, {16, 111, 137, 20}},
    {"artist", NP_TEST_PLAYING, FROM_CARD, {16, 134, 137, 20}},
    {"album", NP_TEST_PLAYING, FROM_CARD, {16, 157, 137, 20}},
    {"lyric", NP_TEST_LYRIC, FROM_CARD, {16, 157, 137, 20}},
    {"stream", NP_TEST_PLAYING, FROM_CARD, {16, 181, 137, 16}},
    {"feedback", NP_TEST_VOLUME, FROM_CARD, {16, 181, 137, 16}},
    {"elapsed", NP_TEST_PLAYING, FROM_CARD, {16, 198, 60, 16}},
    {"remaining", NP_TEST_PLAYING, FROM_CARD, {93, 198, 60, 16}},
    {"codec", NP_TEST_PLAYING, FROM_CARD, {100, 10, 44, 16}},
    {"letter", NP_TEST_PLAYING, FROM_CARD, {25, 6, 14, 20}},
};

static const Spot VU_LANDSCAPE[] = {
    {"title", NP_TEST_PLAYING, ABSOLUTE, {10, 102, 300, 20}},
    {"artist", NP_TEST_PLAYING, ABSOLUTE, {10, 121, 300, 20}},
    {"lyric", NP_TEST_LYRIC, ABSOLUTE, {10, 121, 300, 20}},
    {"feedback", NP_TEST_VOLUME, ABSOLUTE, {10, 121, 300, 20}},
    {"elapsed", NP_TEST_PLAYING, ABSOLUTE, {10, 147, 52, 16}},
    {"remaining", NP_TEST_PLAYING, ABSOLUTE, {262, 147, 48, 16}},
};

static const Spot VU_PORTRAIT[] = {
    {"title", NP_TEST_PLAYING, ABSOLUTE, {10, 174, 150, 22}},
    {"artist", NP_TEST_PLAYING, ABSOLUTE, {10, 196, 150, 22}},
    {"album", NP_TEST_PLAYING, ABSOLUTE, {10, 218, 150, 22}},
    {"lyric", NP_TEST_LYRIC, ABSOLUTE, {10, 218, 150, 22}},
    {"format", NP_TEST_PLAYING, ABSOLUTE, {10, 244, 150, 16}},
    {"feedback", NP_TEST_VOLUME, ABSOLUTE, {10, 244, 150, 16}},
    {"modes", NP_TEST_PLAYING, ABSOLUTE, {10, 278, 150, 16}},
    {"elapsed", NP_TEST_PLAYING, ABSOLUTE, {10, 302, 70, 16}},
    {"remaining", NP_TEST_PLAYING, ABSOLUTE, {90, 302, 70, 16}},
};

static const Spot TURNTABLE_LANDSCAPE[] = {
    {"title", NP_TEST_PLAYING, ABSOLUTE, {170, 28, 140, 22}},
    {"artist", NP_TEST_PLAYING, ABSOLUTE, {170, 50, 140, 22}},
    {"album", NP_TEST_PLAYING, ABSOLUTE, {170, 70, 140, 22}},
    {"lyric", NP_TEST_LYRIC, ABSOLUTE, {170, 70, 140, 22}},
    {"format", NP_TEST_PLAYING, ABSOLUTE, {170, 97, 140, 16}},
    {"detail", NP_TEST_PLAYING, ABSOLUTE, {170, 116, 140, 16}},
    {"modes", NP_TEST_PLAYING, ABSOLUTE, {170, 131, 140, 16}},
    {"feedback", NP_TEST_VOLUME, ABSOLUTE, {170, 131, 140, 16}},
    {"times", NP_TEST_PLAYING, ABSOLUTE, {170, 153, 70, 16}},
    {"remaining", NP_TEST_PLAYING, ABSOLUTE, {240, 153, 70, 16}},
};

static const Spot TURNTABLE_PORTRAIT[] = {
    {"title", NP_TEST_PLAYING, FROM_AREA, {10, 144, 150, 22}},
    {"artist", NP_TEST_PLAYING, FROM_AREA, {10, 166, 150, 22}},
    {"album", NP_TEST_PLAYING, FROM_AREA, {10, 188, 150, 22}},
    {"lyric", NP_TEST_LYRIC, FROM_AREA, {10, 188, 150, 22}},
    {"format", NP_TEST_PLAYING, FROM_AREA, {10, 212, 150, 16}},
    {"modes", NP_TEST_PLAYING, ABSOLUTE, {10, 278, 150, 16}},
    {"feedback", NP_TEST_VOLUME, ABSOLUTE, {10, 278, 150, 16}},
    {"elapsed", NP_TEST_PLAYING, ABSOLUTE, {10, 302, 70, 16}},
    {"remaining", NP_TEST_PLAYING, ABSOLUTE, {90, 302, 70, 16}},
};

static const Spot LCD_LANDSCAPE[] = {
    {"title", NP_TEST_PLAYING, ABSOLUTE, {12, 95, 296, 20}},
    {"artist", NP_TEST_PLAYING, ABSOLUTE, {12, 117, 296, 20}},
    {"lyric", NP_TEST_LYRIC, ABSOLUTE, {12, 117, 296, 20}},
    {"info", NP_TEST_PLAYING, ABSOLUTE, {12, 149, 296, 15}},
    {"feedback", NP_TEST_VOLUME, ABSOLUTE, {12, 149, 296, 15}},
    {"codec", NP_TEST_PLAYING, ABSOLUTE, {250, 31, 58, 15}},
};

static const Spot LCD_PORTRAIT[] = {
    {"title", NP_TEST_PLAYING, FROM_AREA, {10, 174, 150, 20}},
    {"artist", NP_TEST_PLAYING, FROM_AREA, {10, 197, 150, 20}},
    {"album", NP_TEST_PLAYING, FROM_AREA, {10, 217, 150, 20}},
    {"lyric", NP_TEST_LYRIC, FROM_AREA, {10, 217, 150, 20}},
    {"info", NP_TEST_PLAYING, FROM_AREA, {10, 251, 150, 15}},
    {"feedback", NP_TEST_VOLUME, FROM_AREA, {10, 251, 150, 15}},
    {"codec", NP_TEST_PLAYING, FROM_AREA, {120, 24, 40, 15}},
};

#define COUNT(table) (sizeof(table) / sizeof((table)[0]))

static double linear_channel(unsigned value)
{
    double unit = (double)value / 255.0;
    return unit <= 0.04045 ? unit / 12.92 : pow((unit + 0.055) / 1.055, 2.4);
}

static double luminance(Color color)
{
    unsigned red = (unsigned)(color >> 11) & 31u;
    unsigned green = (unsigned)(color >> 5) & 63u;
    unsigned blue = (unsigned)color & 31u;
    return 0.2126 * linear_channel(red << 3 | red >> 2) +
           0.7152 * linear_channel(green << 2 | green >> 4) +
           0.0722 * linear_channel(blue << 3 | blue >> 2);
}

static double contrast(Color first, Color second)
{
    double a = luminance(first);
    double b = luminance(second);
    return (fmax(a, b) + 0.05) / (fmin(a, b) + 0.05);
}

static void set_words(NpTestScene *scene)
{
    NowPlayingFrame *frame = &scene->frame;
    frame->song = WORD;
    frame->artist = WORD;
    frame->album = WORD;
    frame->codec = WORD;
    frame->stream = WORD;
    frame->detail = WORD;
    frame->modes = WORD;
    frame->lyric = frame->lyric != NULL ? WORD : NULL;
}

static void set_fonts(NpTestScene *scene, bool real)
{
    const Font *blank = &TINY_FONT_WITHOUT_FALLBACK;
    scene->theme.fonts.title = real ? &FONT_INTER_SEMIBOLD_16 : blank;
    scene->theme.fonts.body = real ? &FONT_INTER_REGULAR_16 : blank;
    scene->theme.fonts.small = real ? &FONT_INTER_REGULAR_12 : blank;
    scene->theme.fonts.status = scene->theme.fonts.body;
}

static Rect placed(const Spot *spot, NpTestLayout layout)
{
    Rect box = spot->box;
    if (spot->base == FROM_AREA) {
        box.y += layout.area.y;
    } else if (spot->base == FROM_CARD) {
        box.y += layout.area.y + (layout.area.height - PORTRAIT_CARD_HEIGHT) / 2;
    }
    return box;
}

static Measure measure(uint8_t face, NpTestLayout layout, const Spot *spot)
{
    NpTestScene scene = np_test_scene(face, layout, spot->state);
    set_words(&scene);
    set_fonts(&scene, true);
    np_test_draw(&scene, text_pixels);
    set_fonts(&scene, false);
    np_test_draw(&scene, blank_pixels);
    Rect box = placed(spot, layout);
    Measure result = {0.0, 1000.0, 0};
    Color ink = 0;
    for (int y = box.y; y < box.y + box.height; y++) {
        for (int x = box.x; x < box.x + box.width; x++) {
            Color text = np_test_pixel(text_pixels, layout, x, y);
            Color back = np_test_pixel(blank_pixels, layout, x, y);
            double ratio = contrast(text, back);
            result.changed += text != back;
            if (text != back && ratio > result.best) {
                result.best = ratio;
                ink = text;
            }
        }
    }
    for (int y = box.y; y < box.y + box.height; y++) {
        for (int x = box.x; x < box.x + box.width; x++) {
            Color back = np_test_pixel(blank_pixels, layout, x, y);
            if (np_test_pixel(text_pixels, layout, x, y) == ink && back != ink) {
                double ratio = contrast(ink, back);
                result.worst_of_best_ink = fmin(result.worst_of_best_ink, ratio);
            }
        }
    }
    return result;
}

static bool spot_reads(uint8_t face, NpTestLayout layout, const Spot *spot)
{
    Measure result = measure(face, layout, spot);
    bool drawn = result.changed > 0;
    bool readable = result.best >= FLOOR && result.worst_of_best_ink >= FLOOR;
    if (!drawn || !readable) {
        fprintf(stderr, "face %d layout %dx%d %s: changed %d best %.2f worst ink %.2f\n", face,
                layout.width, layout.height, spot->name, result.changed, result.best,
                result.worst_of_best_ink);
    }
    return drawn && readable;
}

static bool face_reads(uint8_t face, const Spot *landscape, size_t landscape_count,
                       const Spot *portrait, size_t portrait_count)
{
    bool all = true;
    for (size_t index = 0; index < landscape_count; index++) {
        all = spot_reads(face, NP_TEST_LAYOUTS[LANDSCAPE], landscape + index) && all;
    }
    for (int layout = 1; layout < NP_TEST_LAYOUT_COUNT; layout++) {
        for (size_t index = 0; index < portrait_count; index++) {
            all = spot_reads(face, NP_TEST_LAYOUTS[layout], portrait + index) && all;
        }
    }
    return all;
}

static void cassette_text_reads_at_least_four_and_a_half_to_one(void)
{
    CHECK(face_reads(NP_FACE_CASSETTE, CASSETTE_LANDSCAPE, COUNT(CASSETTE_LANDSCAPE),
                     CASSETTE_PORTRAIT, COUNT(CASSETTE_PORTRAIT)));
}

static void vu_text_reads_at_least_four_and_a_half_to_one(void)
{
    CHECK(face_reads(NP_FACE_VU_METERS, VU_LANDSCAPE, COUNT(VU_LANDSCAPE), VU_PORTRAIT,
                     COUNT(VU_PORTRAIT)));
}

static void turntable_text_reads_at_least_four_and_a_half_to_one(void)
{
    CHECK(face_reads(NP_FACE_TURNTABLE, TURNTABLE_LANDSCAPE, COUNT(TURNTABLE_LANDSCAPE),
                     TURNTABLE_PORTRAIT, COUNT(TURNTABLE_PORTRAIT)));
}

static void lcd_text_reads_at_least_four_and_a_half_to_one(void)
{
    CHECK(face_reads(NP_FACE_LCD, LCD_LANDSCAPE, COUNT(LCD_LANDSCAPE), LCD_PORTRAIT,
                     COUNT(LCD_PORTRAIT)));
}

static void the_contrast_measure_follows_wcag(void)
{
    CHECK(contrast(0x0000, 0xFFFF) > 20.9 && contrast(0x0000, 0xFFFF) < 21.1);
    CHECK(contrast(0x7BEF, 0x7BEF) > 0.99 && contrast(0x7BEF, 0x7BEF) < 1.01);
    CHECK(contrast(0xFFFF, 0x0000) == contrast(0x0000, 0xFFFF));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_contrast_measure_follows_wcag),
        TEST_CASE(cassette_text_reads_at_least_four_and_a_half_to_one),
        TEST_CASE(vu_text_reads_at_least_four_and_a_half_to_one),
        TEST_CASE(turntable_text_reads_at_least_four_and_a_half_to_one),
        TEST_CASE(lcd_text_reads_at_least_four_and_a_half_to_one),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
