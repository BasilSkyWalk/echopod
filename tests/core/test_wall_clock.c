#include <string.h>

#include "check.h"
#include "core/wall_clock.h"

enum {
    SECONDS_PER_DAY = 86400,
    UNIX_2000 = 946684800,
    UNIX_2024_02_29_NOON = 1709208000,
    UNIX_2026_11_05 = 1793836800,
    LAST_DAY_2099_DAYS = 47481
};

static bool text_is(const char *expected, const char *text)
{
    return strcmp(expected, text) == 0;
}

static void days_count_from_the_unix_epoch(void)
{
    CHECK_EQUAL(0, wall_clock_days_from_civil(1970, 1, 1));
    CHECK_EQUAL(UNIX_2000 / SECONDS_PER_DAY, wall_clock_days_from_civil(2000, 1, 1));
    CHECK_EQUAL(UNIX_2026_11_05 / SECONDS_PER_DAY, wall_clock_days_from_civil(2026, 11, 5));
    CHECK_EQUAL(LAST_DAY_2099_DAYS, wall_clock_days_from_civil(2099, 12, 31));
}

static void civil_times_round_trip_through_seconds(void)
{
    WallCivil civil = {2024, 2, 29, 12, 0, 0};
    CHECK_EQUAL(UNIX_2024_02_29_NOON, wall_clock_seconds_of(&civil));
    WallCivil back;
    wall_clock_civil_of(UNIX_2024_02_29_NOON, &back);
    CHECK(memcmp(&civil, &back, sizeof civil) == 0);
    wall_clock_civil_of(UNIX_2024_02_29_NOON + SECONDS_PER_DAY - 1, &back);
    CHECK_EQUAL(2024, back.year);
    CHECK_EQUAL(3, back.month);
    CHECK_EQUAL(1, back.day);
    CHECK_EQUAL(11, back.hour);
    CHECK_EQUAL(59, back.minute);
    CHECK_EQUAL(59, back.second);
}

static void every_day_from_2000_to_2099_round_trips(void)
{
    uint32_t first = wall_clock_days_from_civil(2000, 1, 1);
    uint32_t last = wall_clock_days_from_civil(2099, 12, 31);
    uint32_t mismatches = 0;
    uint32_t leap_days = 0;
    for (uint32_t day = first; day <= last; day++) {
        WallCivil civil;
        wall_clock_civil_of(day * SECONDS_PER_DAY, &civil);
        mismatches += wall_clock_days_from_civil(civil.year, civil.month, civil.day) != day;
        mismatches += !wall_clock_civil_valid(&civil);
        leap_days += civil.month == 2 && civil.day == 29;
    }
    CHECK_EQUAL(0, mismatches);
    CHECK_EQUAL(25, leap_days);
    CHECK_EQUAL(last - first + 1, 36525u);
}

static void the_weekday_counts_sunday_as_zero(void)
{
    CHECK_EQUAL(4, wall_clock_weekday(0));
    CHECK_EQUAL(4, wall_clock_weekday(UNIX_2026_11_05));
    CHECK_EQUAL(6, wall_clock_weekday(UNIX_2000));
    CHECK_EQUAL(0, wall_clock_weekday(UNIX_2026_11_05 + 3 * SECONDS_PER_DAY));
}

static void civil_times_are_checked_against_the_calendar(void)
{
    WallCivil civil = {2026, 4, 30, 23, 59, 59};
    CHECK(wall_clock_civil_valid(&civil));
    civil.day = 31;
    CHECK(!wall_clock_civil_valid(&civil));
    civil = (WallCivil){2100, 2, 28, 0, 0, 0};
    CHECK(!wall_clock_civil_valid(&civil));
    civil = (WallCivil){1999, 12, 31, 0, 0, 0};
    CHECK(!wall_clock_civil_valid(&civil));
    civil = (WallCivil){2025, 2, 29, 0, 0, 0};
    CHECK(!wall_clock_civil_valid(&civil));
    civil = (WallCivil){2024, 2, 29, 24, 0, 0};
    CHECK(!wall_clock_civil_valid(&civil));
    civil.hour = 23;
    CHECK(wall_clock_civil_valid(&civil));
}

static void the_clock_keeps_time_from_its_anchor(void)
{
    WallClock clock = {0};
    CHECK(!clock.known);
    wall_clock_set(&clock, UNIX_2026_11_05, 1000);
    CHECK(clock.known);
    CHECK_EQUAL(UNIX_2026_11_05, wall_clock_local(&clock, 1000));
    CHECK_EQUAL(UNIX_2026_11_05, wall_clock_local(&clock, 1999));
    CHECK_EQUAL(UNIX_2026_11_05 + 90, wall_clock_local(&clock, 91000));
}

static void the_clock_survives_the_millisecond_counter_wrapping(void)
{
    WallClock clock = {0};
    wall_clock_set(&clock, UNIX_2026_11_05, UINT32_MAX - 4999);
    CHECK_EQUAL(UNIX_2026_11_05 + 10, wall_clock_local(&clock, 5000));
}

static void unix_time_needs_a_known_clock_and_a_zone(void)
{
    WallClock clock = {0};
    uint32_t unix_time = 1;
    CHECK(!wall_clock_unix(&clock, 0, &unix_time));
    wall_clock_set(&clock, UNIX_2026_11_05, 0);
    CHECK(!wall_clock_unix(&clock, 0, &unix_time));
    clock.zone = wall_clock_zone_of(330);
    CHECK(wall_clock_unix(&clock, 0, &unix_time));
    CHECK_EQUAL(UNIX_2026_11_05 - 330 * 60, unix_time);
    clock.zone = wall_clock_zone_of(-300);
    CHECK(wall_clock_unix(&clock, 60000, &unix_time));
    CHECK_EQUAL(UNIX_2026_11_05 + 60 + 300 * 60, unix_time);
    CHECK_EQUAL(UNIX_2026_11_05 + 60, wall_clock_local_of_unix(unix_time, clock.zone));
}

static void zones_step_by_a_quarter_hour_from_minus_12_to_plus_14(void)
{
    CHECK_EQUAL(106, WALL_CLOCK_ZONE_COUNT);
    CHECK_EQUAL(0, wall_clock_zone_minutes(WALL_CLOCK_ZONE_UNSET));
    CHECK_EQUAL(0, wall_clock_zone_minutes(WALL_CLOCK_ZONE_UTC));
    CHECK_EQUAL(-12 * 60, wall_clock_zone_minutes(1));
    CHECK_EQUAL(14 * 60, wall_clock_zone_minutes(WALL_CLOCK_ZONE_COUNT - 1));
    CHECK_EQUAL(0, wall_clock_zone_minutes(WALL_CLOCK_ZONE_COUNT));
    CHECK_EQUAL(WALL_CLOCK_ZONE_UTC, wall_clock_zone_of(0));
    CHECK_EQUAL(345, wall_clock_zone_minutes(wall_clock_zone_of(345)));
    CHECK_EQUAL(-210, wall_clock_zone_minutes(wall_clock_zone_of(-210)));
    CHECK_EQUAL(60, wall_clock_zone_minutes(wall_clock_zone_of(67)));
    CHECK_EQUAL(-12 * 60, wall_clock_zone_minutes(wall_clock_zone_of(-20 * 60)));
    CHECK_EQUAL(14 * 60, wall_clock_zone_minutes(wall_clock_zone_of(20 * 60)));
}

static void times_and_dates_format_with_zero_padding(void)
{
    char text[WALL_CLOCK_DATE_TIME_CAPACITY];
    WallCivil civil = {2026, 1, 5, 9, 7, 3};
    wall_clock_time_text(&civil, text, sizeof text);
    CHECK(text_is("09:07", text));
    wall_clock_date_time_text(&civil, text, sizeof text);
    CHECK(text_is("2026-01-05 09:07", text));
    civil = (WallCivil){2099, 12, 31, 23, 59, 59};
    wall_clock_date_time_text(&civil, text, sizeof text);
    CHECK(text_is("2099-12-31 23:59", text));
}

static void zones_format_as_utc_offsets(void)
{
    char text[WALL_CLOCK_ZONE_CAPACITY];
    wall_clock_zone_text(WALL_CLOCK_ZONE_UTC, text, sizeof text);
    CHECK(text_is("UTC+00:00", text));
    wall_clock_zone_text(wall_clock_zone_of(330), text, sizeof text);
    CHECK(text_is("UTC+05:30", text));
    wall_clock_zone_text(wall_clock_zone_of(-570), text, sizeof text);
    CHECK(text_is("UTC-09:30", text));
    wall_clock_zone_text(WALL_CLOCK_ZONE_UNSET, text, sizeof text);
    CHECK(text_is("UTC+00:00", text));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(days_count_from_the_unix_epoch),
        TEST_CASE(civil_times_round_trip_through_seconds),
        TEST_CASE(every_day_from_2000_to_2099_round_trips),
        TEST_CASE(the_weekday_counts_sunday_as_zero),
        TEST_CASE(civil_times_are_checked_against_the_calendar),
        TEST_CASE(the_clock_keeps_time_from_its_anchor),
        TEST_CASE(the_clock_survives_the_millisecond_counter_wrapping),
        TEST_CASE(unix_time_needs_a_known_clock_and_a_zone),
        TEST_CASE(zones_step_by_a_quarter_hour_from_minus_12_to_plus_14),
        TEST_CASE(times_and_dates_format_with_zero_padding),
        TEST_CASE(zones_format_as_utc_offsets),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
