#include "core/wall_clock.h"

#include "core/string_builder.h"

enum {
    DAYS_PER_ERA = 146097,
    YEARS_PER_ERA = 400,
    DAYS_BEFORE_UNIX_EPOCH = 719468,
    SECONDS_PER_MINUTE = 60,
    SECONDS_PER_HOUR = 3600,
    SECONDS_PER_DAY = 86400,
    MILLISECONDS_PER_SECOND = 1000,
    MONTHS_PER_YEAR = 12,
    EPOCH_WEEKDAY = 4,
    DAYS_PER_WEEK = 7,
    FEBRUARY = 2,
    SHORT_MONTH_DAYS = 30,
    FEBRUARY_DAYS = 28,
    LONG_MONTHS = 0x15AA,
    LAST_MINUTE = 59,
    LAST_HOUR = 23
};

uint32_t wall_clock_days_from_civil(uint32_t year, uint32_t month, uint32_t day)
{
    uint32_t shifted = month <= 2 ? year - 1u : year;
    uint32_t era = shifted / YEARS_PER_ERA;
    uint32_t year_of_era = shifted - era * YEARS_PER_ERA;
    uint32_t day_of_year = (153u * ((month + 9u) % 12u) + 2u) / 5u + day - 1u;
    uint32_t day_of_era = year_of_era * 365u + year_of_era / 4u - year_of_era / 100u + day_of_year;
    return era * DAYS_PER_ERA + day_of_era - DAYS_BEFORE_UNIX_EPOCH;
}

static void civil_from_days(uint32_t days, WallCivil *civil)
{
    uint32_t shifted = days + DAYS_BEFORE_UNIX_EPOCH;
    uint32_t era = shifted / DAYS_PER_ERA;
    uint32_t day_of_era = shifted - era * DAYS_PER_ERA;
    uint32_t year_of_era =
        (day_of_era - day_of_era / 1460u + day_of_era / 36524u - day_of_era / 146096u) / 365u;
    uint32_t day_of_year =
        day_of_era - (365u * year_of_era + year_of_era / 4u - year_of_era / 100u);
    uint32_t shifted_month = (5u * day_of_year + 2u) / 153u;
    uint32_t month = shifted_month < 10u ? shifted_month + 3u : shifted_month - 9u;
    civil->day = (uint8_t)(day_of_year - (153u * shifted_month + 2u) / 5u + 1u);
    civil->month = (uint8_t)month;
    civil->year = (uint16_t)(year_of_era + era * YEARS_PER_ERA + (month <= 2u ? 1u : 0u));
}

uint32_t wall_clock_seconds_of(const WallCivil *civil)
{
    return wall_clock_days_from_civil(civil->year, civil->month, civil->day) * SECONDS_PER_DAY +
           civil->hour * SECONDS_PER_HOUR + civil->minute * SECONDS_PER_MINUTE + civil->second;
}

void wall_clock_civil_of(uint32_t seconds, WallCivil *civil)
{
    civil_from_days(seconds / SECONDS_PER_DAY, civil);
    uint32_t clock = seconds % SECONDS_PER_DAY;
    civil->hour = (uint8_t)(clock / SECONDS_PER_HOUR);
    civil->minute = (uint8_t)(clock % SECONDS_PER_HOUR / SECONDS_PER_MINUTE);
    civil->second = (uint8_t)(clock % SECONDS_PER_MINUTE);
}

uint8_t wall_clock_weekday(uint32_t seconds)
{
    return (uint8_t)((seconds / SECONDS_PER_DAY + EPOCH_WEEKDAY) % DAYS_PER_WEEK);
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    if (month == FEBRUARY) {
        bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        return (uint8_t)(FEBRUARY_DAYS + (leap ? 1 : 0));
    }
    return (uint8_t)(SHORT_MONTH_DAYS + ((LONG_MONTHS >> month) & 1));
}

bool wall_clock_civil_valid(const WallCivil *civil)
{
    return civil->year >= WALL_CLOCK_FIRST_YEAR && civil->year <= WALL_CLOCK_LAST_YEAR &&
           civil->month >= 1 && civil->month <= MONTHS_PER_YEAR && civil->day >= 1 &&
           civil->day <= days_in_month(civil->year, civil->month) && civil->hour <= LAST_HOUR &&
           civil->minute <= LAST_MINUTE && civil->second <= LAST_MINUTE;
}

int32_t wall_clock_zone_minutes(uint8_t zone)
{
    if (zone == WALL_CLOCK_ZONE_UNSET || zone >= WALL_CLOCK_ZONE_COUNT) {
        return 0;
    }
    return WALL_CLOCK_ZONE_FIRST_MINUTES + (zone - 1) * WALL_CLOCK_ZONE_STEP_MINUTES;
}

uint8_t wall_clock_zone_of(int32_t offset_minutes)
{
    int32_t clamped = offset_minutes < WALL_CLOCK_ZONE_FIRST_MINUTES ? WALL_CLOCK_ZONE_FIRST_MINUTES
                      : offset_minutes > WALL_CLOCK_ZONE_LAST_MINUTES ? WALL_CLOCK_ZONE_LAST_MINUTES
                                                                      : offset_minutes;
    int32_t steps = clamped - WALL_CLOCK_ZONE_FIRST_MINUTES + WALL_CLOCK_ZONE_STEP_MINUTES / 2;
    return (uint8_t)(steps / WALL_CLOCK_ZONE_STEP_MINUTES + 1);
}

void wall_clock_set(WallClock *clock, uint32_t local_seconds, uint32_t now_ms)
{
    clock->known = true;
    clock->anchor_seconds = local_seconds;
    clock->anchor_ms = now_ms;
}

uint32_t wall_clock_local(const WallClock *clock, uint32_t now_ms)
{
    return clock->anchor_seconds + (now_ms - clock->anchor_ms) / MILLISECONDS_PER_SECOND;
}

bool wall_clock_unix(const WallClock *clock, uint32_t now_ms, uint32_t *unix_time)
{
    if (!clock->known || clock->zone == WALL_CLOCK_ZONE_UNSET) {
        return false;
    }
    *unix_time = wall_clock_local(clock, now_ms) -
                 (uint32_t)(wall_clock_zone_minutes(clock->zone) * SECONDS_PER_MINUTE);
    return true;
}

uint32_t wall_clock_local_of_unix(uint32_t unix_time, uint8_t zone)
{
    return unix_time + (uint32_t)(wall_clock_zone_minutes(zone) * SECONDS_PER_MINUTE);
}

static void append_two_digits(StringBuilder *builder, uint32_t value)
{
    string_builder_append_char(builder, (char)('0' + value / 10 % 10));
    string_builder_append_char(builder, (char)('0' + value % 10));
}

static void append_time(StringBuilder *builder, const WallCivil *civil)
{
    append_two_digits(builder, civil->hour);
    string_builder_append_char(builder, ':');
    append_two_digits(builder, civil->minute);
}

void wall_clock_time_text(const WallCivil *civil, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_time(&builder, civil);
}

void wall_clock_date_time_text(const WallCivil *civil, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append_unsigned(&builder, civil->year);
    string_builder_append_char(&builder, '-');
    append_two_digits(&builder, civil->month);
    string_builder_append_char(&builder, '-');
    append_two_digits(&builder, civil->day);
    string_builder_append_char(&builder, ' ');
    append_time(&builder, civil);
}

void wall_clock_zone_text(uint8_t zone, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    int32_t minutes = wall_clock_zone_minutes(zone);
    uint32_t magnitude = (uint32_t)(minutes < 0 ? -minutes : minutes);
    string_builder_append(&builder, "UTC");
    string_builder_append_char(&builder, minutes < 0 ? '-' : '+');
    append_two_digits(&builder, magnitude / SECONDS_PER_MINUTE);
    string_builder_append_char(&builder, ':');
    append_two_digits(&builder, magnitude % SECONDS_PER_MINUTE);
}
