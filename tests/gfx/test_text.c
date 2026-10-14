#include "check.h"
#include "gfx/text.h"
#include "tiny_font.h"

enum {
    WIDTH = 10,
    HEIGHT = 4,
    BACKGROUND = 0x0000,
    INK = 0xFFFF
};

static Color pixels[WIDTH * HEIGHT];

static Canvas blank_canvas(void)
{
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, WIDTH, HEIGHT});
    canvas_clear(&canvas, BACKGROUND);
    return canvas;
}

static const TextStyle INK_STYLE = {.font = &TINY_FONT, .color = INK};

static void width_is_the_sum_of_advances(void)
{
    CHECK_EQUAL(7, text_width(&TINY_FONT, "AB"));
    CHECK_EQUAL(0, text_width(&TINY_FONT, ""));
}

static void unknown_characters_measure_as_the_fallback(void)
{
    CHECK_EQUAL(6, text_width(&TINY_FONT, "AZ"));
    CHECK_EQUAL(2, text_width(&TINY_FONT, "\xC3\xA9"));
}

static void glyphs_sit_on_the_baseline_after_their_bearing(void)
{
    Canvas canvas = blank_canvas();
    text_draw(&canvas, INK_STYLE, (Point){0, 2}, "A");
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 1, 0));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 2, 0));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 1, 1));
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 2, 1));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 0, 0));
}

static void partial_coverage_blends_with_the_background(void)
{
    Canvas canvas = blank_canvas();
    text_draw(&canvas, INK_STYLE, (Point){0, 1}, "B");
    CHECK_EQUAL(color_blend(BACKGROUND, INK, 4), canvas_pixel_at(&canvas, 0, 0));
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 1, 0));
}

static void drawing_returns_the_advanced_pen(void)
{
    Canvas canvas = blank_canvas();
    CHECK_EQUAL(9, text_draw(&canvas, INK_STYLE, (Point){2, 2}, "AB"));
}

static void drawing_respects_the_clip(void)
{
    Canvas canvas = blank_canvas();
    Canvas clipped = canvas_clipped(&canvas, (Rect){2, 0, 8, 4});
    text_draw(&clipped, INK_STYLE, (Point){0, 2}, "A");
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 1, 0));
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 2, 1));
}

static void text_outside_the_canvas_changes_nothing(void)
{
    Canvas canvas = blank_canvas();
    text_draw(&canvas, INK_STYLE, (Point){0, 40}, "AB");
    text_draw(&canvas, INK_STYLE, (Point){-20, 2}, "AB");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, x, y));
        }
    }
}

static void a_span_draws_only_its_bytes(void)
{
    Canvas canvas = blank_canvas();
    TextSpan first_letter = {.start = "AB", .byte_count = 1};
    CHECK_EQUAL(4, text_draw_span(&canvas, INK_STYLE, (Point){0, 2}, first_letter));
    CHECK_EQUAL(INK, canvas_pixel_at(&canvas, 1, 0));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 4, 1));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(width_is_the_sum_of_advances),
        TEST_CASE(unknown_characters_measure_as_the_fallback),
        TEST_CASE(glyphs_sit_on_the_baseline_after_their_bearing),
        TEST_CASE(partial_coverage_blends_with_the_background),
        TEST_CASE(drawing_returns_the_advanced_pen),
        TEST_CASE(drawing_respects_the_clip),
        TEST_CASE(text_outside_the_canvas_changes_nothing),
        TEST_CASE(a_span_draws_only_its_bytes),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
