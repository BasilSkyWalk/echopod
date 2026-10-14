#pragma once

#include <stdint.h>

enum {
    UTF8_REPLACEMENT_CHARACTER = 0xFFFD
};

uint32_t utf8_next(const char **cursor);
