#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/theme.h"

enum {
    THEME_PACKAGE_VERSION = 1,
    THEME_PACKAGE_HEADER_BYTES = 48,
    THEME_PACKAGE_NAME_BYTES = 24,
    THEME_PACKAGE_COLORS_FIELD = 16,
    THEME_PACKAGE_BOXES_FIELD = 18,
    THEME_PACKAGE_BOX_BYTES_FIELD = 20,
    THEME_PACKAGE_SELECTION_FIELD = 21,
    THEME_PACKAGE_SPACING_FIELD = 22,
    THEME_PACKAGE_NAME_FIELD = 24,
    THEME_PACKAGE_MAX_BYTES = 512
};

typedef struct ThemePackage {
    ThemeColors colors;
    ThemeBoxes boxes;
    uint8_t selection;
    uint8_t spacing[THEME_SPACING_COUNT];
    char name[THEME_PACKAGE_NAME_BYTES];
} ThemePackage;

enum {
    THEME_PACKAGE_SPACING_UNSET = 0
};

bool theme_package_decode(const uint8_t *bytes, uint32_t size, ThemePackage *package);
