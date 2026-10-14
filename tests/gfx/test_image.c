#include "check.h"
#include "gfx/canvas.h"
#include "gfx/image.h"

enum {
    WIDTH = 16,
    HEIGHT = 10,
    BACKGROUND = 0x0101,
    PAINT = 0x7777
};

static Color pixels[WIDTH * HEIGHT];
static const Color CHECKER[4] = {0x1000, 0x2000, 0x3000, 0x4000};
static const Image TWO_BY_TWO = {.pixels = CHECKER, .width = 2, .height = 2};

static Canvas make_canvas(Rect bounds)
{
    Canvas canvas = canvas_make(pixels, bounds);
    canvas_clear(&canvas, BACKGROUND);
    return canvas;
}

static Quad flat(int x, int y, int width, int height)
{
    return (Quad){.left = x,
                  .width = width,
                  .left_top = y,
                  .left_height = height,
                  .right_top = y,
                  .right_height = height};
}

static int column_height(const Canvas *canvas, int x)
{
    int count = 0;
    for (int y = canvas->bounds.y; y < canvas->bounds.y + canvas->bounds.height; y++) {
        count += canvas_pixel_at(canvas, x, y) != BACKGROUND ? 1 : 0;
    }
    return count;
}

static void an_image_at_its_own_size_is_copied(void)
{
    Canvas canvas = make_canvas((Rect){0, 0, WIDTH, HEIGHT});
    image_draw_quad(&canvas, TWO_BY_TWO, flat(3, 4, 2, 2));
    CHECK_EQUAL(0x1000, canvas_pixel_at(&canvas, 3, 4));
    CHECK_EQUAL(0x2000, canvas_pixel_at(&canvas, 4, 4));
    CHECK_EQUAL(0x3000, canvas_pixel_at(&canvas, 3, 5));
    CHECK_EQUAL(0x4000, canvas_pixel_at(&canvas, 4, 5));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 5, 4));
}

static void a_larger_quad_scales_the_image_up(void)
{
    Canvas canvas = make_canvas((Rect){0, 0, WIDTH, HEIGHT});
    image_draw_quad(&canvas, TWO_BY_TWO, flat(0, 0, 4, 4));
    CHECK_EQUAL(0x1000, canvas_pixel_at(&canvas, 1, 1));
    CHECK_EQUAL(0x2000, canvas_pixel_at(&canvas, 2, 1));
    CHECK_EQUAL(0x3000, canvas_pixel_at(&canvas, 1, 2));
    CHECK_EQUAL(0x4000, canvas_pixel_at(&canvas, 3, 3));
}

static void a_slanted_quad_shrinks_toward_its_far_edge(void)
{
    Canvas canvas = make_canvas((Rect){0, 0, WIDTH, HEIGHT});
    Quad quad = {
        .left = 2, .width = 8, .left_top = 1, .left_height = 8, .right_top = 3, .right_height = 4};
    image_draw_quad(&canvas, TWO_BY_TWO, quad);
    CHECK_EQUAL(8, column_height(&canvas, 2));
    CHECK(column_height(&canvas, 9) < column_height(&canvas, 5));
    CHECK(column_height(&canvas, 5) < 8);
    CHECK_EQUAL(0, column_height(&canvas, 10));
    CHECK_EQUAL(0x1000, canvas_pixel_at(&canvas, 2, 1));
}

static void drawing_stays_inside_the_clip(void)
{
    Canvas canvas = make_canvas((Rect){0, 4, WIDTH, 3});
    image_draw_quad(&canvas, TWO_BY_TWO, flat(0, 0, 10, 10));
    CHECK_EQUAL(0x1000, canvas_pixel_at(&canvas, 0, 4));
    CHECK_EQUAL(0x3000, canvas_pixel_at(&canvas, 0, 6));
}

static void a_quad_can_be_filled_with_a_color(void)
{
    Canvas canvas = make_canvas((Rect){0, 0, WIDTH, HEIGHT});
    Quad quad = {
        .left = 0, .width = 4, .left_top = 0, .left_height = 6, .right_top = 2, .right_height = 2};
    image_fill_quad(&canvas, quad, PAINT);
    CHECK_EQUAL(6, column_height(&canvas, 0));
    CHECK_EQUAL(PAINT, canvas_pixel_at(&canvas, 0, 5));
    CHECK_EQUAL(BACKGROUND, canvas_pixel_at(&canvas, 3, 0));
}

static void an_empty_quad_draws_nothing(void)
{
    Canvas canvas = make_canvas((Rect){0, 0, WIDTH, HEIGHT});
    image_draw_quad(&canvas, TWO_BY_TWO, flat(2, 2, 0, 4));
    image_draw_quad(&canvas, TWO_BY_TWO, flat(2, 2, 4, 0));
    image_fill_quad(&canvas, flat(-8, 2, 4, 4), PAINT);
    for (int x = 0; x < WIDTH; x++) {
        CHECK_EQUAL(0, column_height(&canvas, x));
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(an_image_at_its_own_size_is_copied),
        TEST_CASE(a_larger_quad_scales_the_image_up),
        TEST_CASE(a_slanted_quad_shrinks_toward_its_far_edge),
        TEST_CASE(drawing_stays_inside_the_clip),
        TEST_CASE(a_quad_can_be_filled_with_a_color),
        TEST_CASE(an_empty_quad_draws_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
