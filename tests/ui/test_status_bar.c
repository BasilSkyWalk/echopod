#include "check.h"
#include "ui/header.h"
#include "ui/status_bar.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    STATUS_HEADER_HEIGHT = 12,
    STATUS_ICON_SIZE = 6,
    LOW_BATTERY = 10,
    HALF_BATTERY = 50
};

static Theme status_theme(void)
{
    Theme theme = TEST_THEME;
    theme.metrics.header_height = STATUS_HEADER_HEIGHT;
    theme.metrics.status_icon_size = STATUS_ICON_SIZE;
    return theme;
}

static Canvas draw_status(const Theme *theme, StatusBarState state)
{
    Canvas canvas = ui_test_canvas();
    header_draw(&canvas, theme, "");
    status_bar_draw(&canvas, theme, state);
    return canvas;
}

static StatusBarState battery_at(uint8_t percent, bool charging)
{
    return (StatusBarState){.playback = STATUS_PLAYBACK_NONE,
                            .battery_known = true,
                            .battery_percent = percent,
                            .charging = charging};
}

static void an_idle_player_without_a_battery_reading_draws_nothing(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_NONE});
    Rect header = {0, 0, TEST_SCREEN_WIDTH, STATUS_HEADER_HEIGHT - 1};
    CHECK(ui_test_area_has_only(&canvas, header, theme.colors.header_background));
}

static void a_playing_player_shows_the_play_icon_at_the_left(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PLAYING});
    Rect icon = status_bar_playback_area(&theme);
    Rect middle = {icon.x + 2, icon.y, 3, icon.height};
    CHECK(icon.x + icon.width <= TEST_HEADER_INSET);
    CHECK(ui_test_area_has_color(&canvas, middle, theme.colors.header_text));
}

static void a_paused_player_shows_two_bars(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PAUSED});
    Rect icon = status_bar_playback_area(&theme);
    Rect first = {icon.x, icon.y, 1, icon.height};
    Rect gap = {icon.x + 2, icon.y, 3, icon.height};
    Rect last = {icon.x + icon.width - 1, icon.y, 1, icon.height};
    CHECK(ui_test_area_has_only(&canvas, first, theme.colors.header_text));
    CHECK(!ui_test_area_has_color(&canvas, gap, theme.colors.header_text));
    CHECK(ui_test_area_has_only(&canvas, last, theme.colors.header_text));
}

static void the_battery_sits_inside_the_right_inset(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(HALF_BATTERY, false));
    Rect battery = status_bar_battery_area(&theme);
    CHECK(battery.x >= TEST_SCREEN_WIDTH - TEST_HEADER_INSET);
    CHECK(battery.x + battery.width <= TEST_SCREEN_WIDTH);
    CHECK(ui_test_area_has_color(&canvas, battery, theme.colors.header_text));
}

static void the_fill_follows_the_charge(void)
{
    CHECK_EQUAL(0, status_bar_battery_fill_width(0, 16));
    CHECK_EQUAL(1, status_bar_battery_fill_width(1, 16));
    CHECK_EQUAL(8, status_bar_battery_fill_width(HALF_BATTERY, 16));
    CHECK_EQUAL(16, status_bar_battery_fill_width(100, 16));
    CHECK_EQUAL(16, status_bar_battery_fill_width(250, 16));
}

static void a_low_battery_is_drawn_in_the_danger_color(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(LOW_BATTERY, false));
    CHECK(ui_test_area_has_color(&canvas, status_bar_battery_area(&theme), theme.colors.danger));
}

static void a_charging_battery_is_drawn_in_the_success_color(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(LOW_BATTERY, true));
    Rect battery = status_bar_battery_area(&theme);
    CHECK(ui_test_area_has_color(&canvas, battery, theme.colors.success));
    CHECK(!ui_test_area_has_color(&canvas, battery, theme.colors.danger));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(an_idle_player_without_a_battery_reading_draws_nothing),
        TEST_CASE(a_playing_player_shows_the_play_icon_at_the_left),
        TEST_CASE(a_paused_player_shows_two_bars),
        TEST_CASE(the_battery_sits_inside_the_right_inset),
        TEST_CASE(the_fill_follows_the_charge),
        TEST_CASE(a_low_battery_is_drawn_in_the_danger_color),
        TEST_CASE(a_charging_battery_is_drawn_in_the_success_color),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
