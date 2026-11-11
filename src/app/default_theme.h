#pragma once

#include <stdbool.h>

#include "ui/theme.h"

extern const ThemeColors DEFAULT_THEME_DARK_COLORS;
extern const Theme DEFAULT_THEME;

Theme default_theme_for(bool portrait);

typedef struct ThemeChoice {
    int theme;
    int accent;
    bool flat;
} ThemeChoice;

Theme default_theme_styled(bool portrait, ThemeChoice choice, ThemeColors *tinted);
Theme default_theme_stage(bool portrait, ThemeChoice choice, ThemeColors *tinted);
bool default_theme_takes_accent(int theme);
bool default_theme_takes_highlight(int theme);
void default_theme_enlarge_status(Theme *theme);
