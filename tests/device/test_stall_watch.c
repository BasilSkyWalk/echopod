#include "check.h"
#include "device/stall_watch.h"

static void a_loop_that_keeps_beating_never_trips(void)
{
    StallWatch watch;
    stall_watch_init(&watch, 0);
    for (uint32_t now = 0; now < 60000; now += 500) {
        stall_watch_beat(&watch, now);
        CHECK(!stall_watch_check(&watch, now + 400));
    }
}

static void a_silent_loop_trips_once_after_the_limit(void)
{
    StallWatch watch;
    stall_watch_init(&watch, 1000);
    CHECK(!stall_watch_check(&watch, 1000 + STALL_WATCH_LIMIT_MS - 1));
    CHECK(stall_watch_check(&watch, 1000 + STALL_WATCH_LIMIT_MS));
    CHECK(!stall_watch_check(&watch, 1000 + 2 * STALL_WATCH_LIMIT_MS));
}

static void a_beat_after_a_trip_arms_it_again(void)
{
    StallWatch watch;
    stall_watch_init(&watch, 0);
    CHECK(stall_watch_check(&watch, STALL_WATCH_LIMIT_MS));
    stall_watch_beat(&watch, STALL_WATCH_LIMIT_MS + 5);
    CHECK(!stall_watch_check(&watch, STALL_WATCH_LIMIT_MS + 6));
    CHECK(stall_watch_check(&watch, 2 * STALL_WATCH_LIMIT_MS + 5));
}

static void a_paused_watch_never_trips_and_restarts_its_count_when_resumed(void)
{
    StallWatch watch;
    stall_watch_init(&watch, 0);
    stall_watch_pause(&watch, true, 100);
    CHECK(!stall_watch_check(&watch, 5 * STALL_WATCH_LIMIT_MS));
    stall_watch_pause(&watch, false, 5 * STALL_WATCH_LIMIT_MS);
    CHECK(!stall_watch_check(&watch, 6 * STALL_WATCH_LIMIT_MS - 1));
    CHECK(stall_watch_check(&watch, 6 * STALL_WATCH_LIMIT_MS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_loop_that_keeps_beating_never_trips),
        TEST_CASE(a_silent_loop_trips_once_after_the_limit),
        TEST_CASE(a_beat_after_a_trip_arms_it_again),
        TEST_CASE(a_paused_watch_never_trips_and_restarts_its_count_when_resumed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
