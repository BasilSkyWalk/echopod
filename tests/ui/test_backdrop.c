#include "check.h"
#include "ui/backdrop.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    TILE_WIDTH = 4,
    TILE_HEIGHT = 2,
    PAIRS = 256,
    UNTOUCHED = 0x0821
};

static const Color PALETTE[16] = {0x1111, 0x2222, 0x3333, 0x4444};
static const uint8_t TILE[TILE_HEIGHT][TILE_WIDTH] = {{0, 1, 2, 3}, {3, 2, 1, 0}};
static const uint8_t ROWS[] = {0x01, 0x23, 0x32, 0x10};
static uint32_t pairs[PAIRS];

static Wallpaper tile_wallpaper(void)
{
    for (uint32_t pair = 0; pair < PAIRS; pair++) {
        pairs[pair] = (uint32_t)PALETTE[pair >> 4] | (uint32_t)PALETTE[pair & 0x0F] << 16;
    }
    return (Wallpaper){pairs, ROWS, TILE_WIDTH, TILE_HEIGHT, true};
}

static Theme papered(const Wallpaper *wallpaper)
{
    Theme theme = TEST_THEME;
    theme.wallpaper = wallpaper;
    return theme;
}

static Color tile_color(int x, int y)
{
    return PALETTE[TILE[y % TILE_HEIGHT][x % TILE_WIDTH]];
}

static int wrong_pixels(const Canvas *canvas, Rect painted)
{
    int wrong = 0;
    for (int y = 0; y < TEST_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < TEST_SCREEN_WIDTH; x++) {
            bool inside = x >= painted.x && x < painted.x + painted.width && y >= painted.y &&
                          y < painted.y + painted.height;
            Color expected = inside ? tile_color(x, y) : UNTOUCHED;
            wrong += canvas_pixel_at(canvas, x, y) != expected;
        }
    }
    return wrong;
}

static void without_a_wallpaper_the_area_takes_the_background_colour(void)
{
    Canvas canvas = ui_test_canvas();
    Rect area = {3, 2, 9, 5};
    backdrop_paint(&canvas, &TEST_THEME, area);
    CHECK(ui_test_area_has_only(&canvas, area, TEST_THEME.colors->background));
    CHECK_EQUAL(UNTOUCHED, canvas_pixel_at(&canvas, 2, 2));
}

static void a_wallpaper_that_is_not_ready_is_left_for_the_colour(void)
{
    Wallpaper wallpaper = tile_wallpaper();
    wallpaper.ready = false;
    Theme theme = papered(&wallpaper);
    Canvas canvas = ui_test_canvas();
    Rect area = {0, 0, TEST_SCREEN_WIDTH, TEST_SCREEN_HEIGHT};
    backdrop_paint(&canvas, &theme, area);
    CHECK(ui_test_area_has_only(&canvas, area, theme.colors->background));
}

static void a_wallpaper_repeats_across_and_down_the_screen(void)
{
    Wallpaper wallpaper = tile_wallpaper();
    Theme theme = papered(&wallpaper);
    Canvas canvas = ui_test_canvas();
    Rect area = {0, 0, TEST_SCREEN_WIDTH, TEST_SCREEN_HEIGHT};
    backdrop_paint(&canvas, &theme, area);
    CHECK_EQUAL(0, wrong_pixels(&canvas, area));
}

static void an_area_that_starts_and_ends_mid_pair_gets_the_right_pixels(void)
{
    Wallpaper wallpaper = tile_wallpaper();
    Theme theme = papered(&wallpaper);
    for (int left = 0; left < TILE_WIDTH; left++) {
        for (int width = 1; width <= 2 * TILE_WIDTH + 1; width++) {
            Canvas canvas = ui_test_canvas();
            Rect area = {left, 3, width, 3};
            backdrop_paint(&canvas, &theme, area);
            CHECK_EQUAL(0, wrong_pixels(&canvas, area));
        }
    }
}

static void painting_stays_inside_the_clip(void)
{
    Wallpaper wallpaper = tile_wallpaper();
    Theme theme = papered(&wallpaper);
    Canvas canvas = ui_test_canvas();
    Rect clip = {5, 4, 11, 6};
    Canvas clipped = canvas_clipped(&canvas, clip);
    backdrop_paint(&clipped, &theme, (Rect){0, 0, TEST_SCREEN_WIDTH, TEST_SCREEN_HEIGHT});
    CHECK_EQUAL(0, wrong_pixels(&canvas, clip));
}

static void a_canvas_the_strip_already_papered_is_not_painted_twice(void)
{
    Wallpaper wallpaper = tile_wallpaper();
    Theme theme = papered(&wallpaper);
    Canvas canvas = ui_test_canvas();
    canvas.backdrop_painted = true;
    Rect area = {0, 0, TEST_SCREEN_WIDTH, TEST_SCREEN_HEIGHT};
    backdrop_paint(&canvas, &theme, area);
    CHECK(ui_test_area_has_only(&canvas, area, UNTOUCHED));
    Canvas part = canvas_clipped(&canvas, (Rect){2, 2, 6, 6});
    CHECK(part.backdrop_painted);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(without_a_wallpaper_the_area_takes_the_background_colour),
        TEST_CASE(a_wallpaper_that_is_not_ready_is_left_for_the_colour),
        TEST_CASE(a_wallpaper_repeats_across_and_down_the_screen),
        TEST_CASE(an_area_that_starts_and_ends_mid_pair_gets_the_right_pixels),
        TEST_CASE(painting_stays_inside_the_clip),
        TEST_CASE(a_canvas_the_strip_already_papered_is_not_painted_twice),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
