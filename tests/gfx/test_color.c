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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(white_sets_every_bit),
        TEST_CASE(black_is_zero),
        TEST_CASE(primaries_fill_their_rgb565_fields),
        TEST_CASE(bits_below_rgb565_precision_are_dropped),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
