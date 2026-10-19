#include <stdint.h>
#include <stdlib.h>

#include "check.h"
#include "image/cover_grid.h"

enum {
    FULL = COVER_GRID_SIZE,
    HALF = COVER_GRID_HALF
};

static uint16_t full[COVER_GRID_CELLS];
static uint16_t half[COVER_GRID_HALF_CELLS];

static uint16_t rgb565(uint32_t red, uint32_t green, uint32_t blue)
{
    return (uint16_t)((red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3));
}

static CoverGrid *new_grid(uint32_t width, uint32_t height)
{
    CoverGrid *grid = malloc(sizeof *grid);
    CHECK(cover_grid_init(grid, width, height));
    return grid;
}

static void add_pixel(CoverGrid *grid, uint32_t x, uint32_t y, int32_t red, int32_t green,
                      int32_t blue)
{
    int32_t values[3] = {red, green, blue};
    cover_grid_add_color(grid, (CoverRect){x, y, 1, 1}, values, 3);
}

static void a_uniform_image_renders_uniform_covers(void)
{
    CoverGrid *grid = new_grid(224, 224);
    for (uint32_t y = 0; y < 224; y++) {
        for (uint32_t x = 0; x < 224; x++) {
            add_pixel(grid, x, y, 100, 150, 200);
        }
    }
    cover_grid_render(grid, COVER_COLOR_RGB, full, half);
    CHECK_EQUAL(rgb565(100, 150, 200), full[0]);
    CHECK_EQUAL(rgb565(100, 150, 200), full[FULL * FULL - 1]);
    CHECK_EQUAL(rgb565(100, 150, 200), half[HALF * 20 + 33]);
    free(grid);
}

static void the_crop_keeps_the_centered_square(void)
{
    CoverGrid *grid = new_grid(336, 224);
    for (uint32_t y = 0; y < 224; y++) {
        for (uint32_t x = 0; x < 336; x++) {
            bool inside = x >= 56 && x < 280;
            bool left = x < 168;
            add_pixel(grid, x, y, inside && left ? 255 : 0, 0, inside && !left ? 255 : 0);
        }
    }
    cover_grid_render(grid, COVER_COLOR_RGB, full, half);
    CHECK_EQUAL(rgb565(255, 0, 0), full[0]);
    CHECK_EQUAL(rgb565(255, 0, 0), full[55]);
    CHECK_EQUAL(rgb565(0, 0, 255), full[56]);
    CHECK_EQUAL(rgb565(0, 0, 255), full[FULL * 111 + 111]);
    CHECK_EQUAL(rgb565(255, 0, 0), half[27]);
    CHECK_EQUAL(rgb565(0, 0, 255), half[28]);
    free(grid);
}

static void three_pixels_per_cell_average_exactly(void)
{
    CoverGrid *grid = new_grid(336, 336);
    for (uint32_t y = 0; y < 336; y++) {
        for (uint32_t x = 0; x < 336; x++) {
            add_pixel(grid, x, y, (int32_t)(x * 3 % 256), (int32_t)(y % 7 * 30), 0);
        }
    }
    cover_grid_render(grid, COVER_COLOR_RGB, full, half);
    for (uint32_t cell = 0; cell < FULL; cell++) {
        uint32_t red = 0;
        for (uint32_t x = cell * 3; x < cell * 3 + 3; x++) {
            red += x * 3 % 256;
        }
        uint32_t expected = (red * 2 + 3) / 6;
        uint32_t green_sum = 0;
        for (uint32_t y = 0; y < 3; y++) {
            green_sum += y % 7 * 30;
        }
        CHECK_EQUAL(rgb565(expected, (green_sum * 2 + 3) / 6, 0), full[cell]);
    }
    free(grid);
}

static double reference_cell(uint32_t span, uint32_t cell, const uint8_t *row)
{
    double low = (double)cell * span / FULL;
    double high = (double)(cell + 1) * span / FULL;
    double sum = 0.0;
    for (uint32_t x = (uint32_t)low; x < span && x < high; x++) {
        double from = x > low ? x : low;
        double to = x + 1.0 < high ? x + 1.0 : high;
        sum += row[x] * (to - from);
    }
    return sum / (high - low);
}

static void an_uneven_ratio_matches_an_exact_area_average(void)
{
    enum {
        SPAN = 250
    };
    static uint8_t row[SPAN];
    for (uint32_t x = 0; x < SPAN; x++) {
        row[x] = (uint8_t)(x * 37 % 256);
    }
    CoverGrid *grid = new_grid(SPAN, SPAN);
    for (uint32_t y = 0; y < SPAN; y++) {
        for (uint32_t x = 0; x < SPAN; x++) {
            add_pixel(grid, x, y, row[x], row[x], row[x]);
        }
    }
    cover_grid_render(grid, COVER_COLOR_GRAY, full, half);
    uint32_t off = 0;
    for (uint32_t cell = 0; cell < FULL; cell++) {
        double expected = reference_cell(SPAN, cell, row);
        int32_t got = (full[FULL * 50 + cell] >> 5 & 0x3F) * 255 / 63;
        off += got - expected > 6 || expected - got > 6;
    }
    CHECK_EQUAL(0, off);
    free(grid);
}

static void a_small_image_is_scaled_up_by_repeating_pixels(void)
{
    CoverGrid *grid = new_grid(56, 56);
    for (uint32_t y = 0; y < 56; y++) {
        for (uint32_t x = 0; x < 56; x++) {
            add_pixel(grid, x, y, (int32_t)(x * 4), (int32_t)(y * 4), 0);
        }
    }
    cover_grid_render(grid, COVER_COLOR_RGB, full, half);
    CHECK_EQUAL(rgb565(40, 80, 0), full[FULL * 40 + 20]);
    CHECK_EQUAL(rgb565(40, 80, 0), full[FULL * 41 + 21]);
    CHECK_EQUAL(rgb565(40, 80, 0), half[HALF * 20 + 10]);
    free(grid);
}

static void half_resolution_chroma_converts_to_rgb(void)
{
    CoverGrid *grid = new_grid(64, 64);
    cover_grid_set_sampling(grid, 1, 2, 2);
    cover_grid_set_sampling(grid, 2, 2, 2);
    for (uint32_t y = 0; y < 64; y++) {
        for (uint32_t x = 0; x < 64; x++) {
            cover_grid_add(grid, 0, (CoverRect){x, y, 1, 1}, 76);
        }
    }
    for (uint32_t y = 0; y < 32; y++) {
        for (uint32_t x = 0; x < 32; x++) {
            cover_grid_add(grid, 1, (CoverRect){2 * x, 2 * y, 2, 2}, 85);
            cover_grid_add(grid, 2, (CoverRect){2 * x, 2 * y, 2, 2}, 255);
        }
    }
    cover_grid_render(grid, COVER_COLOR_YCBCR, full, half);
    uint16_t red = full[FULL * 60 + 70];
    CHECK((red >> 11) >= 30);
    CHECK((red >> 5 & 0x3F) <= 2);
    CHECK((red & 0x1F) <= 1);
    CHECK_EQUAL(red, half[HALF * 30 + 35]);
    free(grid);
}

static void rectangles_outside_the_crop_do_not_touch_it(void)
{
    CoverGrid *grid = new_grid(300, 200);
    CHECK(!cover_grid_touches(grid, (CoverRect){0, 0, 50, 200}));
    CHECK(cover_grid_touches(grid, (CoverRect){0, 0, 51, 1}));
    CHECK(!cover_grid_touches(grid, (CoverRect){250, 0, 8, 8}));
    CHECK(cover_grid_touches(grid, (CoverRect){249, 199, 8, 8}));
    CHECK(!cover_grid_init(grid, 0, 10));
    CHECK(!cover_grid_init(grid, 20000, 10));
    free(grid);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_uniform_image_renders_uniform_covers),
        TEST_CASE(the_crop_keeps_the_centered_square),
        TEST_CASE(three_pixels_per_cell_average_exactly),
        TEST_CASE(an_uneven_ratio_matches_an_exact_area_average),
        TEST_CASE(a_small_image_is_scaled_up_by_repeating_pixels),
        TEST_CASE(half_resolution_chroma_converts_to_rgb),
        TEST_CASE(rectangles_outside_the_crop_do_not_touch_it),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
