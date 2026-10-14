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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(milliseconds_show_one_decimal),
        TEST_CASE(milliseconds_round_to_the_nearest_tenth),
        TEST_CASE(durations_show_minutes_and_padded_seconds),
        TEST_CASE(durations_round_down_to_whole_seconds),
        TEST_CASE(long_durations_show_hours),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
