#include "check.h"
#include "device/backlight.h"

static void levels_get_brighter_up_to_full_at_the_top(void)
{
    uint32_t previous = 0;
    for (uint8_t level = HAL_DISPLAY_BRIGHTNESS_MIN; level <= HAL_DISPLAY_BRIGHTNESS_MAX; level++) {
        uint32_t percent = backlight_percent(level);
        CHECK(percent > previous);
        previous = percent;
    }
    CHECK_EQUAL(100u, backlight_percent(HAL_DISPLAY_BRIGHTNESS_MAX));
}

static void levels_outside_the_range_are_clamped(void)
{
    CHECK_EQUAL(backlight_percent(HAL_DISPLAY_BRIGHTNESS_MIN), backlight_percent(0));
    CHECK_EQUAL(100u, backlight_percent(200));
}

static void the_pwm_runs_at_ten_kilohertz_from_pclk(void)
{
    BacklightPwm pwm = backlight_pwm(48000000u, 47);
    CHECK(!pwm.full);
    CHECK_EQUAL(4800u, pwm.period);
    CHECK_EQUAL(2256u, pwm.duty);
    pwm = backlight_pwm(24000000u, 47);
    CHECK_EQUAL(2400u, pwm.period);
    CHECK_EQUAL(1128u, pwm.duty);
}

static void full_brightness_skips_the_pwm(void)
{
    BacklightPwm pwm = backlight_pwm(50000000u, 100);
    CHECK(pwm.full);
}

static void the_fade_in_rises_from_dim_to_the_target(void)
{
    CHECK_EQUAL(1u, backlight_ramp_percent(50, 0));
    CHECK_EQUAL(25u, backlight_ramp_percent(50, BACKLIGHT_RAMP_MS / 2));
    CHECK_EQUAL(50u, backlight_ramp_percent(50, BACKLIGHT_RAMP_MS));
    CHECK_EQUAL(50u, backlight_ramp_percent(50, BACKLIGHT_RAMP_MS * 3));
}

static void the_fade_in_to_full_stays_on_the_pwm_until_it_ends(void)
{
    CHECK(backlight_ramp_percent(100, BACKLIGHT_RAMP_MS - 1) < 100u);
    CHECK_EQUAL(100u, backlight_ramp_percent(100, BACKLIGHT_RAMP_MS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(levels_get_brighter_up_to_full_at_the_top),
        TEST_CASE(levels_outside_the_range_are_clamped),
        TEST_CASE(the_pwm_runs_at_ten_kilohertz_from_pclk),
        TEST_CASE(full_brightness_skips_the_pwm),
        TEST_CASE(the_fade_in_rises_from_dim_to_the_target),
        TEST_CASE(the_fade_in_to_full_stays_on_the_pwm_until_it_ends),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
