#include "core/string_builder.h"

enum {
    MAX_DECIMAL_DIGITS = 10,
    DECIMAL_BASE = 10
};

StringBuilder string_builder_make(char *buffer, size_t capacity)
{
    if (capacity > 0) {
        buffer[0] = '\0';
    }
    return (StringBuilder){.buffer = buffer, .capacity = capacity};
}

void string_builder_append_char(StringBuilder *builder, char character)
{
    if (builder->length + 1 >= builder->capacity) {
        builder->overflowed = true;
        return;
    }
    builder->buffer[builder->length++] = character;
    builder->buffer[builder->length] = '\0';
}

void string_builder_append(StringBuilder *builder, const char *text)
{
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        string_builder_append_char(builder, *cursor);
    }
}

void string_builder_append_unsigned(StringBuilder *builder, uint32_t value)
{
    char digits[MAX_DECIMAL_DIGITS];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % DECIMAL_BASE);
        value /= DECIMAL_BASE;
    } while (value > 0);
    while (count > 0) {
        string_builder_append_char(builder, digits[--count]);
    }
}
