#include "check.h"
#include "device/sleep_meter.h"

static void a_new_meter_reads_zero(void)
{
    SleepMeter meter;
    sleep_meter_init(&meter, 5000);
    CHECK_EQUAL(0, sleep_meter_percent(&meter));
}

static void the_share_is_published_once_a_window_has_passed(void)
{
    SleepMeter meter;
    sleep_meter_init(&meter, 0);
    sleep_meter_slept(&meter, 600000);
    sleep_meter_advance(&meter, SLEEP_METER_WINDOW_US - 1);
    CHECK_EQUAL(0, sleep_meter_percent(&meter));
    sleep_meter_slept(&meter, 400000);
    sleep_meter_advance(&meter, SLEEP_METER_WINDOW_US);
    CHECK_EQUAL(50, sleep_meter_percent(&meter));
}

static void each_window_starts_afresh(void)
{
    SleepMeter meter;
    sleep_meter_init(&meter, 0);
    sleep_meter_slept(&meter, SLEEP_METER_WINDOW_US);
    sleep_meter_advance(&meter, SLEEP_METER_WINDOW_US);
    CHECK_EQUAL(100, sleep_meter_percent(&meter));
    sleep_meter_advance(&meter, 2 * SLEEP_METER_WINDOW_US);
    CHECK_EQUAL(0, sleep_meter_percent(&meter));
}

static void the_share_never_exceeds_100_and_survives_a_wrapped_clock(void)
{
    SleepMeter meter;
    sleep_meter_init(&meter, UINT32_MAX - 1000);
    sleep_meter_slept(&meter, 3 * SLEEP_METER_WINDOW_US);
    sleep_meter_advance(&meter, SLEEP_METER_WINDOW_US);
    CHECK_EQUAL(100, sleep_meter_percent(&meter));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_meter_reads_zero),
        TEST_CASE(the_share_is_published_once_a_window_has_passed),
        TEST_CASE(each_window_starts_afresh),
        TEST_CASE(the_share_never_exceeds_100_and_survives_a_wrapped_clock),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
