#include "core/format.h"

#include "core/string_builder.h"

enum {
    MICROSECONDS_PER_TENTH_MS = 100,
    TENTHS_PER_MS = 10,
    MILLISECONDS_PER_SECOND = 1000,
    SECONDS_PER_MINUTE = 60,
    SECONDS_PER_HOUR = 3600,
    SMALLEST_TWO_DIGIT = 10,
    HERTZ_PER_TENTH_KHZ = 100,
    TENTHS_PER_KHZ = 10,
    THOUSAND = 1000,
    BITS_PER_BYTE = 8
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

void format_sample_rate(uint32_t hertz, char *buffer, size_t capacity)
{
    uint32_t tenths = (hertz + HERTZ_PER_TENTH_KHZ / 2) / HERTZ_PER_TENTH_KHZ;
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append_unsigned(&builder, tenths / TENTHS_PER_KHZ);
    if (tenths % TENTHS_PER_KHZ != 0) {
        string_builder_append_char(&builder, '.');
        string_builder_append_unsigned(&builder, tenths % TENTHS_PER_KHZ);
    }
    string_builder_append(&builder, " kHz");
}

static void append_three_digits(StringBuilder *builder, uint32_t value)
{
    if (value < SMALLEST_TWO_DIGIT * SMALLEST_TWO_DIGIT) {
        string_builder_append_char(builder, '0');
    }
    append_two_digits(builder, value);
}

static void append_grouped(StringBuilder *builder, uint32_t value)
{
    if (value < THOUSAND) {
        string_builder_append_unsigned(builder, value);
        return;
    }
    append_grouped(builder, value / THOUSAND);
    string_builder_append_char(builder, ',');
    append_three_digits(builder, value % THOUSAND);
}

void format_bitrate(uint32_t kbps, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    append_grouped(&builder, kbps);
    string_builder_append(&builder, " kbps");
}

uint32_t average_kbps(uint32_t audio_bytes, uint32_t total_samples, uint32_t sample_rate)
{
    if (total_samples == 0 || sample_rate == 0) {
        return 0;
    }
    uint64_t bits_per_second_scaled = (uint64_t)audio_bytes * BITS_PER_BYTE * sample_rate;
    uint64_t divisor = (uint64_t)total_samples * THOUSAND;
    return (uint32_t)((bits_per_second_scaled + divisor / 2) / divisor);
}
