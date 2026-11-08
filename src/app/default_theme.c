#include "default_theme.h"

#include "fonts/fonts.h"
#include "hal/display.h"

const Theme DEFAULT_THEME = {
    .colors =
        {
            .background = COLOR_RGB(16, 17, 20),
            .text = COLOR_RGB(242, 242, 245),
            .text_secondary = COLOR_RGB(142, 142, 150),
            .text_tertiary = COLOR_RGB(98, 98, 106),
            .selection_background = COLOR_RGB(34, 96, 208),
            .selection_top = COLOR_RGB(82, 145, 240),
            .selection_shine = COLOR_RGB(120, 170, 248),
            .selection_text = COLOR_RGB(255, 255, 255),
            .header_background = COLOR_RGB(22, 23, 27),
            .header_top = COLOR_RGB(46, 47, 54),
            .header_text = COLOR_RGB(242, 242, 245),
            .divider = COLOR_RGB(48, 49, 56),
            .scrollbar_thumb = COLOR_RGB(96, 97, 106),
            .danger = COLOR_RGB(235, 77, 61),
            .success = COLOR_RGB(52, 199, 89),
            .chevron = COLOR_RGB(110, 110, 118),
        },
    .fonts =
        {
            .title = &FONT_INTER_SEMIBOLD_16,
            .body = &FONT_INTER_REGULAR_16,
            .small = &FONT_INTER_REGULAR_12,
        },
    .metrics =
        {
            .screen_width = HAL_DISPLAY_WIDTH,
            .screen_height = HAL_DISPLAY_HEIGHT,
            .header_height = 24,
            .header_inset = 72,
            .row_height = 24,
            .padding = 10,
            .scrollbar_width = 3,
            .scrollbar_inset = 2,
            .detail_gap = 8,
            .status_icon_size = 10,
        },
    .marquee = {.pause_ms = 1200, .speed_px_per_s = 40, .gap_px = 32},
};

enum {
    PORTRAIT_STATUS_HEIGHT = 16,
    PORTRAIT_TITLE_HEIGHT = 24,
    LARGE_STATUS_HEIGHT = 22,
    LARGE_STATUS_ICON_SIZE = 14,
    ACCENT_SHADES = 3,
    ACCENTS = 8
};

static const Color ACCENT_COLORS[ACCENTS][ACCENT_SHADES] = {
    {COLOR_RGB(34, 96, 208), COLOR_RGB(82, 145, 240), COLOR_RGB(120, 170, 248)},
    {COLOR_RGB(192, 38, 46), COLOR_RGB(230, 84, 88), COLOR_RGB(244, 128, 128)},
    {COLOR_RGB(206, 100, 18), COLOR_RGB(240, 148, 58), COLOR_RGB(250, 186, 110)},
    {COLOR_RGB(28, 136, 68), COLOR_RGB(68, 186, 108), COLOR_RGB(120, 214, 150)},
    {COLOR_RGB(14, 124, 136), COLOR_RGB(50, 174, 184), COLOR_RGB(110, 206, 212)},
    {COLOR_RGB(108, 56, 194), COLOR_RGB(150, 102, 232), COLOR_RGB(186, 150, 244)},
    {COLOR_RGB(196, 48, 118), COLOR_RGB(234, 96, 158), COLOR_RGB(246, 146, 190)},
    {COLOR_RGB(76, 78, 90), COLOR_RGB(116, 118, 132), COLOR_RGB(156, 158, 170)},
};

Theme default_theme_for(bool portrait)
{
    Theme theme = DEFAULT_THEME;
    if (portrait) {
        theme.metrics.screen_width = HAL_DISPLAY_SHORT_SIDE;
        theme.metrics.screen_height = HAL_DISPLAY_LONG_SIDE;
        theme.metrics.status_height = PORTRAIT_STATUS_HEIGHT;
        theme.metrics.header_height = PORTRAIT_STATUS_HEIGHT + PORTRAIT_TITLE_HEIGHT;
        theme.metrics.header_inset = theme.metrics.padding;
    }
    return theme;
}

void default_theme_enlarge_status(Theme *theme)
{
    if (!theme_is_portrait(theme)) {
        return;
    }
    theme->metrics.status_height = LARGE_STATUS_HEIGHT;
    theme->metrics.header_height = LARGE_STATUS_HEIGHT + PORTRAIT_TITLE_HEIGHT;
    theme->metrics.status_icon_size = LARGE_STATUS_ICON_SIZE;
    theme->fonts.status = &FONT_INTER_REGULAR_16;
}

static const ThemeColors LIGHT_COLORS = {
    .background = COLOR_RGB(244, 244, 247),
    .text = COLOR_RGB(18, 18, 22),
    .text_secondary = COLOR_RGB(84, 84, 92),
    .text_tertiary = COLOR_RGB(118, 118, 126),
    .selection_text = COLOR_RGB(255, 255, 255),
    .header_background = COLOR_RGB(232, 232, 238),
    .header_top = COLOR_RGB(208, 208, 218),
    .header_text = COLOR_RGB(18, 18, 22),
    .divider = COLOR_RGB(200, 200, 208),
    .scrollbar_thumb = COLOR_RGB(150, 150, 160),
    .danger = COLOR_RGB(200, 40, 30),
    .success = COLOR_RGB(24, 140, 56),
    .chevron = COLOR_RGB(132, 132, 140),
};

Theme default_theme_styled(bool portrait, int accent, bool flat, bool light)
{
    Theme theme = default_theme_for(portrait);
    if (light) {
        theme.colors = LIGHT_COLORS;
    }
    const Color *shades = ACCENT_COLORS[accent >= 0 && accent < ACCENTS ? accent : 0];
    theme.colors.selection_background = shades[0];
    theme.colors.selection_top = flat ? shades[0] : shades[1];
    theme.colors.selection_shine = flat ? shades[0] : shades[2];
    return theme;
}
