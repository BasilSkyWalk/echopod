#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/font.h"
#include "settings/settings.h"
#include "theme_assets.h"
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
const ThemeIcons *theme_store_icons(void);
uint8_t theme_store_revision(void);
int theme_store_scan(void);
const char *theme_store_name(int index);
int theme_store_current(void);
bool theme_store_choose(int index);
bool theme_store_wear(const char *name);
const Font *theme_store_font(ThemeFontId id);
const Wallpaper *theme_store_wallpaper(bool portrait);
void theme_store_watch_assets(void);
bool theme_store_tend_assets(void);
const char **theme_store_options(void);
