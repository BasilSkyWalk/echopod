#pragma once

#include "gfx/color.h"
#include "gfx/font.h"
#include "gfx/rect.h"
#include "ui/marquee.h"

typedef struct ThemeColors {
    Color background;
    Color text;
    Color text_secondary;
    Color text_tertiary;
    Color selection_background;
    Color selection_top;
    Color selection_shine;
    Color selection_text;
    Color header_background;
    Color header_top;
    Color header_text;
    Color divider;
    Color scrollbar_thumb;
    Color danger;
    Color success;
    Color chevron;
} ThemeColors;

typedef struct ThemeFonts {
    const Font *title;
    const Font *body;
    const Font *small;
} ThemeFonts;

typedef struct ThemeMetrics {
    int screen_width;
    int screen_height;
    int header_height;
    int header_inset;
    int row_height;
    int padding;
    int scrollbar_width;
    int scrollbar_inset;
    int detail_gap;
    int status_icon_size;
    int status_height;
} ThemeMetrics;

typedef struct Theme {
    ThemeColors colors;
    ThemeFonts fonts;
    ThemeMetrics metrics;
    MarqueeTiming marquee;
} Theme;

Rect theme_header_area(const Theme *theme);
Rect theme_content_area(const Theme *theme);
int theme_visible_rows(const Theme *theme);
bool theme_is_portrait(const Theme *theme);
Rect theme_title_area(const Theme *theme);
