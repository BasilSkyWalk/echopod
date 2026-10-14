#include "check.h"
#include "device/tick_clock.h"

enum {
    TICKS_PER_MICROSECOND = 24,
    COUNTER_TOP = 0xFFFFFF
};

static TickClock started_clock(uint32_t counter)
{
    TickClock clock;
    tick_clock_init(&clock, TICKS_PER_MICROSECOND, counter);
    return clock;
}

static void a_new_clock_reads_zero(void)
{
    TickClock clock = started_clock(COUNTER_TOP);
    CHECK_EQUAL(0, tick_clock_milliseconds(&clock));
    CHECK_EQUAL(0, tick_clock_microseconds(&clock));
}

static void the_down_counter_advances_the_clock(void)
{
    TickClock clock = started_clock(COUNTER_TOP);
    tick_clock_update(&clock, COUNTER_TOP - 24000);
    CHECK_EQUAL(1, tick_clock_milliseconds(&clock));
    CHECK_EQUAL(1000, tick_clock_microseconds(&clock));
}

static void a_counter_wrap_is_counted(void)
{
    TickClock clock = started_clock(100);
    tick_clock_update(&clock, COUNTER_TOP - 99);
    CHECK_EQUAL(200 / TICKS_PER_MICROSECOND, tick_clock_microseconds(&clock));
}

static void partial_ticks_carry_over_between_updates(void)
{
    TickClock clock = started_clock(COUNTER_TOP);
    for (uint32_t step = 1; step <= 3; step++) {
        tick_clock_update(&clock, COUNTER_TOP - step * 12);
    }
    CHECK_EQUAL(1, tick_clock_microseconds(&clock));
    tick_clock_update(&clock, COUNTER_TOP - 48);
    CHECK_EQUAL(2, tick_clock_microseconds(&clock));
}

static void many_wraps_add_up_to_whole_seconds(void)
{
    TickClock clock = started_clock(COUNTER_TOP);
    uint32_t counter = COUNTER_TOP;
    for (int step = 0; step < 3000; step++) {
        counter = (counter - 8000) & COUNTER_TOP;
        tick_clock_update(&clock, counter);
    }
    CHECK_EQUAL(1000, tick_clock_milliseconds(&clock));
    CHECK_EQUAL(1000000, tick_clock_microseconds(&clock));
}

static void a_new_rate_keeps_the_time_counted_so_far(void)
{
    TickClock clock = started_clock(COUNTER_TOP);
    tick_clock_update(&clock, COUNTER_TOP - 24 * 1500);
    tick_clock_set_rate(&clock, 96, COUNTER_TOP - 24 * 2000);
    CHECK_EQUAL(2000, tick_clock_microseconds(&clock));
    CHECK_EQUAL(2, tick_clock_milliseconds(&clock));
    tick_clock_update(&clock, COUNTER_TOP - 24 * 2000 - 96 * 1000);
    CHECK_EQUAL(3000, tick_clock_microseconds(&clock));
    CHECK_EQUAL(3, tick_clock_milliseconds(&clock));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_clock_reads_zero),
        TEST_CASE(the_down_counter_advances_the_clock),
        TEST_CASE(a_counter_wrap_is_counted),
        TEST_CASE(partial_ticks_carry_over_between_updates),
        TEST_CASE(many_wraps_add_up_to_whole_seconds),
        TEST_CASE(a_new_rate_keeps_the_time_counted_so_far),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
