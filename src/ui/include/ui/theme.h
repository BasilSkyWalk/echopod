#pragma once

#include "gfx/color.h"
#include "gfx/font.h"
#include "gfx/rect.h"
#include "ui/box_style.h"
#include "ui/marquee.h"

#define THEME_COLOR(id, name) Color name;
typedef struct ThemeColors {
#include "ui/theme_colors.def"
} ThemeColors;
#undef THEME_COLOR

#define THEME_COLOR(id, name) THEME_COLOR_##id,
typedef enum ThemeColorId {
#include "ui/theme_colors.def"
    THEME_COLOR_COUNT
} ThemeColorId;
#undef THEME_COLOR

#define THEME_BOX(id, name) BoxStyle name;
typedef struct ThemeBoxes {
#include "ui/theme_boxes.def"
} ThemeBoxes;
#undef THEME_BOX

#define THEME_BOX(id, name) THEME_BOX_##id,
typedef enum ThemeBoxId {
#include "ui/theme_boxes.def"
    THEME_BOX_COUNT
} ThemeBoxId;
#undef THEME_BOX

#define THEME_SPACING(id, name, least, most) THEME_SPACING_##id,
typedef enum ThemeSpacingId {
#include "ui/theme_spacing.def"
    THEME_SPACING_COUNT
} ThemeSpacingId;
#undef THEME_SPACING

#define THEME_ICON(id, name, width, height) THEME_ICON_##id,
typedef enum ThemeIconId {
#include "ui/theme_icons.def"
    THEME_ICON_COUNT
} ThemeIconId;
#undef THEME_ICON

#define THEME_FONT(id, name, height) THEME_FONT_##id,
typedef enum ThemeFontId {
#include "ui/theme_fonts.def"
    THEME_FONT_COUNT
} ThemeFontId;
#undef THEME_FONT

#define THEME_LAYOUT(id, name, height) THEME_LAYOUT_##id,
typedef enum ThemeLayoutId {
#include "ui/theme_layout.def"
    THEME_LAYOUT_COUNT
} ThemeLayoutId;
#undef THEME_LAYOUT

typedef enum ThemeAlign {
    THEME_ALIGN_LEFT,
    THEME_ALIGN_CENTER,
    THEME_ALIGN_RIGHT,
    THEME_ALIGN_COUNT,
    THEME_ALIGN_UNSET = 0xFF
} ThemeAlign;

typedef struct ThemeLayoutBox {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint8_t height;
    uint8_t align;
} ThemeLayoutBox;

typedef struct ThemeLayout {
    ThemeLayoutBox boxes[THEME_LAYOUT_COUNT];
} ThemeLayout;

typedef struct IconMask {
    const uint8_t *levels;
    uint8_t width;
    uint8_t height;
} IconMask;

typedef struct ThemeIcons {
    IconMask masks[THEME_ICON_COUNT];
} ThemeIcons;

typedef struct Wallpaper {
    const uint32_t *pairs;
    const uint8_t *rows;
    uint16_t width;
    uint16_t height;
    bool ready;
} Wallpaper;

_Static_assert(sizeof(ThemeColors) == THEME_COLOR_COUNT * sizeof(Color),
               "the colours are a table indexed by ThemeColorId");
_Static_assert(sizeof(ThemeBoxes) == THEME_BOX_COUNT * sizeof(BoxStyle),
               "the boxes are a table indexed by ThemeBoxId");

typedef struct ThemeFonts {
    const Font *title;
    const Font *body;
    const Font *small;
    const Font *status;
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

typedef enum ThemeSelection {
    THEME_SELECTION_GRADIENT,
    THEME_SELECTION_FLAT,
    THEME_SELECTION_OUTLINE
} ThemeSelection;

_Static_assert((int)THEME_SELECTION_GRADIENT == (int)THEME_BOX_SELECTION &&
                   (int)THEME_SELECTION_FLAT == (int)THEME_BOX_SELECTION_FLAT &&
                   (int)THEME_SELECTION_OUTLINE == (int)THEME_BOX_SELECTION_OUTLINE,
               "a selection shape is the id of the box that draws it");

typedef struct Theme {
    const ThemeColors *colors;
    const ThemeBoxes *boxes;
    const ThemeIcons *icons;
    const Wallpaper *wallpaper;
    const ThemeLayout *layout;
    ThemeFonts fonts;
    ThemeMetrics metrics;
    MarqueeTiming marquee;
    ThemeSelection selection;
} Theme;

extern const ThemeBoxes THEME_STANDARD_BOXES;

Color theme_color(const ThemeColors *colors, uint8_t id);
void theme_space(ThemeMetrics *metrics, ThemeSpacingId id, int value);
const BoxStyle *theme_selection_box(const Theme *theme);
const IconMask *theme_icon(const Theme *theme, ThemeIconId id);
Rect theme_header_area(const Theme *theme);
Rect theme_content_area(const Theme *theme);
int theme_visible_rows(const Theme *theme);
bool theme_is_portrait(const Theme *theme);
Rect theme_title_area(const Theme *theme);
Color theme_selected_text(const Theme *theme);
