#pragma once

#include <stdbool.h>

#include "ui/theme.h"

extern const Theme DEFAULT_THEME;

Theme default_theme_for(bool portrait);
Theme default_theme_styled(bool portrait, int accent, bool flat, bool light);
