#include "check.h"
#include "core/low_battery.h"

static LowBatteryReading on_battery(uint32_t millivolts)
{
    return (LowBatteryReading){.known = true, .millivolts = millivolts, .usb_power = false};
}

static void a_reading_below_empty_for_two_seconds_asks_for_shutdown(void)
{
    LowBattery low;
    low_battery_init(&low);
    CHECK(!low_battery_update(&low, on_battery(3440), 1000));
    CHECK(!low_battery_update(&low, on_battery(3440), 2999));
    CHECK(low_battery_update(&low, on_battery(3440), 3000));
}

static void a_reading_back_above_empty_restarts_the_wait(void)
{
    LowBattery low;
    low_battery_init(&low);
    low_battery_update(&low, on_battery(3440), 1000);
    CHECK(!low_battery_update(&low, on_battery(3450), 2500));
    CHECK(!low_battery_update(&low, on_battery(3440), 3000));
    CHECK(!low_battery_update(&low, on_battery(3440), 4999));
    CHECK(low_battery_update(&low, on_battery(3440), 5000));
}

static void usb_power_never_shuts_down(void)
{
    LowBattery low;
    low_battery_init(&low);
    LowBatteryReading plugged = {.known = true, .millivolts = 3300, .usb_power = true};
    low_battery_update(&low, plugged, 0);
    CHECK(!low_battery_update(&low, plugged, 10000));
}

static void an_unknown_reading_never_shuts_down(void)
{
    LowBattery low;
    low_battery_init(&low);
    LowBatteryReading unknown = {.known = false, .millivolts = 0, .usb_power = false};
    low_battery_update(&low, unknown, 0);
    CHECK(!low_battery_update(&low, unknown, 10000));
}

static void unplugging_restarts_the_wait(void)
{
    LowBattery low;
    low_battery_init(&low);
    low_battery_update(&low, on_battery(3400), 0);
    low_battery_update(
        &low, (LowBatteryReading){.known = true, .millivolts = 3400, .usb_power = true}, 1500);
    CHECK(!low_battery_update(&low, on_battery(3400), 2500));
    CHECK(low_battery_update(&low, on_battery(3400), 4500));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_reading_below_empty_for_two_seconds_asks_for_shutdown),
        TEST_CASE(a_reading_back_above_empty_restarts_the_wait),
        TEST_CASE(usb_power_never_shuts_down),
        TEST_CASE(an_unknown_reading_never_shuts_down),
        TEST_CASE(unplugging_restarts_the_wait),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
