#include "core/format.h"

#include "core/string_builder.h"

enum {
    MICROSECONDS_PER_TENTH_MS = 100,
    TENTHS_PER_MS = 10,
    MILLISECONDS_PER_SECOND = 1000,
    SECONDS_PER_MINUTE = 60,
    SECONDS_PER_HOUR = 3600,
    SMALLEST_TWO_DIGIT = 10
};

static void append_two_digits(StringBuilder *builder, uint32_t value)
{
    if (value < SMALLEST_TWO_DIGIT) {
        string_builder_append_char(builder, '0');
    }
    string_builder_append_unsigned(builder, value);
}

void format_milliseconds(uint32_t microseconds, char *buffer, size_t capacity)
{
    uint32_t tenths = (microseconds + MICROSECONDS_PER_TENTH_MS / 2) / MICROSECONDS_PER_TENTH_MS;
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append_unsigned(&builder, tenths / TENTHS_PER_MS);
    string_builder_append_char(&builder, '.');
    string_builder_append_unsigned(&builder, tenths % TENTHS_PER_MS);
    string_builder_append(&builder, " ms");
}

void format_duration(uint32_t milliseconds, char *buffer, size_t capacity)
{
    uint32_t total_seconds = milliseconds / MILLISECONDS_PER_SECOND;
    uint32_t hours = total_seconds / SECONDS_PER_HOUR;
    uint32_t minutes = total_seconds / SECONDS_PER_MINUTE % SECONDS_PER_MINUTE;
    StringBuilder builder = string_builder_make(buffer, capacity);
    if (hours > 0) {
        string_builder_append_unsigned(&builder, hours);
        string_builder_append_char(&builder, ':');
        append_two_digits(&builder, minutes);
    } else {
        string_builder_append_unsigned(&builder, minutes);
    }
    string_builder_append_char(&builder, ':');
    append_two_digits(&builder, total_seconds % SECONDS_PER_MINUTE);
}
