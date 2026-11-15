#pragma once

#include <stdint.h>

#include "settings/settings.h"

enum {
    BUILTIN_THEME_COUNT = THEME_COUNT
};

typedef struct BuiltinTheme {
    const uint8_t *bytes;
    uint32_t size;
} BuiltinTheme;

extern const BuiltinTheme BUILTIN_THEMES[BUILTIN_THEME_COUNT];
