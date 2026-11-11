#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings/settings.h"
#include "ui/theme_package.h"

enum {
    THEME_STORE_FILES = 8,
    THEME_STORE_OPTIONS = THEME_COUNT + THEME_STORE_FILES,
    THEME_STORE_NONE = -1
};

extern const char THEME_STORE_PATH[];
extern const char THEME_STORE_FOLDER[];

void theme_store_start(void);
const ThemePackage *theme_store_package(void);
uint8_t theme_store_revision(void);
int theme_store_scan(void);
const char *theme_store_name(int index);
int theme_store_current(void);
bool theme_store_choose(int index);
bool theme_store_wear(const uint8_t *bytes, uint32_t size);
const char **theme_store_options(void);
