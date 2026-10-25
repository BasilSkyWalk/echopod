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

static void the_average_spans_several_seconds_of_readings(void)
{
    CHECK(BATTERY_GAUGE_READINGS >= 64);
}

static void the_shown_percent_starts_at_the_first_level(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    CHECK_EQUAL(34, battery_display_update(&display, 34, false));
}

static void a_one_point_wobble_keeps_the_shown_percent(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, false);
    CHECK_EQUAL(34, battery_display_update(&display, 33, false));
    CHECK_EQUAL(34, battery_display_update(&display, 35, false));
    CHECK_EQUAL(34, battery_display_update(&display, 34, false));
}

static void on_battery_the_shown_percent_falls_freely_and_rises_reluctantly(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, false);
    CHECK_EQUAL(32, battery_display_update(&display, 32, false));
    CHECK_EQUAL(32, battery_display_update(&display, 36, false));
    CHECK_EQUAL(42, battery_display_update(&display, 42, false));
}

static void on_usb_power_the_shown_percent_rises_freely_and_falls_reluctantly(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 34, true);
    CHECK_EQUAL(36, battery_display_update(&display, 36, true));
    CHECK_EQUAL(36, battery_display_update(&display, 30, true));
    CHECK_EQUAL(26, battery_display_update(&display, 26, true));
}

static void plugging_in_does_not_drop_the_shown_percent(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 60, false);
    CHECK_EQUAL(60, battery_display_update(&display, 53, true));
    CHECK_EQUAL(60, battery_display_update(&display, 59, true));
    CHECK_EQUAL(62, battery_display_update(&display, 62, true));
}

static void unplugging_does_not_raise_the_shown_percent(void)
{
    BatteryDisplay display;
    battery_display_init(&display);
    battery_display_update(&display, 70, true);
    CHECK_EQUAL(70, battery_display_update(&display, 75, false));
    CHECK_EQUAL(68, battery_display_update(&display, 68, false));
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
        TEST_CASE(the_average_spans_several_seconds_of_readings),
        TEST_CASE(the_shown_percent_starts_at_the_first_level),
        TEST_CASE(a_one_point_wobble_keeps_the_shown_percent),
        TEST_CASE(on_battery_the_shown_percent_falls_freely_and_rises_reluctantly),
        TEST_CASE(on_usb_power_the_shown_percent_rises_freely_and_falls_reluctantly),
        TEST_CASE(plugging_in_does_not_drop_the_shown_percent),
        TEST_CASE(unplugging_does_not_raise_the_shown_percent),
        TEST_CASE(a_boot_below_3400_mv_on_battery_switches_off),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
