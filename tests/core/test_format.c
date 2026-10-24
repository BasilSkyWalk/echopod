#include <string.h>

#include "check.h"
#include "core/format.h"

enum {
    CAPACITY = 16
};

static bool formats_as(uint32_t microseconds, const char *expected)
{
    char buffer[CAPACITY];
    format_milliseconds(microseconds, buffer, sizeof buffer);
    return strcmp(expected, buffer) == 0;
}

static void milliseconds_show_one_decimal(void)
{
    CHECK(formats_as(16700, "16.7 ms"));
    CHECK(formats_as(0, "0.0 ms"));
}

static void milliseconds_round_to_the_nearest_tenth(void)
{
    CHECK(formats_as(1649, "1.6 ms"));
    CHECK(formats_as(1650, "1.7 ms"));
    CHECK(formats_as(9950, "10.0 ms"));
}

static bool duration_formats_as(uint32_t milliseconds, const char *expected)
{
    char buffer[CAPACITY];
    format_duration(milliseconds, buffer, sizeof buffer);
    return strcmp(expected, buffer) == 0;
}

static void durations_show_minutes_and_padded_seconds(void)
{
    CHECK(duration_formats_as(312800, "5:12"));
    CHECK(duration_formats_as(65000, "1:05"));
    CHECK(duration_formats_as(0, "0:00"));
}

static void durations_round_down_to_whole_seconds(void)
{
    CHECK(duration_formats_as(59999, "0:59"));
}

static void long_durations_show_hours(void)
{
    CHECK(duration_formats_as(3600000, "1:00:00"));
    CHECK(duration_formats_as(4384000, "1:13:04"));
}

static bool text_is(const char *buffer, const char *expected)
{
    return strcmp(expected, buffer) == 0;
}

static bool rate_formats_as(uint32_t hertz, const char *expected)
{
    char buffer[CAPACITY];
    format_sample_rate(hertz, buffer, sizeof buffer);
    return text_is(buffer, expected);
}

static bool bitrate_formats_as(uint32_t kbps, const char *expected)
{
    char buffer[CAPACITY];
    format_bitrate(kbps, buffer, sizeof buffer);
    return text_is(buffer, expected);
}

static void sample_rates_show_kilohertz_with_a_decimal_only_when_needed(void)
{
    CHECK(rate_formats_as(44100, "44.1 kHz"));
    CHECK(rate_formats_as(48000, "48 kHz"));
    CHECK(rate_formats_as(88200, "88.2 kHz"));
    CHECK(rate_formats_as(96000, "96 kHz"));
    CHECK(rate_formats_as(192000, "192 kHz"));
}

static void bitrates_group_thousands(void)
{
    CHECK(bitrate_formats_as(320, "320 kbps"));
    CHECK(bitrate_formats_as(2304, "2,304 kbps"));
    CHECK(bitrate_formats_as(1000, "1,000 kbps"));
    CHECK(bitrate_formats_as(12045, "12,045 kbps"));
}

static void the_average_bitrate_comes_from_audio_bytes_and_length(void)
{
    CHECK_EQUAL(320, average_kbps(2400000, 2646000, 44100));
    CHECK_EQUAL(2304, average_kbps(345600000, 115200000, 96000));
}

static void an_unknown_length_has_no_bitrate(void)
{
    CHECK_EQUAL(0, average_kbps(2400000, 0, 44100));
    CHECK_EQUAL(0, average_kbps(2400000, 2646000, 0));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(milliseconds_show_one_decimal),
        TEST_CASE(milliseconds_round_to_the_nearest_tenth),
        TEST_CASE(durations_show_minutes_and_padded_seconds),
        TEST_CASE(durations_round_down_to_whole_seconds),
        TEST_CASE(long_durations_show_hours),
        TEST_CASE(sample_rates_show_kilohertz_with_a_decimal_only_when_needed),
        TEST_CASE(bitrates_group_thousands),
        TEST_CASE(the_average_bitrate_comes_from_audio_bytes_and_length),
        TEST_CASE(an_unknown_length_has_no_bitrate),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
