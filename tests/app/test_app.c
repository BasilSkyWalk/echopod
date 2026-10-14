#include "app/app.h"
#include "check.h"
#include "fake_hal.h"
#include "hal/display.h"
#include "test_pattern_screen.h"

enum {
    START_MS = 1000,
    FULL_FRAME_PIXELS = HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT
};

static void start_app(void)
{
    fake_hal_reset();
    fake_hal_set_milliseconds(START_MS);
    app_start();
}

static void step_at(uint32_t now_ms)
{
    fake_hal_set_milliseconds(now_ms);
    app_step();
}

static uint16_t indicator_color(Key key)
{
    Rect indicator = test_pattern_key_indicator(key);
    return fake_hal_pixel(indicator.x + indicator.width / 2, indicator.y + indicator.height / 2);
}

static void tap(Key key, uint32_t down_ms, uint32_t up_ms)
{
    fake_hal_queue_transition(key, true, down_ms);
    fake_hal_queue_transition(key, false, up_ms);
}

static void a_step_writes_one_full_frame_and_presents_it(void)
{
    start_app();
    step_at(START_MS);
    CHECK_EQUAL(FULL_FRAME_PIXELS, fake_hal_pixels_written());
    CHECK_EQUAL(1, fake_hal_present_count());
}

static void the_frame_starts_with_the_first_color_bar(void)
{
    start_app();
    step_at(START_MS);
    CHECK_EQUAL(test_pattern_first_bar_color(), fake_hal_pixel(0, 0));
}

static void indicators_start_idle(void)
{
    start_app();
    step_at(START_MS);
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(KEY_PLAY));
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(KEY_POWER));
}

static void a_tapped_key_flashes_its_indicator_with_the_press_color(void)
{
    start_app();
    tap(KEY_PLAY, START_MS, START_MS + 50);
    step_at(START_MS + 60);
    CHECK_EQUAL(test_pattern_gesture_color(KEY_GESTURE_PRESS), indicator_color(KEY_PLAY));
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(KEY_MENU));
}

static void the_flash_fades_back_to_idle(void)
{
    start_app();
    tap(KEY_BACK, START_MS, START_MS + 50);
    step_at(START_MS + 60);
    step_at(START_MS + 60 + TEST_PATTERN_FLASH_MS);
    CHECK_EQUAL(test_pattern_idle_color(), indicator_color(KEY_BACK));
}

static void holding_a_key_shows_the_long_press_color(void)
{
    start_app();
    fake_hal_queue_transition(KEY_MENU, true, START_MS);
    step_at(START_MS + 10);
    step_at(START_MS + 1000);
    CHECK_EQUAL(test_pattern_gesture_color(KEY_GESTURE_LONG_PRESS), indicator_color(KEY_MENU));
}

static void holding_a_volume_key_shows_the_repeat_color(void)
{
    start_app();
    fake_hal_queue_transition(KEY_VOLUME_UP, true, START_MS);
    step_at(START_MS + 10);
    step_at(START_MS + 1000);
    CHECK_EQUAL(test_pattern_gesture_color(KEY_GESTURE_REPEAT), indicator_color(KEY_VOLUME_UP));
}

static void each_step_records_its_duration(void)
{
    start_app();
    fake_hal_set_microseconds_per_read(4000);
    step_at(START_MS);
    CHECK_EQUAL(4000, frame_stats_latest_us(app_frame_stats()));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_step_writes_one_full_frame_and_presents_it),
        TEST_CASE(the_frame_starts_with_the_first_color_bar),
        TEST_CASE(indicators_start_idle),
        TEST_CASE(a_tapped_key_flashes_its_indicator_with_the_press_color),
        TEST_CASE(the_flash_fades_back_to_idle),
        TEST_CASE(holding_a_key_shows_the_long_press_color),
        TEST_CASE(holding_a_volume_key_shows_the_repeat_color),
        TEST_CASE(each_step_records_its_duration),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
