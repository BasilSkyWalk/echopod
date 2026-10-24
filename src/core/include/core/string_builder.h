#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct StringBuilder {
    char *buffer;
    size_t capacity;
    size_t length;
    bool overflowed;
} StringBuilder;

StringBuilder string_builder_make(char *buffer, size_t capacity);
void string_builder_append(StringBuilder *builder, const char *text);
void string_builder_append_char(StringBuilder *builder, char character);
void string_builder_append_unsigned(StringBuilder *builder, uint32_t value);
