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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(milliseconds_show_one_decimal),
        TEST_CASE(milliseconds_round_to_the_nearest_tenth),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
