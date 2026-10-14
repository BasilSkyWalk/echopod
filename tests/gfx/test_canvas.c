#include "check.h"
#include "gfx/canvas.h"

enum {
    CANVAS_X = 0,
    CANVAS_Y = 10,
    CANVAS_WIDTH = 4,
    CANVAS_HEIGHT = 3,
    PIXEL_COUNT = CANVAS_WIDTH * CANVAS_HEIGHT,
    PAINT = 0x1234,
    BACKGROUND = 0x0101
};

static Color pixels[PIXEL_COUNT];

static Canvas make_strip_canvas(void)
{
    Canvas canvas = canvas_make(pixels, (Rect){CANVAS_X, CANVAS_Y, CANVAS_WIDTH, CANVAS_HEIGHT});
    for (int index = 0; index < PIXEL_COUNT; index++) {
        pixels[index] = BACKGROUND;
    }
    return canvas;
}

static int count_pixels_of(Color color)
{
    int count = 0;
    for (int index = 0; index < PIXEL_COUNT; index++) {
        count += pixels[index] == color ? 1 : 0;
    }
    return count;
}

static void clear_paints_every_pixel(void)
{
    Canvas canvas = make_strip_canvas();
    canvas_clear(&canvas, PAINT);
    CHECK_EQUAL(PIXEL_COUNT, count_pixels_of(PAINT));
}

static void fill_rect_uses_screen_coordinates(void)
{
    Canvas canvas = make_strip_canvas();
    canvas_fill_rect(&canvas, (Rect){1, 11, 2, 1}, PAINT);
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 1, 11));
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 2, 11));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 0, 11));
    CHECK_EQUAL(2, count_pixels_of(PAINT));
}

static void fill_rect_is_clipped_to_the_canvas(void)
{
    Canvas canvas = make_strip_canvas();
    canvas_fill_rect(&canvas, (Rect){-2, 5, 4, 7}, PAINT);
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 0, 10));
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 1, 11));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 2, 10));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 0, 12));
    CHECK_EQUAL(4, count_pixels_of(PAINT));
}

static void fill_rect_outside_the_canvas_changes_nothing(void)
{
    Canvas canvas = make_strip_canvas();
    canvas_fill_rect(&canvas, (Rect){0, 0, 4, 10}, PAINT);
    canvas_fill_rect(&canvas, (Rect){0, 13, 4, 5}, PAINT);
    CHECK_EQUAL(0, count_pixels_of(PAINT));
}

static void pixels_outside_the_canvas_read_as_black(void)
{
    Canvas canvas = make_strip_canvas();
    canvas_clear(&canvas, PAINT);
    CHECK_EQUAL(0, canvas_pixel_at(&canvas, 0, 9));
    CHECK_EQUAL(0, canvas_pixel_at(&canvas, CANVAS_WIDTH, 10));
}

static void a_clipped_canvas_only_paints_inside_its_clip(void)
{
    Canvas canvas = make_strip_canvas();
    Canvas clipped = canvas_clipped(&canvas, (Rect){1, 10, 2, 2});
    canvas_clear(&clipped, PAINT);
    CHECK_EQUAL(4, count_pixels_of(PAINT));
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 2, 11));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 0, 10));
}

static void clipping_can_only_shrink_the_drawable_area(void)
{
    Canvas canvas = make_strip_canvas();
    Canvas narrow = canvas_clipped(&canvas, (Rect){0, 10, 1, 3});
    Canvas widened = canvas_clipped(&narrow, (Rect){0, 0, 100, 100});
    canvas_clear(&widened, PAINT);
    CHECK_EQUAL(3, count_pixels_of(PAINT));
}

static void blending_a_pixel_mixes_with_what_is_there(void)
{
    Canvas canvas = make_strip_canvas();
    canvas_clear(&canvas, 0x0000);
    canvas_blend_pixel(&canvas, (Point){1, 11}, 0xFFFF, 8);
    CHECK_EQUAL(0x8C51, canvas_pixel_at(&canvas, 1, 11));
    CHECK_EQUAL(0x0000, canvas_pixel_at(&canvas, 0, 11));
}

static void blending_respects_the_clip(void)
{
    Canvas canvas = make_strip_canvas();
    Canvas clipped = canvas_clipped(&canvas, (Rect){0, 10, 1, 1});
    canvas_blend_pixel(&clipped, (Point){2, 11}, PAINT, COLOR_ALPHA_OPAQUE);
    canvas_blend_pixel(&clipped, (Point){0, 10}, PAINT, COLOR_ALPHA_OPAQUE);
    CHECK_EQUAL(1, count_pixels_of(PAINT));
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 0, 10));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(clear_paints_every_pixel),
        TEST_CASE(fill_rect_uses_screen_coordinates),
        TEST_CASE(fill_rect_is_clipped_to_the_canvas),
        TEST_CASE(fill_rect_outside_the_canvas_changes_nothing),
        TEST_CASE(pixels_outside_the_canvas_read_as_black),
        TEST_CASE(a_clipped_canvas_only_paints_inside_its_clip),
        TEST_CASE(clipping_can_only_shrink_the_drawable_area),
        TEST_CASE(blending_a_pixel_mixes_with_what_is_there),
        TEST_CASE(blending_respects_the_clip),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
