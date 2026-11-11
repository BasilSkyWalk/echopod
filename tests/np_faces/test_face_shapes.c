#include <string.h>

#include "check.h"
#include "np_faces/face_shapes.h"

enum {
    SIDE = 48,
    STRIP = 7,
    BACKGROUND = 0x001F,
    FOREGROUND = 0xF800,
    LIT = 0xFFE0,
    UNLIT = 0x2104,
    UPRIGHT = 255
};

typedef void (*Painter)(Canvas *canvas, int variant);

typedef struct Golden {
    const char *name;
    Painter paint;
    int variant;
    uint32_t hash;
} Golden;

static Color grid[SIDE * SIDE];
static Color strips[SIDE * SIDE];
static Color other[SIDE * SIDE];

static Canvas fresh(Color *pixels)
{
    for (int index = 0; index < SIDE * SIDE; index++) {
        pixels[index] = BACKGROUND;
    }
    return canvas_make(pixels, (Rect){0, 0, SIDE, SIDE});
}

static Color at(const Color *pixels, int x, int y)
{
    return pixels[y * SIDE + x];
}

static uint32_t hash_of(const Color *pixels)
{
    uint32_t hash = 0;
    for (int index = 0; index < SIDE * SIDE; index++) {
        hash = hash * 31u + pixels[index];
    }
    return hash;
}

static int count_of(const Color *pixels, Color color)
{
    int count = 0;
    for (int index = 0; index < SIDE * SIDE; index++) {
        count += pixels[index] == color ? 1 : 0;
    }
    return count;
}

static void paint_ring(Canvas *canvas, int variant)
{
    static const ShapeRing RINGS[] = {{392, 330, 150, 0}, {392, 330, 180, 80}, {397, 331, 137, 51}};
    shape_ring(canvas, RINGS[variant], FOREGROUND);
}

static void paint_rounded(Canvas *canvas, int variant)
{
    shape_rounded(canvas, variant == 0 ? (Rect){4, 3, 38, 30} : (Rect){4, 3, 20, 12},
                  6 * (1 - variant), FOREGROUND);
}

static void paint_trapezoid(Canvas *canvas, int variant)
{
    (void)variant;
    shape_trapezoid(canvas, (Rect){3, 4, 40, 20}, 6, FOREGROUND);
}

static void paint_spoke(Canvas *canvas, int angle)
{
    ShapeSpoke spoke = {{24, 24}, (uint8_t)angle, 24, -10, 30};
    shape_spoke(canvas, &spoke, FOREGROUND);
}

static void paint_wide_spoke(Canvas *canvas, int variant)
{
    (void)variant;
    ShapeSpoke spoke = {{24, 24}, 90, 64, 0, 20};
    shape_spoke(canvas, &spoke, FOREGROUND);
}

static void paint_digit(Canvas *canvas, int mask)
{
    SegmentStyle style = {22, 40, 5, 8, LIT, UNLIT};
    shape_digit(canvas, &style, (Point){2, 2}, (uint8_t)mask);
}

static void paint_small_digit(Canvas *canvas, int variant)
{
    (void)variant;
    SegmentStyle style = {13, 24, 3, 8, LIT, UNLIT};
    shape_digit(canvas, &style, (Point){5, 5}, 0x6D);
}

static void paint_colon(Canvas *canvas, int lit)
{
    SegmentStyle style = {22, 40, 5, 8, LIT, UNLIT};
    shape_colon(canvas, &style, (Point){2, 2}, lit != 0);
}

static void paint_lattice(Canvas *canvas, int variant)
{
    (void)variant;
    shape_lattice(canvas, (Rect){4, 6, 20, 12}, 3, FOREGROUND);
}

static void paint_spans(Canvas *canvas, int variant)
{
    (void)variant;
    shape_span(canvas, (ShapeSpan){1, 8, 56}, FOREGROUND);
    shape_span(canvas, (ShapeSpan){2, 8, 60}, FOREGROUND);
    shape_span(canvas, (ShapeSpan){3, 10, 15}, FOREGROUND);
    shape_span(canvas, (ShapeSpan){4, 17, 31}, FOREGROUND);
    shape_span(canvas, (ShapeSpan){5, -20, 24}, FOREGROUND);
}

static const Golden GOLDEN[] = {
    {"ring_disc", paint_ring, 0, 0xCFF0CB41},
    {"ring_hole", paint_ring, 1, 0x9D58363B},
    {"ring_subpixel", paint_ring, 2, 0x02390295},
    {"rounded", paint_rounded, 0, 0x749C5240},
    {"rounded_flat", paint_rounded, 1, 0x2968BF00},
    {"trapezoid", paint_trapezoid, 0, 0xE0842E80},
    {"spoke_0", paint_spoke, 0, 0xC328EAB9},
    {"spoke_37", paint_spoke, 37, 0x97909601},
    {"spoke_64", paint_spoke, 64, 0xCFB0C62F},
    {"spoke_128", paint_spoke, 128, 0xF59E5746},
    {"spoke_192", paint_spoke, 192, 0xD3D9336E},
    {"spoke_200", paint_spoke, 200, 0x0C2EBA08},
    {"spoke_wide", paint_wide_spoke, 0, 0x3BC69440},
    {"digit_8", paint_digit, 0x7F, 0x031AE6E0},
    {"digit_2", paint_digit, 0x5B, 0xB154C404},
    {"digit_0", paint_digit, 0x3F, 0x922E0BE0},
    {"digit_small", paint_small_digit, 0, 0xC8EAEB1D},
    {"colon_lit", paint_colon, 1, 0x54C1AE00},
    {"colon_unlit", paint_colon, 0, 0xD1BE6600},
    {"spans", paint_spans, 0, 0xBA3E9555},
};

enum {
    GOLDEN_COUNT = sizeof GOLDEN / sizeof GOLDEN[0]
};

static const Painter PAINTERS[] = {paint_ring,  paint_rounded,     paint_trapezoid, paint_spoke,
                                   paint_digit, paint_small_digit, paint_colon,     paint_lattice,
                                   paint_spans, paint_wide_spoke};

static void the_sine_table_is_a_signed_quarter_wave(void)
{
    CHECK_EQUAL(0, shape_sine(0));
    CHECK_EQUAL(0, shape_sine(128));
    CHECK_EQUAL(255, shape_sine(64));
    CHECK_EQUAL(-255, shape_sine(192));
    for (int angle = 1; angle < 256; angle++) {
        CHECK_EQUAL(-shape_sine((uint8_t)angle), shape_sine((uint8_t)(256 - angle)));
    }
    for (int angle = 1; angle <= 64; angle++) {
        CHECK(shape_sine((uint8_t)angle) >= shape_sine((uint8_t)(angle - 1)));
    }
    CHECK(shape_sine(32) > 0 && shape_sine(32) < 255);
}

static void the_sine_table_matches_the_reference(void)
{
    uint32_t sum = 0;
    for (int angle = 0; angle < 256; angle++) {
        sum += (uint32_t)(shape_sine((uint8_t)angle) + UPRIGHT) * (uint32_t)(angle + 1);
    }
    CHECK_EQUAL(0x576800, sum);
}

static void the_cosine_is_the_sine_a_quarter_turn_ahead(void)
{
    for (int angle = 0; angle < 256; angle++) {
        CHECK_EQUAL(shape_sine((uint8_t)(angle + 64)), shape_cosine((uint8_t)angle));
    }
    CHECK_EQUAL(255, shape_cosine(0));
    CHECK_EQUAL(-255, shape_cosine(128));
}

static void the_integer_square_root_is_exact_and_floors(void)
{
    CHECK_EQUAL(0, shape_isqrt(0));
    CHECK_EQUAL(1, shape_isqrt(1));
    CHECK_EQUAL(1, shape_isqrt(2));
    CHECK_EQUAL(1, shape_isqrt(3));
    CHECK_EQUAL(2, shape_isqrt(4));
    CHECK_EQUAL(2, shape_isqrt(8));
    CHECK_EQUAL(3, shape_isqrt(15));
    CHECK_EQUAL(4, shape_isqrt(16));
    CHECK_EQUAL(32768, shape_isqrt(1u << 30));
    CHECK_EQUAL(32767, shape_isqrt((1u << 30) - 1));
    CHECK_EQUAL(65535, shape_isqrt(0xFFFFFFFFu));
}

static void polar_points_up_at_zero_and_right_at_a_quarter(void)
{
    Point up = shape_polar((Point){20, 30}, 0, 10);
    CHECK_EQUAL(20, up.x);
    CHECK_EQUAL(20, up.y);
    Point right = shape_polar((Point){20, 30}, 64, 10);
    CHECK_EQUAL(30, right.x);
    CHECK_EQUAL(30, right.y);
    Point down = shape_polar((Point){20, 30}, 128, 10);
    CHECK_EQUAL(20, down.x);
    CHECK_EQUAL(40, down.y);
}

static void polar_rounds_instead_of_truncating(void)
{
    Point point = shape_polar((Point){0, 0}, 42, 10);
    CHECK_EQUAL(9, point.x);
    CHECK_EQUAL(-5, point.y);
    Point behind = shape_polar((Point){0, 0}, 214, 10);
    CHECK_EQUAL(-9, behind.x);
    CHECK_EQUAL(-5, behind.y);
}

static void a_span_fills_its_interior_and_blends_its_edges(void)
{
    Canvas canvas = fresh(grid);
    paint_spans(&canvas, 0);
    CHECK_EQUAL(FOREGROUND, at(grid, 1, 1));
    CHECK_EQUAL(FOREGROUND, at(grid, 2, 1));
    CHECK_EQUAL(0x7011, at(grid, 0, 1));
    CHECK_EQUAL(0x7011, at(grid, 3, 1));
    CHECK_EQUAL(BACKGROUND, at(grid, 4, 1));
    CHECK_EQUAL(0xB808, at(grid, 3, 2));
}

static void a_quarter_and_three_quarter_edge_use_fifteen_sixteenths_of_the_coverage(void)
{
    Canvas canvas = fresh(grid);
    shape_span(&canvas, (ShapeSpan){4, 4, 48}, FOREGROUND);
    shape_span(&canvas, (ShapeSpan){6, 12, 48}, FOREGROUND);
    CHECK_EQUAL(color_blend(BACKGROUND, FOREGROUND, 11), at(grid, 0, 4));
    CHECK_EQUAL(color_blend(BACKGROUND, FOREGROUND, 3), at(grid, 0, 6));
    CHECK_EQUAL(FOREGROUND, at(grid, 1, 4));
}

static void a_span_inside_one_pixel_blends_only_that_pixel(void)
{
    Canvas canvas = fresh(grid);
    shape_span(&canvas, (ShapeSpan){3, 10, 15}, FOREGROUND);
    CHECK_EQUAL(0x4017, at(grid, 0, 3));
    CHECK_EQUAL(1, SIDE * SIDE - count_of(grid, BACKGROUND));
}

static void a_span_leaving_the_canvas_keeps_its_visible_part(void)
{
    Canvas canvas = fresh(grid);
    shape_span(&canvas, (ShapeSpan){5, -20, 24}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 0, 5));
    CHECK_EQUAL(0x7011, at(grid, 1, 5));
}

static void an_empty_or_reversed_span_writes_nothing(void)
{
    Canvas canvas = fresh(grid);
    shape_span(&canvas, (ShapeSpan){3, 40, 40}, FOREGROUND);
    shape_span(&canvas, (ShapeSpan){3, 80, 40}, FOREGROUND);
    CHECK_EQUAL(SIDE * SIDE, count_of(grid, BACKGROUND));
}

static void a_span_writes_nothing_outside_the_clip_rows(void)
{
    Canvas canvas = fresh(grid);
    canvas.clip = (Rect){0, 10, SIDE, 5};
    shape_span(&canvas, (ShapeSpan){9, 0, 400}, FOREGROUND);
    shape_span(&canvas, (ShapeSpan){15, 0, 400}, FOREGROUND);
    CHECK_EQUAL(SIDE * SIDE, count_of(grid, BACKGROUND));
    shape_span(&canvas, (ShapeSpan){10, 0, 400}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 5, 10));
    CHECK_EQUAL(25, SIDE * SIDE - count_of(grid, BACKGROUND));
}

static void a_ring_fills_its_centre_and_blends_its_edge(void)
{
    Canvas canvas = fresh(grid);
    shape_ring(&canvas, (ShapeRing){392, 392, 160, 0}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 24, 24));
    CHECK_EQUAL(BACKGROUND, at(grid, 13, 24));
    CHECK_EQUAL(color_blend(BACKGROUND, FOREGROUND, 7), at(grid, 14, 24));
    CHECK_EQUAL(color_blend(BACKGROUND, FOREGROUND, 7), at(grid, 34, 24));
    CHECK_EQUAL(BACKGROUND, at(grid, 35, 24));
}

static void a_ring_on_a_pixel_centre_is_mirror_symmetric(void)
{
    Canvas canvas = fresh(grid);
    shape_ring(&canvas, (ShapeRing){392, 392, 160, 48}, FOREGROUND);
    for (int distance = 1; distance <= 12; distance++) {
        CHECK_EQUAL(at(grid, 24 + distance, 24), at(grid, 24 - distance, 24));
        CHECK_EQUAL(at(grid, 24, 24 + distance), at(grid, 24, 24 - distance));
        CHECK_EQUAL(at(grid, 24 + distance, 20), at(grid, 24 - distance, 20));
    }
}

static void a_ring_leaves_its_hole_untouched(void)
{
    Canvas canvas = fresh(grid);
    shape_ring(&canvas, (ShapeRing){392, 392, 160, 64}, FOREGROUND);
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 24));
    CHECK_EQUAL(BACKGROUND, at(grid, 22, 24));
    CHECK_EQUAL(FOREGROUND, at(grid, 30, 24));
    Canvas disc = fresh(other);
    shape_ring(&disc, (ShapeRing){392, 392, 160, 0}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(other, 24, 24));
}

static void a_sub_pixel_centre_moves_the_blend(void)
{
    Canvas first = fresh(grid);
    shape_ring(&first, (ShapeRing){392, 392, 160, 0}, FOREGROUND);
    Canvas moved = fresh(other);
    shape_ring(&moved, (ShapeRing){396, 392, 160, 0}, FOREGROUND);
    CHECK(at(grid, 14, 24) != at(other, 14, 24));
    CHECK_EQUAL(color_blend(BACKGROUND, FOREGROUND, 3), at(other, 14, 24));
}

static void a_rounded_rectangle_cuts_its_corners_and_keeps_its_edges(void)
{
    Canvas canvas = fresh(grid);
    paint_rounded(&canvas, 0);
    CHECK_EQUAL(BACKGROUND, at(grid, 4, 3));
    CHECK_EQUAL(BACKGROUND, at(grid, 4, 4));
    CHECK_EQUAL(FOREGROUND, at(grid, 23, 3));
    CHECK_EQUAL(FOREGROUND, at(grid, 4, 16));
    CHECK_EQUAL(FOREGROUND, at(grid, 41, 16));
    CHECK_EQUAL(FOREGROUND, at(grid, 23, 32));
}

static void a_zero_radius_is_a_plain_rectangle(void)
{
    Canvas canvas = fresh(grid);
    paint_rounded(&canvas, 1);
    CHECK_EQUAL(FOREGROUND, at(grid, 4, 3));
    CHECK_EQUAL(20 * 12, count_of(grid, FOREGROUND));
}

static void a_trapezoid_is_inset_at_the_top_and_full_at_the_bottom(void)
{
    Canvas canvas = fresh(grid);
    paint_trapezoid(&canvas, 0);
    CHECK_EQUAL(BACKGROUND, at(grid, 4, 4));
    CHECK_EQUAL(BACKGROUND, at(grid, 7, 4));
    CHECK_EQUAL(FOREGROUND, at(grid, 10, 4));
    CHECK_EQUAL(FOREGROUND, at(grid, 4, 23));
    CHECK(at(grid, 3, 23) != BACKGROUND);
}

static void a_spoke_at_angle_zero_points_up(void)
{
    Canvas canvas = fresh(grid);
    shape_spoke(&canvas, &(ShapeSpoke){{24, 24}, 0, 48, 0, 20}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 24, 10));
    CHECK_EQUAL(FOREGROUND, at(grid, 24, 4));
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 3));
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 25));
}

static void a_spoke_keeps_to_its_width(void)
{
    Canvas canvas = fresh(grid);
    shape_spoke(&canvas, &(ShapeSpoke){{24, 24}, 0, 48, 0, 20}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 23, 10));
    CHECK_EQUAL(FOREGROUND, at(grid, 25, 10));
    CHECK_EQUAL(BACKGROUND, at(grid, 22, 10));
    CHECK_EQUAL(BACKGROUND, at(grid, 26, 10));
}

static void spokes_at_a_quarter_and_three_quarters_point_right_and_left(void)
{
    Canvas right = fresh(grid);
    shape_spoke(&right, &(ShapeSpoke){{24, 24}, 64, 48, 0, 20}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 34, 24));
    CHECK_EQUAL(BACKGROUND, at(grid, 14, 24));
    CHECK_EQUAL(BACKGROUND, at(grid, 34, 22));
    Canvas left = fresh(other);
    shape_spoke(&left, &(ShapeSpoke){{24, 24}, 192, 48, 0, 20}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(other, 14, 24));
    CHECK_EQUAL(BACKGROUND, at(other, 34, 24));
}

static void a_negative_start_draws_behind_the_pivot(void)
{
    Canvas canvas = fresh(grid);
    shape_spoke(&canvas, &(ShapeSpoke){{24, 24}, 0, 48, -10, 0}, FOREGROUND);
    CHECK_EQUAL(FOREGROUND, at(grid, 24, 30));
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 20));
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 35));
}

static void a_vertical_spoke_only_draws_the_rows_inside_its_range(void)
{
    Canvas canvas = fresh(grid);
    shape_spoke(&canvas, &(ShapeSpoke){{24, 24}, 0, 48, 0, 10}, FOREGROUND);
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 13));
    CHECK_EQUAL(FOREGROUND, at(grid, 24, 15));
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 25));
}

static void the_digit_masks_follow_the_seven_segment_table(void)
{
    static const uint8_t EXPECTED[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    for (unsigned digit = 0; digit < sizeof EXPECTED; digit++) {
        CHECK_EQUAL(EXPECTED[digit], shape_digit_mask(digit));
    }
    CHECK_EQUAL(0, shape_digit_mask(10));
}

static void an_eight_lights_every_segment(void)
{
    Canvas canvas = fresh(grid);
    SegmentStyle style = {22, 40, 5, UPRIGHT, LIT, UNLIT};
    shape_digit(&canvas, &style, (Point){2, 2}, shape_digit_mask(8));
    CHECK_EQUAL(0, count_of(grid, UNLIT));
    CHECK(count_of(grid, LIT) > 0);
}

static void a_one_lights_only_the_right_hand_segments(void)
{
    Canvas canvas = fresh(grid);
    SegmentStyle style = {22, 40, 5, UPRIGHT, LIT, UNLIT};
    shape_digit(&canvas, &style, (Point){2, 2}, shape_digit_mask(1));
    CHECK_EQUAL(LIT, at(grid, 21, 12));
    CHECK_EQUAL(LIT, at(grid, 21, 30));
    CHECK_EQUAL(UNLIT, at(grid, 4, 12));
    CHECK_EQUAL(UNLIT, at(grid, 12, 3));
    CHECK_EQUAL(UNLIT, at(grid, 12, 21));
}

static void an_empty_mask_draws_everything_unlit(void)
{
    Canvas canvas = fresh(grid);
    SegmentStyle style = {22, 40, 5, UPRIGHT, LIT, UNLIT};
    shape_digit(&canvas, &style, (Point){2, 2}, 0);
    CHECK_EQUAL(0, count_of(grid, LIT));
    CHECK(count_of(grid, UNLIT) > 0);
}

static void a_zero_has_no_middle_bar(void)
{
    Canvas eight = fresh(grid);
    Canvas zero = fresh(other);
    SegmentStyle style = {22, 40, 5, UPRIGHT, LIT, UNLIT};
    shape_digit(&eight, &style, (Point){2, 2}, shape_digit_mask(8));
    shape_digit(&zero, &style, (Point){2, 2}, shape_digit_mask(0));
    CHECK_EQUAL(LIT, at(grid, 12, 21));
    CHECK_EQUAL(UNLIT, at(other, 12, 21));
    CHECK(count_of(other, LIT) < count_of(grid, LIT));
}

static int first_lit_column(const Color *pixels, int row)
{
    for (int x = 0; x < SIDE; x++) {
        if (at(pixels, x, row) == LIT) {
            return x;
        }
    }
    return -1;
}

static void lean_shifts_the_upper_rows_to_the_right(void)
{
    Canvas leaning = fresh(grid);
    Canvas upright = fresh(other);
    SegmentStyle slanted = {22, 40, 5, 8, LIT, UNLIT};
    SegmentStyle straight = {22, 40, 5, UPRIGHT, LIT, UNLIT};
    shape_digit(&leaning, &slanted, (Point){2, 2}, shape_digit_mask(8));
    shape_digit(&upright, &straight, (Point){2, 2}, shape_digit_mask(8));
    CHECK_EQUAL(7, first_lit_column(other, 2));
    CHECK_EQUAL(11, first_lit_column(grid, 2));
    CHECK_EQUAL(first_lit_column(other, 41), first_lit_column(grid, 41));
}

static void a_lit_colon_draws_two_dots_and_an_unlit_one_the_same_dots_dimmer(void)
{
    Canvas lit = fresh(grid);
    Canvas unlit = fresh(other);
    SegmentStyle style = {22, 40, 5, UPRIGHT, LIT, UNLIT};
    shape_colon(&lit, &style, (Point){2, 2}, true);
    shape_colon(&unlit, &style, (Point){2, 2}, false);
    CHECK_EQUAL(LIT, at(grid, 5, 11));
    CHECK_EQUAL(LIT, at(grid, 5, 30));
    CHECK_EQUAL(BACKGROUND, at(grid, 5, 20));
    CHECK_EQUAL(BACKGROUND, at(grid, 3, 11));
    CHECK_EQUAL(BACKGROUND, at(grid, 8, 11));
    CHECK_EQUAL(UNLIT, at(other, 5, 11));
    CHECK_EQUAL(count_of(grid, LIT), count_of(other, UNLIT));
}

static void the_lattice_leaves_gaps_at_the_pitch(void)
{
    Canvas canvas = fresh(grid);
    paint_lattice(&canvas, 0);
    CHECK_EQUAL(FOREGROUND, at(grid, 6, 7));
    CHECK_EQUAL(FOREGROUND, at(grid, 9, 7));
    CHECK_EQUAL(BACKGROUND, at(grid, 7, 7));
    CHECK_EQUAL(FOREGROUND, at(grid, 7, 8));
    CHECK_EQUAL(FOREGROUND, at(grid, 7, 11));
    CHECK_EQUAL(BACKGROUND, at(grid, 7, 10));
    CHECK_EQUAL(BACKGROUND, at(grid, 5, 7));
}

static void the_lattice_stays_inside_its_area_and_ignores_a_zero_pitch(void)
{
    Canvas canvas = fresh(grid);
    paint_lattice(&canvas, 0);
    CHECK_EQUAL(BACKGROUND, at(grid, 6, 5));
    CHECK_EQUAL(BACKGROUND, at(grid, 6, 18));
    CHECK_EQUAL(BACKGROUND, at(grid, 24, 8));
    Canvas none = fresh(other);
    shape_lattice(&none, (Rect){4, 6, 20, 12}, 0, FOREGROUND);
    CHECK_EQUAL(SIDE * SIDE, count_of(other, BACKGROUND));
}

static void every_primitive_matches_the_reference_pixels(void)
{
    for (int index = 0; index < GOLDEN_COUNT; index++) {
        Canvas canvas = fresh(grid);
        GOLDEN[index].paint(&canvas, GOLDEN[index].variant);
        CHECK_EQUAL(GOLDEN[index].hash, hash_of(grid));
    }
}

static void paint_in_strips(Painter paint, int variant)
{
    for (int top = 0; top < SIDE; top += STRIP) {
        int rows = SIDE - top < STRIP ? SIDE - top : STRIP;
        Color strip[SIDE * STRIP];
        for (int index = 0; index < SIDE * rows; index++) {
            strip[index] = BACKGROUND;
        }
        Canvas canvas = canvas_make(strip, (Rect){0, top, SIDE, rows});
        paint(&canvas, variant);
        memcpy(strips + top * SIDE, strip, (size_t)(SIDE * rows) * sizeof strip[0]);
    }
}

static void strip_canvases_match_a_full_canvas_for_every_primitive(void)
{
    for (int index = 0; index < GOLDEN_COUNT; index++) {
        Canvas canvas = fresh(grid);
        GOLDEN[index].paint(&canvas, GOLDEN[index].variant);
        paint_in_strips(GOLDEN[index].paint, GOLDEN[index].variant);
        CHECK(memcmp(grid, strips, sizeof grid) == 0);
    }
    Canvas canvas = fresh(grid);
    paint_lattice(&canvas, 0);
    paint_in_strips(paint_lattice, 0);
    CHECK(memcmp(grid, strips, sizeof grid) == 0);
}

static bool row_has_ink(const Color *pixels, int row)
{
    for (int x = 0; x < SIDE; x++) {
        if (at(pixels, x, row) != BACKGROUND) {
            return true;
        }
    }
    return false;
}

static void a_shape_wholly_outside_the_clip_writes_nothing(void)
{
    for (int index = 0; index < GOLDEN_COUNT; index++) {
        Canvas canvas = fresh(grid);
        GOLDEN[index].paint(&canvas, GOLDEN[index].variant);
        int last = SIDE - 1;
        while (last > 0 && !row_has_ink(grid, last)) {
            last--;
        }
        if (last >= SIDE - 2) {
            continue;
        }
        Canvas clipped = fresh(other);
        clipped.clip = (Rect){0, last + 1, SIDE, SIDE - last - 1};
        GOLDEN[index].paint(&clipped, GOLDEN[index].variant);
        CHECK_EQUAL(SIDE * SIDE, count_of(other, BACKGROUND));
    }
}

static void every_painter_stays_inside_a_one_row_clip(void)
{
    for (size_t index = 0; index < sizeof PAINTERS / sizeof PAINTERS[0]; index++) {
        Canvas canvas = fresh(grid);
        canvas.clip = (Rect){0, 30, SIDE, 1};
        PAINTERS[index](&canvas, 0);
        for (int y = 0; y < SIDE; y++) {
            if (y != 30) {
                CHECK(!row_has_ink(grid, y));
            }
        }
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_sine_table_is_a_signed_quarter_wave),
        TEST_CASE(the_sine_table_matches_the_reference),
        TEST_CASE(the_cosine_is_the_sine_a_quarter_turn_ahead),
        TEST_CASE(the_integer_square_root_is_exact_and_floors),
        TEST_CASE(polar_points_up_at_zero_and_right_at_a_quarter),
        TEST_CASE(polar_rounds_instead_of_truncating),
        TEST_CASE(a_span_fills_its_interior_and_blends_its_edges),
        TEST_CASE(a_quarter_and_three_quarter_edge_use_fifteen_sixteenths_of_the_coverage),
        TEST_CASE(a_span_inside_one_pixel_blends_only_that_pixel),
        TEST_CASE(a_span_leaving_the_canvas_keeps_its_visible_part),
        TEST_CASE(an_empty_or_reversed_span_writes_nothing),
        TEST_CASE(a_span_writes_nothing_outside_the_clip_rows),
        TEST_CASE(a_ring_fills_its_centre_and_blends_its_edge),
        TEST_CASE(a_ring_on_a_pixel_centre_is_mirror_symmetric),
        TEST_CASE(a_ring_leaves_its_hole_untouched),
        TEST_CASE(a_sub_pixel_centre_moves_the_blend),
        TEST_CASE(a_rounded_rectangle_cuts_its_corners_and_keeps_its_edges),
        TEST_CASE(a_zero_radius_is_a_plain_rectangle),
        TEST_CASE(a_trapezoid_is_inset_at_the_top_and_full_at_the_bottom),
        TEST_CASE(a_spoke_at_angle_zero_points_up),
        TEST_CASE(a_spoke_keeps_to_its_width),
        TEST_CASE(spokes_at_a_quarter_and_three_quarters_point_right_and_left),
        TEST_CASE(a_negative_start_draws_behind_the_pivot),
        TEST_CASE(a_vertical_spoke_only_draws_the_rows_inside_its_range),
        TEST_CASE(the_digit_masks_follow_the_seven_segment_table),
        TEST_CASE(an_eight_lights_every_segment),
        TEST_CASE(a_one_lights_only_the_right_hand_segments),
        TEST_CASE(an_empty_mask_draws_everything_unlit),
        TEST_CASE(a_zero_has_no_middle_bar),
        TEST_CASE(lean_shifts_the_upper_rows_to_the_right),
        TEST_CASE(a_lit_colon_draws_two_dots_and_an_unlit_one_the_same_dots_dimmer),
        TEST_CASE(the_lattice_leaves_gaps_at_the_pitch),
        TEST_CASE(the_lattice_stays_inside_its_area_and_ignores_a_zero_pitch),
        TEST_CASE(every_primitive_matches_the_reference_pixels),
        TEST_CASE(strip_canvases_match_a_full_canvas_for_every_primitive),
        TEST_CASE(a_shape_wholly_outside_the_clip_writes_nothing),
        TEST_CASE(every_painter_stays_inside_a_one_row_clip),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
