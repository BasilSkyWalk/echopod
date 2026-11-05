#include <string.h>

#include "check.h"
#include "clock_store.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "hal/display.h"
#include "screens/date_time_screen.h"
#include "settings_store.h"

enum {
    START_MS = 1000,
    FIELD_YEAR = 0,
    FIELD_MONTH = 1,
    FIELD_DAY = 2,
    FIELD_HOUR = 3,
    FIELD_MINUTE = 4,
    FIELD_ZONE = 5
};

static ClockStore clock_store;
static SettingsStore store;
static Toast toast;
static DateTimeScreen date_time;

static Screen *open_screen(HalTime chip)
{
    fake_hal_reset();
    fake_hal_set_milliseconds(START_MS);
    fake_hal_set_time(chip);
    settings_store_load(&store);
    clock_store = (ClockStore){0};
    clock_store_start(&clock_store, &store.settings, START_MS);
    toast = (Toast){0};
    Screen *screen = date_time_screen_init(&date_time, (DateTimeConfig){.clock = &clock_store,
                                                                        .store = &store,
                                                                        .toast = &toast,
                                                                        .theme = &DEFAULT_THEME});
    ScreenAction open = date_time_screen_open(&date_time);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, open.type);
    CHECK(open.target == screen);
    screen->enter(screen->context, START_MS);
    return screen;
}

static HalTime running_chip(void)
{
    return (HalTime){.present = true,
                     .valid = true,
                     .year = 2026,
                     .month = 11,
                     .day = 5,
                     .hour = 9,
                     .minute = 8,
                     .second = 7};
}

static ScreenAction press(Screen *screen, Key key, KeyGesture gesture)
{
    return screen->handle_key(screen->context, (KeyEvent){.key = key, .gesture = gesture},
                              START_MS);
}

static ScreenAction tap(Screen *screen, Key key)
{
    return press(screen, key, KEY_GESTURE_PRESS);
}

static void go_to_field(Screen *screen, int field)
{
    while (date_time.field != field) {
        tap(screen, KEY_VOLUME_UP);
    }
}

static void adjust(Screen *screen, int field, int delta)
{
    go_to_field(screen, field);
    tap(screen, KEY_PLAY);
    CHECK(date_time.editing);
    for (int step = 0; step < (delta < 0 ? -delta : delta); step++) {
        tap(screen, delta < 0 ? KEY_VOLUME_DOWN : KEY_VOLUME_UP);
    }
    tap(screen, KEY_PLAY);
    CHECK(!date_time.editing);
}

static void the_editor_starts_from_the_clock_when_it_is_known(void)
{
    open_screen(running_chip());
    CHECK_EQUAL(2026, date_time.civil.year);
    CHECK_EQUAL(11, date_time.civil.month);
    CHECK_EQUAL(5, date_time.civil.day);
    CHECK_EQUAL(9, date_time.civil.hour);
    CHECK_EQUAL(8, date_time.civil.minute);
    CHECK_EQUAL(0, date_time.civil.second);
    CHECK_EQUAL(WALL_CLOCK_ZONE_UTC, date_time.zone);
    CHECK_EQUAL(0, date_time.field);
    CHECK(!date_time.editing);
    CHECK(strcmp("2026-11-05 09:08", date_time_screen_summary(&date_time)) == 0);
}

static void the_editor_starts_from_a_fixed_date_when_the_clock_is_unknown(void)
{
    open_screen((HalTime){.present = false});
    CHECK(strcmp("Not set", date_time_screen_summary(&date_time)) == 0);
    CHECK_EQUAL(2026, date_time.civil.year);
    CHECK_EQUAL(1, date_time.civil.month);
    CHECK_EQUAL(1, date_time.civil.day);
    CHECK_EQUAL(0, date_time.civil.hour);
    CHECK_EQUAL(0, date_time.civil.minute);
}

static void volume_keys_move_between_fields_and_wrap(void)
{
    Screen *screen = open_screen(running_chip());
    tap(screen, KEY_VOLUME_DOWN);
    CHECK_EQUAL(FIELD_ZONE, date_time.field);
    tap(screen, KEY_VOLUME_UP);
    CHECK_EQUAL(FIELD_YEAR, date_time.field);
    tap(screen, KEY_VOLUME_UP);
    CHECK_EQUAL(FIELD_MONTH, date_time.field);
    CHECK(!date_time.changed);
}

static void play_adjusts_the_chosen_field_and_values_wrap(void)
{
    Screen *screen = open_screen(running_chip());
    adjust(screen, FIELD_MONTH, 2);
    CHECK_EQUAL(1, date_time.civil.month);
    adjust(screen, FIELD_HOUR, -10);
    CHECK_EQUAL(23, date_time.civil.hour);
    adjust(screen, FIELD_MINUTE, 52);
    CHECK_EQUAL(0, date_time.civil.minute);
    adjust(screen, FIELD_YEAR, -27);
    CHECK_EQUAL(2099, date_time.civil.year);
    adjust(screen, FIELD_ZONE, 2);
    CHECK_EQUAL(30, wall_clock_zone_minutes(date_time.zone));
    CHECK(date_time.changed);
}

static void back_while_adjusting_only_stops_adjusting(void)
{
    Screen *screen = open_screen(running_chip());
    tap(screen, KEY_PLAY);
    CHECK(date_time.editing);
    ScreenAction action = tap(screen, KEY_BACK);
    CHECK_EQUAL(SCREEN_ACTION_NONE, action.type);
    CHECK(!date_time.editing);
    CHECK_EQUAL(0, fake_hal_time_write_count());
}

static void leaving_without_a_change_sets_nothing(void)
{
    Screen *screen = open_screen(running_chip());
    int writes = fake_hal_write_count();
    ScreenAction action = tap(screen, KEY_BACK);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(0, fake_hal_time_write_count());
    CHECK_EQUAL(writes, fake_hal_write_count());
    CHECK(!toast.visible);
}

static void leaving_after_a_change_sets_the_clock_the_chip_and_the_zone(void)
{
    Screen *screen = open_screen(running_chip());
    adjust(screen, FIELD_DAY, 1);
    adjust(screen, FIELD_MINUTE, 7);
    adjust(screen, FIELD_ZONE, 4);
    ScreenAction action = tap(screen, KEY_BACK);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(1, fake_hal_time_write_count());
    HalTime written = fake_hal_time();
    CHECK_EQUAL(6, written.day);
    CHECK_EQUAL(15, written.minute);
    CHECK_EQUAL(0, written.second);
    CHECK_EQUAL(wall_clock_zone_of(60), store.settings.options[OPTION_TIME_ZONE]);
    Settings saved;
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(SETTINGS_STORE_PATH, &size);
    CHECK(bytes != NULL && settings_decode(bytes, size, &saved));
    CHECK_EQUAL(wall_clock_zone_of(60), saved.options[OPTION_TIME_ZONE]);
    CHECK(toast.visible && strcmp("Clock set", toast.text) == 0);
    WallCivil civil;
    CHECK(clock_store_now(&clock_store, START_MS, &civil));
    CHECK_EQUAL(6, civil.day);
    CHECK_EQUAL(15, civil.minute);
}

static void a_day_past_the_end_of_the_month_is_pulled_back(void)
{
    Screen *screen = open_screen(running_chip());
    adjust(screen, FIELD_DAY, 26);
    CHECK_EQUAL(31, date_time.civil.day);
    tap(screen, KEY_BACK);
    CHECK_EQUAL(30, fake_hal_time().day);
    CHECK_EQUAL(11, fake_hal_time().month);
}

static void without_a_chip_the_toast_says_the_time_lasts_until_power_off(void)
{
    Screen *screen = open_screen((HalTime){.present = false});
    adjust(screen, FIELD_HOUR, 9);
    tap(screen, KEY_BACK);
    CHECK(toast.visible && strcmp("Clock set until the player is off", toast.text) == 0);
    CHECK(strcmp("2026-01-01 09:00", date_time_screen_summary(&date_time)) == 0);
}

static void a_long_back_leaves_for_home_and_saves(void)
{
    Screen *screen = open_screen(running_chip());
    adjust(screen, FIELD_HOUR, 1);
    ScreenAction action = press(screen, KEY_BACK, KEY_GESTURE_LONG_PRESS);
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, action.type);
    CHECK_EQUAL(1, fake_hal_time_write_count());
}

static void the_screen_draws_every_field_and_the_hint(void)
{
    Screen *screen = open_screen(running_chip());
    static Color pixels[HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT];
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT});
    canvas_clear(&canvas, DEFAULT_THEME.colors.background);
    screen->draw(screen->context, &canvas);
    Rect content = theme_content_area(&DEFAULT_THEME);
    CHECK_EQUAL(DEFAULT_THEME.colors.header_top,
                canvas_pixel_at(&canvas, content.x + content.width / 2, content.y + 2));
    tap(screen, KEY_PLAY);
    screen->draw(screen->context, &canvas);
    CHECK_EQUAL(DEFAULT_THEME.colors.selection_background,
                canvas_pixel_at(&canvas, content.x + content.width / 2, content.y + 2));
    bool text_drawn = false;
    for (int y = content.y; y < content.y + content.height; y++) {
        for (int x = content.x; x < content.x + content.width; x++) {
            text_drawn |= canvas_pixel_at(&canvas, x, y) == DEFAULT_THEME.colors.text;
        }
    }
    CHECK(text_drawn);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_editor_starts_from_the_clock_when_it_is_known),
        TEST_CASE(the_editor_starts_from_a_fixed_date_when_the_clock_is_unknown),
        TEST_CASE(volume_keys_move_between_fields_and_wrap),
        TEST_CASE(play_adjusts_the_chosen_field_and_values_wrap),
        TEST_CASE(back_while_adjusting_only_stops_adjusting),
        TEST_CASE(leaving_without_a_change_sets_nothing),
        TEST_CASE(leaving_after_a_change_sets_the_clock_the_chip_and_the_zone),
        TEST_CASE(a_day_past_the_end_of_the_month_is_pulled_back),
        TEST_CASE(without_a_chip_the_toast_says_the_time_lasts_until_power_off),
        TEST_CASE(a_long_back_leaves_for_home_and_saves),
        TEST_CASE(the_screen_draws_every_field_and_the_hint),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
