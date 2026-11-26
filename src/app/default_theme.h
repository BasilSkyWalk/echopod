#pragma once

#include <stdbool.h>

#include "ui/theme.h"
#include "ui/theme_package.h"

extern const ThemeColors DEFAULT_THEME_DARK_COLORS;
extern const Theme DEFAULT_THEME;

Theme default_theme_for(bool portrait);

typedef struct ThemeChoice {
    int accent;
    bool flat;
} ThemeChoice;

bool default_theme_takes_accent(const ThemePackage *package);
bool default_theme_takes_highlight(const ThemePackage *package);
int default_theme_accent(const ThemePackage *package, int accent);
void default_theme_dress(Theme *theme, const ThemePackage *package, ThemeChoice choice,
                         ThemeColors *tinted);
bool default_theme_stage_keeps_colors(const ThemePackage *package);
void default_theme_stage(Theme *stage, const ThemePackage *package, ThemeChoice choice,
                         ThemeColors *tinted);
void default_theme_enlarge_status(Theme *theme);
