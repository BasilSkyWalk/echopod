#include <string.h>

#include "check.h"
#include "tiny_font.h"
#include "ui/header.h"
#include "ui/status_bar.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    STATUS_HEADER_HEIGHT = 12,
    STATUS_ICON_SIZE = 6,
    LOW_BATTERY = 10,
    HALF_BATTERY = 50,
    CLOCK_INSET = 34
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
    CHECK(ui_test_area_has_only(&canvas, header, theme.colors->header_background));
}

static void a_playing_player_shows_the_play_icon_at_the_left(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PLAYING});
    Rect icon = status_bar_playback_area(&theme);
    Rect middle = {icon.x + 2, icon.y, 3, icon.height};
    CHECK(icon.x + icon.width <= TEST_HEADER_INSET);
    CHECK(ui_test_area_has_color(&canvas, middle, theme.colors->header_text));
}

static void a_paused_player_shows_two_bars(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PAUSED});
    Rect icon = status_bar_playback_area(&theme);
    Rect first = {icon.x, icon.y, 1, icon.height};
    Rect gap = {icon.x + 2, icon.y, 3, icon.height};
    Rect last = {icon.x + icon.width - 1, icon.y, 1, icon.height};
    CHECK(ui_test_area_has_only(&canvas, first, theme.colors->header_text));
    CHECK(!ui_test_area_has_color(&canvas, gap, theme.colors->header_text));
    CHECK(ui_test_area_has_only(&canvas, last, theme.colors->header_text));
}

static void the_battery_sits_inside_the_right_inset(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(HALF_BATTERY, false));
    Rect battery = status_bar_battery_area(&theme);
    CHECK(battery.x >= TEST_SCREEN_WIDTH - TEST_HEADER_INSET);
    CHECK(battery.x + battery.width <= TEST_SCREEN_WIDTH);
    CHECK(ui_test_area_has_color(&canvas, battery, theme.colors->header_text));
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
    CHECK(ui_test_area_has_color(&canvas, status_bar_battery_area(&theme), theme.colors->danger));
}

static void a_charging_battery_is_drawn_in_the_success_color(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(LOW_BATTERY, true));
    Rect battery = status_bar_battery_area(&theme);
    CHECK(ui_test_area_has_color(&canvas, battery, theme.colors->success));
    CHECK(!ui_test_area_has_color(&canvas, battery, theme.colors->danger));
}

static void the_percentage_reads_as_a_number_and_a_percent_sign(void)
{
    char text[STATUS_BAR_PERCENT_CAPACITY];
    status_bar_percent_text(73, text, sizeof text);
    CHECK(strcmp("73%", text) == 0);
    status_bar_percent_text(100, text, sizeof text);
    CHECK(strcmp("100%", text) == 0);
    status_bar_percent_text(0, text, sizeof text);
    CHECK(strcmp("0%", text) == 0);
}

static void the_percentage_sits_just_left_of_the_battery(void)
{
    Theme theme = status_theme();
    Rect battery = status_bar_battery_area(&theme);
    Rect percent = status_bar_percent_area(&theme, "73%", false);
    CHECK(percent.width > 0);
    CHECK(percent.x + percent.width < battery.x);
    CHECK(percent.x + percent.width >= battery.x - theme.metrics.padding);
}

static void a_known_battery_shows_its_percentage(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(HALF_BATTERY, false));
    Rect percent = status_bar_percent_area(&theme, "50%", false);
    CHECK(!ui_test_area_has_only(&canvas, percent, theme.colors->header_background));
}

static void an_unknown_battery_shows_no_percentage(void)
{
    Theme theme = status_theme();
    StatusBarState state = battery_at(HALF_BATTERY, false);
    state.battery_known = false;
    Canvas canvas = draw_status(&theme, state);
    Rect percent = status_bar_percent_area(&theme, "50%", false);
    CHECK(ui_test_area_has_only(&canvas, percent, theme.colors->header_background));
}

static void a_charging_battery_has_a_bolt_beside_it(void)
{
    Theme theme = status_theme();
    Rect bolt = status_bar_bolt_area(&theme);
    Rect battery = status_bar_battery_area(&theme);
    CHECK(bolt.x + bolt.width < battery.x);
    Canvas charging = draw_status(&theme, battery_at(50, true));
    CHECK(ui_test_area_has_color(&charging, bolt, theme.colors->success));
    Canvas resting = draw_status(&theme, battery_at(50, false));
    CHECK(!ui_test_area_has_color(&resting, bolt, theme.colors->success));
}

static void the_percentage_moves_left_of_the_bolt_while_charging(void)
{
    Theme theme = status_theme();
    Rect bolt = status_bar_bolt_area(&theme);
    Rect percent = status_bar_percent_area(&theme, "50%", true);
    CHECK(percent.x + percent.width < bolt.x);
    Canvas canvas = draw_status(&theme, battery_at(50, true));
    CHECK(!ui_test_area_has_only(&canvas, percent, theme.colors->header_background));
}

static void the_clock_sits_after_the_left_icons_inside_the_inset(void)
{
    Theme theme = status_theme();
    theme.metrics.header_inset = CLOCK_INSET;
    StatusBarState state = {.playback = STATUS_PLAYBACK_PLAYING, .clock = "12:34"};
    Rect clock = status_bar_clock_area(&theme, state);
    Rect bluetooth = status_bar_bluetooth_area(&theme);
    CHECK(clock.width > 0);
    CHECK(clock.x > bluetooth.x + bluetooth.width);
    CHECK(clock.x + clock.width <= theme.metrics.header_inset);
    Canvas canvas = draw_status(&theme, state);
    CHECK(!ui_test_area_has_only(&canvas, clock, theme.colors->header_background));
    state.clock = NULL;
    CHECK(rect_is_empty(status_bar_clock_area(&theme, state)));
    Canvas without = draw_status(&theme, state);
    CHECK(ui_test_area_has_only(&without, clock, theme.colors->header_background));
}

static void a_clock_that_does_not_fit_the_inset_is_left_out(void)
{
    Theme theme = status_theme();
    theme.metrics.header_inset = CLOCK_INSET;
    StatusBarState state = battery_at(HALF_BATTERY, false);
    state.clock = "12:34:56 and more";
    CHECK(rect_is_empty(status_bar_clock_area(&theme, state)));
}

static void with_a_status_band_the_clock_is_centered_between_the_icons_and_the_battery(void)
{
    Theme theme = status_theme();
    theme.metrics.status_height = STATUS_HEADER_HEIGHT;
    theme.metrics.header_inset = theme.metrics.padding;
    StatusBarState state = battery_at(HALF_BATTERY, false);
    state.hide_percent = true;
    state.clock = "12:34";
    Rect clock = status_bar_clock_area(&theme, state);
    Rect battery = status_bar_battery_area(&theme);
    Rect bluetooth = status_bar_bluetooth_area(&theme);
    CHECK(clock.width > 0);
    CHECK(clock.x > bluetooth.x + bluetooth.width);
    CHECK(clock.x + clock.width < battery.x);
    CHECK(clock.x - (bluetooth.x + bluetooth.width) >= battery.x - (clock.x + clock.width) - 2);
    state.hide_percent = false;
    CHECK(rect_is_empty(status_bar_clock_area(&theme, state)));
}

static void a_status_font_replaces_the_small_font(void)
{
    Theme theme = status_theme();
    CHECK_EQUAL(6, status_bar_percent_area(&theme, "50%", false).width);
    theme.fonts.status = &TINY_FONT_WITHOUT_FALLBACK;
    CHECK_EQUAL(0, status_bar_percent_area(&theme, "50%", false).width);
}

static void a_clock_too_wide_to_center_sits_after_the_left_icons(void)
{
    Theme theme = status_theme();
    theme.metrics.status_height = STATUS_HEADER_HEIGHT;
    theme.metrics.header_inset = theme.metrics.padding;
    StatusBarState state = battery_at(HALF_BATTERY, false);
    state.hide_percent = true;
    state.clock = "AAA";
    Rect clock = status_bar_clock_area(&theme, state);
    Rect bluetooth = status_bar_bluetooth_area(&theme);
    CHECK_EQUAL(12, clock.width);
    CHECK_EQUAL(bluetooth.x + bluetooth.width + 1, clock.x);
    CHECK(clock.x + clock.width < status_bar_battery_area(&theme).x);
    state.clock = "AAAA";
    CHECK(rect_is_empty(status_bar_clock_area(&theme, state)));
}

static void the_percentage_gives_way_to_a_clock_that_does_not_fit_beside_it(void)
{
    Theme theme = status_theme();
    theme.metrics.status_height = STATUS_HEADER_HEIGHT;
    theme.metrics.header_inset = theme.metrics.padding;
    StatusBarState state = battery_at(HALF_BATTERY, false);
    state.clock = "AAA";
    CHECK(rect_is_empty(status_bar_clock_area(&theme, state)));
    Canvas canvas = draw_status(&theme, state);
    Color shown[TEST_SCREEN_WIDTH * STATUS_HEADER_HEIGHT];
    for (int y = 0; y < STATUS_HEADER_HEIGHT; y++) {
        for (int x = 0; x < TEST_SCREEN_WIDTH; x++) {
            shown[y * TEST_SCREEN_WIDTH + x] = canvas_pixel_at(&canvas, x, y);
        }
    }
    state.hide_percent = true;
    Rect clock = status_bar_clock_area(&theme, state);
    CHECK(!rect_is_empty(clock));
    Canvas expected = draw_status(&theme, state);
    int differences = 0;
    for (int y = 0; y < STATUS_HEADER_HEIGHT; y++) {
        for (int x = 0; x < TEST_SCREEN_WIDTH; x++) {
            differences += shown[y * TEST_SCREEN_WIDTH + x] != canvas_pixel_at(&expected, x, y);
        }
    }
    CHECK_EQUAL(0, differences);
    CHECK(!ui_test_area_has_only(&expected, clock, theme.colors->header_background));
}

static const uint8_t SOLID_ROWS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t HALF_ROWS[] = {0x88, 0x80, 0x88, 0x80, 0x88, 0x80};
static const uint8_t FRAME_ROWS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t LEVEL_ROWS[] = {0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0, 0};

static Theme icon_theme(const ThemeIcons *icons)
{
    Theme theme = status_theme();
    theme.icons = icons;
    return theme;
}

static void a_theme_icon_replaces_the_play_shape_centered_in_its_place(void)
{
    ThemeIcons icons = {.masks[THEME_ICON_STATUS_PLAY] = {SOLID_ROWS, 3, 3}};
    Theme theme = icon_theme(&icons);
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PLAYING});
    Rect area = status_bar_playback_area(&theme);
    Rect inked = {area.x + 1, area.y + 1, 3, 3};
    Rect left_edge = {area.x, area.y, 1, area.height};
    CHECK(ui_test_area_has_only(&canvas, inked, theme.colors->header_text));
    CHECK(!ui_test_area_has_color(&canvas, left_edge, theme.colors->header_text));
}

static void an_icon_the_theme_lacks_keeps_the_built_in_shape(void)
{
    ThemeIcons icons = {.masks[THEME_ICON_STATUS_PLAY] = {SOLID_ROWS, 3, 3}};
    Theme theme = icon_theme(&icons);
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PAUSED});
    Rect area = status_bar_playback_area(&theme);
    Rect first = {area.x, area.y, 1, area.height};
    CHECK(ui_test_area_has_only(&canvas, first, theme.colors->header_text));
}

static void a_level_between_clear_and_solid_blends_with_what_is_behind(void)
{
    ThemeIcons icons = {.masks[THEME_ICON_STATUS_PLAY] = {HALF_ROWS, 3, 3}};
    Theme theme = icon_theme(&icons);
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PLAYING});
    Rect area = status_bar_playback_area(&theme);
    Color blended = canvas_pixel_at(&canvas, area.x + 2, area.y + 2);
    CHECK(blended != theme.colors->header_text && blended != theme.colors->header_background);
}

static void a_large_icon_is_used_when_it_fits_the_status_icon_size(void)
{
    ThemeIcons icons = {.masks[THEME_ICON_STATUS_PLAY] = {SOLID_ROWS, 3, 3},
                        .masks[THEME_ICON_STATUS_PLAY_LARGE] = {SOLID_ROWS, 5, 6}};
    Theme theme = icon_theme(&icons);
    StatusBarState playing = {.playback = STATUS_PLAYBACK_PLAYING};
    Canvas canvas = draw_status(&theme, playing);
    Rect area = status_bar_playback_area(&theme);
    Rect large = {area.x, area.y, 5, 6};
    CHECK(ui_test_area_has_only(&canvas, large, theme.colors->header_text));
    icons.masks[THEME_ICON_STATUS_PLAY_LARGE].height = 7;
    canvas = draw_status(&theme, playing);
    Rect top_row = {area.x, area.y, area.width, 1};
    CHECK(!ui_test_area_has_color(&canvas, top_row, theme.colors->header_text));
}

static Canvas draw_themed_battery(const ThemeIcons *icons, uint8_t percent, Rect *origin)
{
    Theme theme = icon_theme(icons);
    Rect area = status_bar_battery_area(&theme);
    *origin = (Rect){area.x + (area.width - 8) / 2, area.y + 1, 8, 4};
    StatusBarState state = battery_at(percent, true);
    state.hide_percent = true;
    return draw_status(&theme, state);
}

static void a_theme_battery_lights_its_level_from_the_left_by_the_charge(void)
{
    ThemeIcons icons = {.masks[THEME_ICON_BATTERY] = {FRAME_ROWS, 8, 4},
                        .masks[THEME_ICON_BATTERY_LEVEL] = {LEVEL_ROWS, 8, 4}};
    Rect at;
    Canvas canvas = draw_themed_battery(&icons, HALF_BATTERY, &at);
    Rect frame = {at.x, at.y, 8, 1};
    Rect lit = {at.x + 2, at.y + 1, 2, 2};
    Rect unlit = {at.x + 4, at.y + 1, 2, 2};
    CHECK(ui_test_area_has_only(&canvas, frame, TEST_COLORS.header_text));
    CHECK(ui_test_area_has_only(&canvas, lit, TEST_COLORS.success));
    CHECK(ui_test_area_has_only(&canvas, unlit, TEST_COLORS.header_background));
    canvas = draw_themed_battery(&icons, 100, &at);
    CHECK(ui_test_area_has_only(&canvas, unlit, TEST_COLORS.success));
    canvas = draw_themed_battery(&icons, 1, &at);
    Rect least = {at.x + 2, at.y + 1, 1, 2};
    CHECK(ui_test_area_has_only(&canvas, least, TEST_COLORS.success));
}

static void a_theme_battery_without_a_level_of_its_size_stays_the_built_in_one(void)
{
    ThemeIcons icons = {.masks[THEME_ICON_BATTERY] = {FRAME_ROWS, 8, 4}};
    Rect at;
    Canvas canvas = draw_themed_battery(&icons, HALF_BATTERY, &at);
    Theme theme = status_theme();
    Rect area = status_bar_battery_area(&theme);
    Rect left_side = {area.x, area.y, 1, area.height};
    CHECK(ui_test_area_has_only(&canvas, left_side, TEST_COLORS.header_text));
    icons.masks[THEME_ICON_BATTERY_LEVEL] = (IconMask){LEVEL_ROWS, 6, 4};
    canvas = draw_themed_battery(&icons, HALF_BATTERY, &at);
    CHECK(ui_test_area_has_only(&canvas, left_side, TEST_COLORS.header_text));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_clock_sits_after_the_left_icons_inside_the_inset),
        TEST_CASE(a_clock_that_does_not_fit_the_inset_is_left_out),
        TEST_CASE(with_a_status_band_the_clock_is_centered_between_the_icons_and_the_battery),
        TEST_CASE(the_percentage_reads_as_a_number_and_a_percent_sign),
        TEST_CASE(the_percentage_sits_just_left_of_the_battery),
        TEST_CASE(a_known_battery_shows_its_percentage),
        TEST_CASE(an_unknown_battery_shows_no_percentage),
        TEST_CASE(an_idle_player_without_a_battery_reading_draws_nothing),
        TEST_CASE(a_playing_player_shows_the_play_icon_at_the_left),
        TEST_CASE(a_paused_player_shows_two_bars),
        TEST_CASE(the_battery_sits_inside_the_right_inset),
        TEST_CASE(the_fill_follows_the_charge),
        TEST_CASE(a_low_battery_is_drawn_in_the_danger_color),
        TEST_CASE(a_charging_battery_is_drawn_in_the_success_color),
        TEST_CASE(a_charging_battery_has_a_bolt_beside_it),
        TEST_CASE(the_percentage_moves_left_of_the_bolt_while_charging),
        TEST_CASE(a_status_font_replaces_the_small_font),
        TEST_CASE(a_clock_too_wide_to_center_sits_after_the_left_icons),
        TEST_CASE(the_percentage_gives_way_to_a_clock_that_does_not_fit_beside_it),
        TEST_CASE(a_theme_icon_replaces_the_play_shape_centered_in_its_place),
        TEST_CASE(an_icon_the_theme_lacks_keeps_the_built_in_shape),
        TEST_CASE(a_level_between_clear_and_solid_blends_with_what_is_behind),
        TEST_CASE(a_large_icon_is_used_when_it_fits_the_status_icon_size),
        TEST_CASE(a_theme_battery_lights_its_level_from_the_left_by_the_charge),
        TEST_CASE(a_theme_battery_without_a_level_of_its_size_stays_the_built_in_one),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
