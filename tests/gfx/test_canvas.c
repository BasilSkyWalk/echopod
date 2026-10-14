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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(clear_paints_every_pixel),
        TEST_CASE(fill_rect_uses_screen_coordinates),
        TEST_CASE(fill_rect_is_clipped_to_the_canvas),
        TEST_CASE(fill_rect_outside_the_canvas_changes_nothing),
        TEST_CASE(pixels_outside_the_canvas_read_as_black),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
