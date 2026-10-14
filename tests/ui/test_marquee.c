#include "check.h"
#include "ui/marquee.h"

enum {
    LONG_TEXT = 200,
    VIEW = 100,
    PAUSE_MS = 1000,
    SPEED = 30,
    GAP = 20,
    SCROLL_MS = (LONG_TEXT + GAP) * 1000 / SPEED,
    CYCLE_MS = PAUSE_MS + SCROLL_MS
};

static const MarqueeTiming TIMING = {.pause_ms = PAUSE_MS, .speed_px_per_s = SPEED, .gap_px = GAP};

static void text_that_fits_never_scrolls(void)
{
    CHECK_EQUAL(0, marquee_offset(TIMING, 50, VIEW, 5000));
    CHECK_EQUAL(0, marquee_offset(TIMING, VIEW, VIEW, 5000));
}

static void scrolling_waits_for_the_pause(void)
{
    CHECK_EQUAL(0, marquee_offset(TIMING, LONG_TEXT, VIEW, PAUSE_MS - 1));
}

static void scrolling_moves_at_the_configured_speed(void)
{
    CHECK_EQUAL(30, marquee_offset(TIMING, LONG_TEXT, VIEW, PAUSE_MS + 1000));
}

static void a_full_cycle_returns_to_the_start_and_pauses_again(void)
{
    CHECK_EQUAL(0, marquee_offset(TIMING, LONG_TEXT, VIEW, CYCLE_MS));
    CHECK_EQUAL(0, marquee_offset(TIMING, LONG_TEXT, VIEW, CYCLE_MS + PAUSE_MS - 1));
    CHECK_EQUAL(30, marquee_offset(TIMING, LONG_TEXT, VIEW, CYCLE_MS + PAUSE_MS + 1000));
}

static void the_offset_stays_within_one_text_and_gap(void)
{
    int offset = marquee_offset(TIMING, LONG_TEXT, VIEW, CYCLE_MS - 1);
    CHECK(offset > 0);
    CHECK(offset < LONG_TEXT + GAP);
}

static void a_zero_speed_never_scrolls(void)
{
    MarqueeTiming still = {.pause_ms = PAUSE_MS, .speed_px_per_s = 0, .gap_px = GAP};
    CHECK_EQUAL(0, marquee_offset(still, LONG_TEXT, VIEW, 5000));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(text_that_fits_never_scrolls),
        TEST_CASE(scrolling_waits_for_the_pause),
        TEST_CASE(scrolling_moves_at_the_configured_speed),
        TEST_CASE(a_full_cycle_returns_to_the_start_and_pauses_again),
        TEST_CASE(the_offset_stays_within_one_text_and_gap),
        TEST_CASE(a_zero_speed_never_scrolls),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
