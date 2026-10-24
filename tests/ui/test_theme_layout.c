#include "check.h"
#include "ui_test_theme.h"

static void the_header_spans_the_top_of_the_screen(void)
{
    Rect header = theme_header_area(&TEST_THEME);
    CHECK_EQUAL(0, header.y);
    CHECK_EQUAL(TEST_SCREEN_WIDTH, header.width);
    CHECK_EQUAL(TEST_HEADER_HEIGHT, header.height);
}

static void the_content_fills_the_rest_of_the_screen(void)
{
    Rect content = theme_content_area(&TEST_THEME);
    CHECK_EQUAL(TEST_HEADER_HEIGHT, content.y);
    CHECK_EQUAL(TEST_SCREEN_WIDTH, content.width);
    CHECK_EQUAL(TEST_SCREEN_HEIGHT - TEST_HEADER_HEIGHT, content.height);
}

static void visible_rows_are_the_whole_rows_that_fit_the_content(void)
{
    CHECK_EQUAL(TEST_VISIBLE_ROWS, theme_visible_rows(&TEST_THEME));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_header_spans_the_top_of_the_screen),
        TEST_CASE(the_content_fills_the_rest_of_the_screen),
        TEST_CASE(visible_rows_are_the_whole_rows_that_fit_the_content),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
