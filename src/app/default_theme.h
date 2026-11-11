#pragma once

#include <stdbool.h>

#include "ui/theme.h"

extern const Theme DEFAULT_THEME;

Theme default_theme_for(bool portrait);

typedef struct ThemeChoice {
    int theme;
    int accent;
    bool flat;
} ThemeChoice;

Theme default_theme_styled(bool portrait, ThemeChoice choice);
Theme default_theme_stage(bool portrait, ThemeChoice choice);
bool default_theme_takes_accent(int theme);
bool default_theme_takes_highlight(int theme);
void default_theme_enlarge_status(Theme *theme);
