#include "check.h"
#include "ui/toast.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    NOW_MS = 1000
};

static void a_new_toast_is_hidden(void)
{
    Toast toast = {0};
    CHECK(!toast.visible);
}

static void a_shown_toast_lasts_its_duration(void)
{
    Toast toast = {0};
    toast_show(&toast, "A", NOW_MS);
    toast_update(&toast, NOW_MS + TOAST_DURATION_MS - 1);
    CHECK(toast.visible);
    toast_update(&toast, NOW_MS + TOAST_DURATION_MS);
    CHECK(!toast.visible);
}

static void a_visible_toast_paints_its_box(void)
{
    Toast toast = {0};
    toast_show(&toast, "AB", NOW_MS);
    Canvas canvas = ui_test_canvas();
    toast_draw(&toast, &canvas, &TEST_THEME);
    Rect area = toast_area(&toast, &TEST_THEME);
    CHECK(area.width > 0);
    CHECK(area.y + area.height <= TEST_SCREEN_HEIGHT);
    CHECK(ui_test_area_has_color(&canvas, area, TEST_THEME.colors.header_background));
    CHECK(ui_test_area_has_color(&canvas, area, TEST_THEME.colors.header_text));
}

static void a_hidden_toast_paints_nothing(void)
{
    Toast toast = {0};
    toast_show(&toast, "AB", NOW_MS);
    toast_update(&toast, NOW_MS + TOAST_DURATION_MS);
    Canvas canvas = ui_test_canvas();
    Rect screen = {0, 0, TEST_SCREEN_WIDTH, TEST_SCREEN_HEIGHT};
    toast_draw(&toast, &canvas, &TEST_THEME);
    CHECK(!ui_test_area_has_color(&canvas, screen, TEST_THEME.colors.header_background));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_toast_is_hidden),
        TEST_CASE(a_shown_toast_lasts_its_duration),
        TEST_CASE(a_visible_toast_paints_its_box),
        TEST_CASE(a_hidden_toast_paints_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
