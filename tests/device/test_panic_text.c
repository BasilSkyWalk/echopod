#include "check.h"
#include "device/panic_text.h"

enum {
    SCALE = 2,
    WIDTH = PANIC_GLYPH_ADVANCE * SCALE,
    HEIGHT = PANIC_GLYPH_ROWS * SCALE,
    FG = 0xFFFF,
    BG = 0x0000
};

static int lit_pixels(const uint16_t *pixels)
{
    int lit = 0;
    for (int index = 0; index < WIDTH * HEIGHT; index++) {
        lit += pixels[index] == FG;
    }
    return lit;
}

static void a_space_draws_only_background(void)
{
    uint16_t pixels[WIDTH * HEIGHT];
    panic_render_glyph(' ', SCALE, (PanicColors){FG, BG}, pixels);
    CHECK_EQUAL(0, lit_pixels(pixels));
}

static void every_character_the_screen_needs_has_a_glyph(void)
{
    static const char NEEDED[] = "0123456789ABCDEFHLMPRSTUV";
    uint16_t pixels[WIDTH * HEIGHT];
    for (const char *character = NEEDED; *character != '\0'; character++) {
        panic_render_glyph(*character, SCALE, (PanicColors){FG, BG}, pixels);
        CHECK(lit_pixels(pixels) > 0);
    }
}

static void the_one_is_a_column_scaled_up(void)
{
    uint16_t pixels[WIDTH * HEIGHT];
    panic_render_glyph('1', SCALE, (PanicColors){FG, BG}, pixels);
    CHECK_EQUAL(FG, pixels[(HEIGHT - 1) * WIDTH + 2 * SCALE]);
    CHECK_EQUAL(FG, pixels[(HEIGHT - 1) * WIDTH + 2 * SCALE + 1]);
    CHECK_EQUAL(BG, pixels[(HEIGHT - 1) * WIDTH + WIDTH - 1]);
}

static void lowercase_and_unknown_characters_draw_blank(void)
{
    uint16_t pixels[WIDTH * HEIGHT];
    panic_render_glyph('?', SCALE, (PanicColors){FG, BG}, pixels);
    CHECK_EQUAL(0, lit_pixels(pixels));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_space_draws_only_background),
        TEST_CASE(every_character_the_screen_needs_has_a_glyph),
        TEST_CASE(the_one_is_a_column_scaled_up),
        TEST_CASE(lowercase_and_unknown_characters_draw_blank),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
