#include "core/format.h"

#include "core/string_builder.h"

enum {
    MICROSECONDS_PER_TENTH_MS = 100,
    TENTHS_PER_MS = 10
};

void format_milliseconds(uint32_t microseconds, char *buffer, size_t capacity)
{
    uint32_t tenths = (microseconds + MICROSECONDS_PER_TENTH_MS / 2) / MICROSECONDS_PER_TENTH_MS;
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append_unsigned(&builder, tenths / TENTHS_PER_MS);
    string_builder_append_char(&builder, '.');
    string_builder_append_unsigned(&builder, tenths % TENTHS_PER_MS);
    string_builder_append(&builder, " ms");
}
