#include <string.h>

#include "app/app.h"
#include "check.h"
#include "fake_hal.h"
#include "frame_overlay.h"
#include "hal/display.h"

enum {
    START_MS = 1000,
    KEY_HOLD_MS = 30,
    FULL_FRAME_PIXELS = HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT,
    HOME_DIAGNOSTICS_INDEX = 3,
    DIAGNOSTICS_FRAME_TIME_INDEX = 3
};

static uint32_t now_ms;

static void start_app(void)
{
    fake_hal_reset();
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void step(void)
{
    fake_hal_set_milliseconds(now_ms);
    app_step();
}

static void tap(Key key)
{
    fake_hal_queue_transition(key, true, now_ms);
    fake_hal_queue_transition(key, false, now_ms + KEY_HOLD_MS);
    now_ms += KEY_HOLD_MS + 10;
    step();
}

static void hold_back(void)
{
    fake_hal_queue_transition(KEY_BACK, true, now_ms);
    now_ms += 1000;
    fake_hal_queue_transition(KEY_BACK, false, now_ms);
    step();
}

static void move_down(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_DOWN);
    }
}

static bool title_is(const char *expected)
{
    return strcmp(expected, app_screen_title()) == 0;
}

static bool overlay_visible(void)
{
    Rect area = frame_overlay_area();
    return fake_hal_pixel(area.x, area.y) == frame_overlay_background();
}

static void a_step_writes_one_full_frame_and_presents_it(void)
{
    start_app();
    step();
    CHECK_EQUAL(FULL_FRAME_PIXELS, fake_hal_pixels_written());
    CHECK_EQUAL(1, fake_hal_present_count());
}

static void the_home_menu_is_the_root(void)
{
    start_app();
    step();
    CHECK(title_is("Echo Mini"));
    CHECK_EQUAL(1, app_screen_depth());
}

static void selecting_music_opens_the_music_menu(void)
{
    start_app();
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    CHECK_EQUAL(2, app_screen_depth());
}

static void back_returns_to_the_previous_screen(void)
{
    start_app();
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(title_is("Echo Mini"));
}

static void a_long_back_returns_home_from_deep_screens(void)
{
    start_app();
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK_EQUAL(3, app_screen_depth());
    hold_back();
    CHECK_EQUAL(1, app_screen_depth());
}

static void the_frame_overlay_is_off_until_enabled_in_diagnostics(void)
{
    start_app();
    step();
    CHECK(!overlay_visible());
    move_down(HOME_DIAGNOSTICS_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Diagnostics"));
    move_down(DIAGNOSTICS_FRAME_TIME_INDEX);
    tap(KEY_PLAY);
    CHECK(overlay_visible());
}

static void each_step_records_its_duration(void)
{
    start_app();
    fake_hal_set_microseconds_per_read(4000);
    step();
    CHECK_EQUAL(4000, frame_stats_latest_us(app_frame_stats()));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_step_writes_one_full_frame_and_presents_it),
        TEST_CASE(the_home_menu_is_the_root),
        TEST_CASE(selecting_music_opens_the_music_menu),
        TEST_CASE(back_returns_to_the_previous_screen),
        TEST_CASE(a_long_back_returns_home_from_deep_screens),
        TEST_CASE(the_frame_overlay_is_off_until_enabled_in_diagnostics),
        TEST_CASE(each_step_records_its_duration),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
