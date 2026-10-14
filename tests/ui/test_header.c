#include "check.h"
#include "ui/header.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

static void the_header_is_filled_with_its_background(void)
{
    Canvas canvas = ui_test_canvas();
    header_draw(&canvas, &TEST_THEME, "");
    Rect header = theme_header_area(&TEST_THEME);
    Rect body = {header.x, header.y, header.width, header.height - 1};
    CHECK(ui_test_area_has_only(&canvas, body, TEST_THEME.colors.header_background));
}

static void a_divider_separates_the_header_from_the_content(void)
{
    Canvas canvas = ui_test_canvas();
    header_draw(&canvas, &TEST_THEME, "");
    Rect divider = {0, TEST_HEADER_HEIGHT - 1, TEST_SCREEN_WIDTH, 1};
    CHECK(ui_test_area_has_only(&canvas, divider, TEST_THEME.colors.divider));
}

static void the_title_is_centred(void)
{
    Canvas canvas = ui_test_canvas();
    header_draw(&canvas, &TEST_THEME, "A");
    Rect left_third = {0, 0, TEST_SCREEN_WIDTH / 3, TEST_HEADER_HEIGHT};
    Rect middle_third = {TEST_SCREEN_WIDTH / 3, 0, TEST_SCREEN_WIDTH / 3, TEST_HEADER_HEIGHT};
    CHECK(ui_test_area_has_color(&canvas, middle_third, TEST_THEME.colors.header_text));
    CHECK(!ui_test_area_has_color(&canvas, left_third, TEST_THEME.colors.header_text));
}

static void a_long_title_stays_clear_of_the_header_insets(void)
{
    Canvas canvas = ui_test_canvas();
    header_draw(&canvas, &TEST_THEME, "ABABABABABABAB");
    Rect left = {0, 0, TEST_HEADER_INSET, TEST_HEADER_HEIGHT};
    Rect right = {TEST_SCREEN_WIDTH - TEST_HEADER_INSET, 0, TEST_HEADER_INSET, TEST_HEADER_HEIGHT};
    CHECK(!ui_test_area_has_color(&canvas, left, TEST_THEME.colors.header_text));
    CHECK(!ui_test_area_has_color(&canvas, right, TEST_THEME.colors.header_text));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_header_is_filled_with_its_background),
        TEST_CASE(a_divider_separates_the_header_from_the_content),
        TEST_CASE(the_title_is_centred),
        TEST_CASE(a_long_title_stays_clear_of_the_header_insets),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
