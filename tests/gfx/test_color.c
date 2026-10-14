#include "check.h"
#include "gfx/color.h"

static void white_sets_every_bit(void)
{
    CHECK_EQUAL(0xFFFF, color_from_rgb(255, 255, 255));
}

static void black_is_zero(void)
{
    CHECK_EQUAL(0x0000, color_from_rgb(0, 0, 0));
}

static void primaries_fill_their_rgb565_fields(void)
{
    CHECK_EQUAL(0xF800, color_from_rgb(255, 0, 0));
    CHECK_EQUAL(0x07E0, color_from_rgb(0, 255, 0));
    CHECK_EQUAL(0x001F, color_from_rgb(0, 0, 255));
}

static void bits_below_rgb565_precision_are_dropped(void)
{
    CHECK_EQUAL(0x0000, color_from_rgb(7, 3, 7));
    CHECK_EQUAL(0x0821, color_from_rgb(8, 4, 8));
}

static void zero_alpha_keeps_the_background(void)
{
    CHECK_EQUAL(0x1234, color_blend(0x1234, 0xFFFF, 0));
}

static void opaque_alpha_takes_the_foreground(void)
{
    CHECK_EQUAL(0xFFFF, color_blend(0x1234, 0xFFFF, COLOR_ALPHA_OPAQUE));
}

static void partial_alpha_mixes_each_channel(void)
{
    CHECK_EQUAL(0x8C51, color_blend(0x0000, 0xFFFF, 8));
    CHECK_EQUAL(0x8800, color_blend(0x0000, 0xF800, 8));
}

static void the_constant_macro_matches_the_function(void)
{
    CHECK_EQUAL(color_from_rgb(18, 52, 86), COLOR_RGB(18, 52, 86));
    CHECK_EQUAL(color_from_rgb(255, 128, 1), COLOR_RGB(255, 128, 1));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(white_sets_every_bit),
        TEST_CASE(black_is_zero),
        TEST_CASE(primaries_fill_their_rgb565_fields),
        TEST_CASE(bits_below_rgb565_precision_are_dropped),
        TEST_CASE(zero_alpha_keeps_the_background),
        TEST_CASE(opaque_alpha_takes_the_foreground),
        TEST_CASE(partial_alpha_mixes_each_channel),
        TEST_CASE(the_constant_macro_matches_the_function),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
