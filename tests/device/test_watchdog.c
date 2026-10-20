#include "check.h"
#include "device/watchdog.h"

static void the_timeout_is_at_least_eight_seconds_at_each_clock(void)
{
    CHECK_EQUAL(12, watchdog_top_for(24000000u));
    CHECK_EQUAL(13, watchdog_top_for(50000000u));
    CHECK_EQUAL(14, watchdog_top_for(100000000u));
    CHECK_EQUAL(15, watchdog_top_for(200000000u));
}

static void a_clock_too_fast_for_eight_seconds_gets_the_longest_timeout(void)
{
    CHECK_EQUAL(WATCHDOG_TOP_MAX, watchdog_top_for(400000000u));
}

static void a_slow_clock_never_goes_below_the_shortest_timeout(void)
{
    CHECK_EQUAL(0, watchdog_top_for(1000u));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_timeout_is_at_least_eight_seconds_at_each_clock),
        TEST_CASE(a_clock_too_fast_for_eight_seconds_gets_the_longest_timeout),
        TEST_CASE(a_slow_clock_never_goes_below_the_shortest_timeout),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
