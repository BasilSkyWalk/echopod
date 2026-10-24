#include "check.h"
#include "device/audio_math.h"

static void the_i2c_divider_matches_the_boot_clock_value(void)
{
    CHECK_EQUAL(0x000E000Eu, i2c_clock_divider(24000000, 100));
}

static void the_i2c_divider_follows_pclk(void)
{
    CHECK_EQUAL(0x001D001Du, i2c_clock_divider(48000000, 100));
    CHECK_EQUAL(0x001F001Eu, i2c_clock_divider(50000000, 100));
}

static void the_i2s_divider_truncates_or_rounds(void)
{
    CHECK_EQUAL(2, i2s_divider_truncated(11288960, 44100));
    CHECK_EQUAL(3, i2s_divider_rounded(11288960, 44100));
    CHECK_EQUAL(3, i2s_divider_truncated(12288000, 48000));
    CHECK_EQUAL(3, i2s_divider_rounded(12288000, 48000));
}

static void the_i2s_clock_register_holds_the_divider(void)
{
    CHECK_EQUAL(0x00023F3Fu, i2s_clock_register(2));
    CHECK_EQUAL(0x00033F3Fu, i2s_clock_register(3));
}

static void one_sine_period_starts_at_zero_and_peaks_at_a_quarter(void)
{
    int16_t samples[100];
    tone_fill(samples, 100, 3000);
    CHECK_EQUAL(0, samples[0]);
    CHECK(samples[25] >= 2990 && samples[25] <= 3000);
    CHECK(samples[50] >= -10 && samples[50] <= 10);
    CHECK(samples[75] <= -2990 && samples[75] >= -3000);
}

static void the_sine_rises_then_falls(void)
{
    int16_t samples[100];
    tone_fill(samples, 100, 3000);
    for (int index = 1; index <= 25; index++) {
        CHECK(samples[index] > samples[index - 1]);
    }
    for (int index = 26; index <= 75; index++) {
        CHECK(samples[index] < samples[index - 1]);
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_i2c_divider_matches_the_boot_clock_value),
        TEST_CASE(the_i2c_divider_follows_pclk),
        TEST_CASE(the_i2s_divider_truncates_or_rounds),
        TEST_CASE(the_i2s_clock_register_holds_the_divider),
        TEST_CASE(one_sine_period_starts_at_zero_and_peaks_at_a_quarter),
        TEST_CASE(the_sine_rises_then_falls),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
