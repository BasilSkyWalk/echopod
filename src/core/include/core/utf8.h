#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    UTF8_REPLACEMENT_CHARACTER = 0xFFFD
};

uint32_t utf8_next(const char **cursor);
size_t utf8_complete_length(const char *text, size_t length);
