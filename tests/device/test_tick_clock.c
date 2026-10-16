#include "check.h"
#include "device/tick_clock.h"

enum {
    TICKS_PER_MICROSECOND = 24,
    PERIOD = TICKS_PER_MICROSECOND * 1000
};

static TickClock started_clock(void)
{
    TickClock clock;
    tick_clock_init(&clock, TICKS_PER_MICROSECOND, PERIOD);
    return clock;
}

static TickReading counter_at(uint32_t counter)
{
    return (TickReading){.counter = counter, .reload_pending = false};
}

static TickReading pending_at(uint32_t counter)
{
    return (TickReading){.counter = counter, .reload_pending = true};
}

static void a_new_clock_reads_zero(void)
{
    TickClock clock = started_clock();
    CHECK_EQUAL(0, tick_clock_milliseconds(&clock, counter_at(0)));
    CHECK_EQUAL(0, tick_clock_microseconds(&clock, counter_at(0)));
}

static void the_down_counter_advances_within_a_period(void)
{
    TickClock clock = started_clock();
    CHECK_EQUAL(500, tick_clock_microseconds(&clock, counter_at(PERIOD - 12000)));
    CHECK_EQUAL(0, tick_clock_milliseconds(&clock, counter_at(PERIOD - 12000)));
    CHECK_EQUAL(999, tick_clock_microseconds(&clock, counter_at(1)));
}

static void each_reload_adds_a_full_period(void)
{
    TickClock clock = started_clock();
    tick_clock_reload(&clock);
    CHECK_EQUAL(1000, tick_clock_microseconds(&clock, counter_at(0)));
    CHECK_EQUAL(1, tick_clock_milliseconds(&clock, counter_at(0)));
    CHECK_EQUAL(1500, tick_clock_microseconds(&clock, counter_at(PERIOD - 12000)));
}

static void reloads_with_no_reads_between_still_count_a_millisecond_each(void)
{
    TickClock clock = started_clock();
    for (int reload = 0; reload < 1000; reload++) {
        tick_clock_reload(&clock);
        uint32_t entry_delay = reload % 2 == 0 ? 30 : 10;
        CHECK(tick_clock_milliseconds(&clock, counter_at(PERIOD - 1 - entry_delay)) ==
              (uint32_t)reload + 1);
    }
    CHECK_EQUAL(1000, tick_clock_milliseconds(&clock, counter_at(0)));
    CHECK_EQUAL(1000000, tick_clock_microseconds(&clock, counter_at(0)));
}

static void a_pending_reload_counts_before_its_interrupt_runs(void)
{
    TickClock clock = started_clock();
    CHECK_EQUAL(1001, tick_clock_microseconds(&clock, pending_at(PERIOD - 24)));
    CHECK_EQUAL(1000, tick_clock_microseconds(&clock, pending_at(0)));
    tick_clock_reload(&clock);
    CHECK_EQUAL(1001, tick_clock_microseconds(&clock, counter_at(PERIOD - 24)));
}

static void partial_ticks_carry_across_a_new_rate(void)
{
    TickClock clock = started_clock();
    tick_clock_set_rate(&clock, counter_at(PERIOD - 12), 96, 96000);
    CHECK_EQUAL(0, tick_clock_microseconds(&clock, counter_at(0)));
    CHECK_EQUAL(1, tick_clock_microseconds(&clock, counter_at(96000 - 48)));
}

static void a_new_rate_keeps_the_time_counted_so_far(void)
{
    TickClock clock = started_clock();
    tick_clock_reload(&clock);
    tick_clock_set_rate(&clock, counter_at(PERIOD - 12000), 96, 96000);
    CHECK_EQUAL(1500, tick_clock_microseconds(&clock, counter_at(0)));
    CHECK_EQUAL(2000, tick_clock_microseconds(&clock, counter_at(96000 - 48000)));
    CHECK_EQUAL(2, tick_clock_milliseconds(&clock, counter_at(96000 - 48000)));
    tick_clock_reload(&clock);
    CHECK_EQUAL(2500, tick_clock_microseconds(&clock, counter_at(0)));
}

static void a_new_rate_folds_in_a_pending_reload(void)
{
    TickClock clock = started_clock();
    tick_clock_set_rate(&clock, pending_at(PERIOD - 24), 96, 96000);
    CHECK_EQUAL(1001, tick_clock_microseconds(&clock, counter_at(0)));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_clock_reads_zero),
        TEST_CASE(the_down_counter_advances_within_a_period),
        TEST_CASE(each_reload_adds_a_full_period),
        TEST_CASE(reloads_with_no_reads_between_still_count_a_millisecond_each),
        TEST_CASE(a_pending_reload_counts_before_its_interrupt_runs),
        TEST_CASE(partial_ticks_carry_across_a_new_rate),
        TEST_CASE(a_new_rate_keeps_the_time_counted_so_far),
        TEST_CASE(a_new_rate_folds_in_a_pending_reload),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
