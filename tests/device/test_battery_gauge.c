#include "check.h"
#include "device/battery_gauge.h"

static uint16_t raw_for(uint32_t millivolts)
{
    return (uint16_t)((millivolts * 1024u + 4999u) / 5000u);
}

static void readings_scale_to_millivolts_through_the_divider(void)
{
    CHECK_EQUAL(0u, battery_millivolts(0));
    CHECK_EQUAL(2500u, battery_millivolts(512));
    CHECK_EQUAL(4995u, battery_millivolts(1023));
}

static void the_curve_runs_from_empty_to_full(void)
{
    CHECK_EQUAL(0, battery_percent_for(3000));
    CHECK_EQUAL(0, battery_percent_for(3450));
    CHECK_EQUAL(50, battery_percent_for(3800));
    CHECK_EQUAL(100, battery_percent_for(4200));
    CHECK_EQUAL(100, battery_percent_for(4400));
}

static void the_curve_is_linear_between_points(void)
{
    CHECK_EQUAL(55, battery_percent_for(3825));
    CHECK_EQUAL(5, battery_percent_for(3515));
    CHECK_EQUAL(95, battery_percent_for(4150));
}

static void a_new_gauge_knows_nothing(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    CHECK(!battery_gauge_known(&gauge));
}

static void one_reading_gives_a_level(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    battery_gauge_add(&gauge, raw_for(3800));
    CHECK(battery_gauge_known(&gauge));
    CHECK(battery_gauge_millivolts(&gauge) >= 3795 && battery_gauge_millivolts(&gauge) <= 3805);
    CHECK_EQUAL(50, battery_gauge_percent(&gauge, false));
}

static void a_single_odd_reading_moves_the_level_only_a_little(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    for (int reading = 0; reading < BATTERY_GAUGE_READINGS; reading++) {
        battery_gauge_add(&gauge, raw_for(3800));
    }
    battery_gauge_add(&gauge, raw_for(3450));
    CHECK(battery_gauge_millivolts(&gauge) > 3750);
}

static void charging_reads_50_mv_lower(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    battery_gauge_add(&gauge, raw_for(3850));
    CHECK_EQUAL(50, battery_gauge_percent(&gauge, true));
}

static void charging_never_shows_full(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    battery_gauge_add(&gauge, raw_for(4300));
    CHECK_EQUAL(BATTERY_CHARGING_MAX_PERCENT, battery_gauge_percent(&gauge, true));
    CHECK_EQUAL(100, battery_gauge_percent(&gauge, false));
}

static void a_restart_keeps_only_the_latest_reading(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    for (int reading = 0; reading < BATTERY_GAUGE_READINGS + 3; reading++) {
        battery_gauge_add(&gauge, raw_for(4200));
    }
    battery_gauge_add(&gauge, raw_for(3900));
    battery_gauge_restart(&gauge);
    CHECK(battery_gauge_known(&gauge));
    CHECK(battery_gauge_millivolts(&gauge) >= 3895 && battery_gauge_millivolts(&gauge) <= 3905);
    battery_gauge_add(&gauge, raw_for(3700));
    CHECK(battery_gauge_millivolts(&gauge) >= 3795 && battery_gauge_millivolts(&gauge) <= 3805);
}

static void the_recent_level_follows_the_last_readings(void)
{
    BatteryGauge gauge;
    battery_gauge_init(&gauge);
    for (int reading = 0; reading < BATTERY_GAUGE_READINGS; reading++) {
        battery_gauge_add(&gauge, raw_for(3800));
    }
    for (int reading = 0; reading < BATTERY_RECENT_READINGS; reading++) {
        battery_gauge_add(&gauge, raw_for(3400));
    }
    CHECK(battery_gauge_recent_millivolts(&gauge) <= 3405);
    CHECK(battery_gauge_millivolts(&gauge) > 3700);
}

static void the_average_spans_several_seconds_of_readings(void)
{
    CHECK(BATTERY_GAUGE_READINGS >= 64);
}

enum {
    STEP = BATTERY_DISPLAY_STEP_MS,
    HOLD = BATTERY_DISPLAY_HOLD_MS
};

static void the_shown_percent_starts_at_the_first_level(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    CHECK_EQUAL(34, battery_display_update(&display, 34, false, 1000));
}

static void a_one_point_wobble_keeps_the_shown_percent(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, false, 0);
    CHECK_EQUAL(34, battery_display_update(&display, 33, false, 1000));
    CHECK_EQUAL(34, battery_display_update(&display, 35, false, 1000 + STEP));
    CHECK_EQUAL(34, battery_display_update(&display, 33, false, 2000 + STEP));
    CHECK_EQUAL(34, battery_display_update(&display, 34, false, 3000 + 2 * STEP));
}

static void a_lasting_fall_moves_the_shown_percent_one_point_at_a_time(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, false, 0);
    CHECK_EQUAL(34, battery_display_update(&display, 31, false, 100));
    CHECK_EQUAL(34, battery_display_update(&display, 31, false, 100 + STEP - 1));
    CHECK_EQUAL(33, battery_display_update(&display, 31, false, 100 + STEP));
    CHECK_EQUAL(33, battery_display_update(&display, 31, false, 100 + STEP + 1));
    CHECK_EQUAL(32, battery_display_update(&display, 31, false, 100 + 2 * STEP));
    CHECK_EQUAL(31, battery_display_update(&display, 31, false, 100 + 3 * STEP));
    CHECK_EQUAL(31, battery_display_update(&display, 31, false, 100 + 4 * STEP));
}

static void on_battery_the_shown_percent_rises_only_on_a_big_jump(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, false, 0);
    CHECK_EQUAL(34, battery_display_update(&display, 40, false, STEP));
    CHECK_EQUAL(34, battery_display_update(&display, 40, false, 5 * STEP));
    CHECK_EQUAL(44, battery_display_update(&display, 44, false, 6 * STEP));
}

static void on_usb_power_the_shown_percent_rises_slowly_and_falls_only_on_a_big_jump(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, true, 0);
    CHECK_EQUAL(34, battery_display_update(&display, 36, true, 1));
    CHECK_EQUAL(35, battery_display_update(&display, 36, true, 1 + STEP));
    CHECK_EQUAL(35, battery_display_update(&display, 30, true, 1 + 3 * STEP));
    CHECK_EQUAL(25, battery_display_update(&display, 25, true, 1 + 4 * STEP));
}

static void plugging_in_holds_the_shown_percent_for_half_a_minute(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 60, false, 0);
    CHECK_EQUAL(60, battery_display_update(&display, 75, true, 1000));
    CHECK_EQUAL(60, battery_display_update(&display, 75, true, 1000 + HOLD - 1));
    CHECK_EQUAL(60, battery_display_update(&display, 62, true, 1000 + HOLD));
    CHECK_EQUAL(61, battery_display_update(&display, 62, true, 1000 + HOLD + STEP));
}

static void plugging_in_does_not_drop_the_shown_percent(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 60, false, 0);
    CHECK_EQUAL(60, battery_display_update(&display, 53, true, 100));
    CHECK_EQUAL(60, battery_display_update(&display, 53, true, 100 + HOLD));
    CHECK_EQUAL(60, battery_display_update(&display, 59, true, 100 + HOLD + STEP));
}

static void unplugging_holds_then_does_not_raise_the_shown_percent(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 70, true, 0);
    CHECK_EQUAL(70, battery_display_update(&display, 62, false, 100));
    CHECK_EQUAL(70, battery_display_update(&display, 75, false, 100 + HOLD));
    CHECK_EQUAL(70, battery_display_update(&display, 68, false, 200 + HOLD));
    CHECK_EQUAL(69, battery_display_update(&display, 68, false, 200 + HOLD + STEP));
}

static void the_clock_wrapping_keeps_the_steps(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 50, false, UINT32_MAX - 100);
    battery_display_update(&display, 48, false, UINT32_MAX - 50);
    CHECK_EQUAL(49, battery_display_update(&display, 48, false, STEP));
}

static void a_boot_below_3400_mv_on_battery_switches_off(void)
{
    CHECK(battery_boot_cutoff(3399, false));
    CHECK(!battery_boot_cutoff(3400, false));
    CHECK(!battery_boot_cutoff(3000, true));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(readings_scale_to_millivolts_through_the_divider),
        TEST_CASE(the_curve_runs_from_empty_to_full),
        TEST_CASE(the_curve_is_linear_between_points),
        TEST_CASE(a_new_gauge_knows_nothing),
        TEST_CASE(one_reading_gives_a_level),
        TEST_CASE(a_single_odd_reading_moves_the_level_only_a_little),
        TEST_CASE(charging_reads_50_mv_lower),
        TEST_CASE(charging_never_shows_full),
        TEST_CASE(a_restart_keeps_only_the_latest_reading),
        TEST_CASE(the_recent_level_follows_the_last_readings),
        TEST_CASE(the_average_spans_several_seconds_of_readings),
        TEST_CASE(the_shown_percent_starts_at_the_first_level),
        TEST_CASE(a_one_point_wobble_keeps_the_shown_percent),
        TEST_CASE(a_lasting_fall_moves_the_shown_percent_one_point_at_a_time),
        TEST_CASE(on_battery_the_shown_percent_rises_only_on_a_big_jump),
        TEST_CASE(on_usb_power_the_shown_percent_rises_slowly_and_falls_only_on_a_big_jump),
        TEST_CASE(plugging_in_holds_the_shown_percent_for_half_a_minute),
        TEST_CASE(plugging_in_does_not_drop_the_shown_percent),
        TEST_CASE(unplugging_holds_then_does_not_raise_the_shown_percent),
        TEST_CASE(the_clock_wrapping_keeps_the_steps),
        TEST_CASE(a_boot_below_3400_mv_on_battery_switches_off),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
