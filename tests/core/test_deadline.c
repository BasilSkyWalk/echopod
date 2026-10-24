#include "check.h"
#include "core/deadline.h"

static void a_deadline_is_reached_at_its_exact_time(void)
{
    CHECK(deadline_reached(5000, 5000));
}

static void a_deadline_is_not_reached_before_its_time(void)
{
    CHECK(!deadline_reached(4999, 5000));
}

static void a_deadline_stays_reached_after_its_time(void)
{
    CHECK(deadline_reached(9000, 5000));
}

static void a_deadline_past_the_clock_wrap_is_not_reached_before_the_wrap(void)
{
    uint32_t now_ms = UINT32_MAX - 50;
    uint32_t deadline_ms = now_ms + 300;
    CHECK(!deadline_reached(now_ms, deadline_ms));
    CHECK(deadline_reached(deadline_ms, deadline_ms));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_deadline_is_reached_at_its_exact_time),
        TEST_CASE(a_deadline_is_not_reached_before_its_time),
        TEST_CASE(a_deadline_stays_reached_after_its_time),
        TEST_CASE(a_deadline_past_the_clock_wrap_is_not_reached_before_the_wrap),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
