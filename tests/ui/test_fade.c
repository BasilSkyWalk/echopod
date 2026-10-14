#include "check.h"
#include "ui/fade.h"

enum {
    START_MS = 1000,
    FADE_MS = 600,
    HOLD_MS = 400
};

static Fade rising(void)
{
    Fade fade;
    fade_start(&fade, START_MS, (FadeTiming){FADE_MS, HOLD_MS, true});
    return fade;
}

static Fade falling(void)
{
    Fade fade;
    fade_start(&fade, START_MS, (FadeTiming){FADE_MS, HOLD_MS, false});
    return fade;
}

static void a_rising_fade_goes_from_clear_to_opaque(void)
{
    Fade fade = rising();
    CHECK_EQUAL(0, fade_alpha(&fade, START_MS));
    CHECK_EQUAL(7, fade_alpha(&fade, START_MS + FADE_MS / 2));
    CHECK_EQUAL(FADE_OPAQUE, fade_alpha(&fade, START_MS + FADE_MS));
    CHECK_EQUAL(FADE_OPAQUE, fade_alpha(&fade, START_MS + FADE_MS + HOLD_MS - 1));
}

static void a_rising_fade_finishes_after_its_hold(void)
{
    Fade fade = rising();
    CHECK(!fade_finished(&fade, START_MS + FADE_MS + HOLD_MS - 1));
    CHECK(fade_finished(&fade, START_MS + FADE_MS + HOLD_MS));
}

static void a_falling_fade_holds_first_then_clears(void)
{
    Fade fade = falling();
    CHECK_EQUAL(FADE_OPAQUE, fade_alpha(&fade, START_MS));
    CHECK_EQUAL(FADE_OPAQUE, fade_alpha(&fade, START_MS + HOLD_MS));
    CHECK_EQUAL(8, fade_alpha(&fade, START_MS + HOLD_MS + FADE_MS / 2));
    CHECK_EQUAL(0, fade_alpha(&fade, START_MS + HOLD_MS + FADE_MS));
    CHECK(fade_finished(&fade, START_MS + HOLD_MS + FADE_MS));
}

static void a_fade_that_was_never_started_is_finished(void)
{
    Fade fade = {.active = false};
    CHECK(fade_finished(&fade, START_MS));
}

static void a_fade_survives_the_clock_wrapping(void)
{
    Fade fade;
    fade_start(&fade, UINT32_MAX - 100, (FadeTiming){FADE_MS, HOLD_MS, true});
    CHECK_EQUAL(FADE_OPAQUE, fade_alpha(&fade, (uint32_t)(FADE_MS - 101)));
    CHECK(!fade_finished(&fade, 10));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_rising_fade_goes_from_clear_to_opaque),
        TEST_CASE(a_rising_fade_finishes_after_its_hold),
        TEST_CASE(a_falling_fade_holds_first_then_clears),
        TEST_CASE(a_fade_that_was_never_started_is_finished),
        TEST_CASE(a_fade_survives_the_clock_wrapping),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
