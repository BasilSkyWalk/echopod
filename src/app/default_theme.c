#include "default_theme.h"

#include "core/resident.h"
#include "fonts/fonts.h"
#include "hal/display.h"
#include "settings/settings.h"

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
    ACCENTS = 8,
    THEME_TRAIT_STAGE = 1,
    THEME_TRAIT_TINTABLE = 2,
    DARK_TRAITS = THEME_TRAIT_STAGE | THEME_TRAIT_TINTABLE
};

typedef struct ThemePalette {
    ThemeColors colors;
    uint8_t selection;
    uint8_t traits;
} ThemePalette;

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

ECHO_RESIDENT static const ThemePalette
    PALETTES[THEME_COUNT - 1] =
        {
            {.colors =
                 {
                     .background = COLOR_RGB(244, 244, 247),
                     .text = COLOR_RGB(18, 18, 22),
                     .text_secondary = COLOR_RGB(84, 84, 92),
                     .text_tertiary = COLOR_RGB(118, 118, 126),
                     .selection_background = COLOR_RGB(34, 96, 208),
                     .selection_top = COLOR_RGB(82, 145, 240),
                     .selection_shine = COLOR_RGB(120, 170, 248),
                     .selection_text = COLOR_RGB(255, 255, 255),
                     .header_background = COLOR_RGB(232, 232, 238),
                     .header_top = COLOR_RGB(208, 208, 218),
                     .header_text = COLOR_RGB(18, 18, 22),
                     .divider = COLOR_RGB(200, 200, 208),
                     .scrollbar_thumb = COLOR_RGB(150, 150, 160),
                     .danger = COLOR_RGB(200, 40, 30),
                     .success = COLOR_RGB(24, 140, 56),
                     .chevron = COLOR_RGB(132, 132, 140),
                 },
             .selection = THEME_SELECTION_GRADIENT,
             .traits = THEME_TRAIT_TINTABLE},
            {.colors =
                 {
                     .background = COLOR_RGB(24, 32, 40),
                     .text = COLOR_RGB(232, 236, 240),
                     .text_secondary = COLOR_RGB(168, 180, 192),
                     .text_tertiary = COLOR_RGB(128, 140, 160),
                     .selection_background = COLOR_RGB(128, 168, 200),
                     .selection_top = COLOR_RGB(152, 192, 224),
                     .selection_shine = COLOR_RGB(224, 240, 248),
                     .selection_text = COLOR_RGB(24, 32, 40),
                     .header_background = COLOR_RGB(32, 44, 56),
                     .header_top = COLOR_RGB(48, 60, 72),
                     .header_text = COLOR_RGB(232, 236, 240),
                     .divider = COLOR_RGB(64, 76, 96),
                     .scrollbar_thumb = COLOR_RGB(104, 120, 144),
                     .danger = COLOR_RGB(240, 120, 112),
                     .success = COLOR_RGB(136, 208, 160),
                     .chevron = COLOR_RGB(120, 136, 160),
                 },
             .selection = THEME_SELECTION_GRADIENT,
             .traits = THEME_TRAIT_STAGE},
            {.colors =
                 {
                     .background = COLOR_RGB(24, 16, 48),
                     .text = COLOR_RGB(248, 232, 248),
                     .text_secondary = COLOR_RGB(200, 168, 216),
                     .text_tertiary = COLOR_RGB(168, 136, 200),
                     .selection_background = COLOR_RGB(72, 208, 224),
                     .selection_top = COLOR_RGB(96, 232, 248),
                     .selection_shine = COLOR_RGB(248, 252, 248),
                     .selection_text = COLOR_RGB(24, 16, 48),
                     .header_background = COLOR_RGB(48, 24, 72),
                     .header_top = COLOR_RGB(112, 24, 104),
                     .header_text = COLOR_RGB(248, 232, 248),
                     .divider = COLOR_RGB(96, 48, 128),
                     .scrollbar_thumb = COLOR_RGB(128, 88, 168),
                     .danger = COLOR_RGB(248, 184, 72),
                     .success = COLOR_RGB(96, 248, 200),
                     .chevron = COLOR_RGB(160, 120, 192),
                 },
             .selection = THEME_SELECTION_GRADIENT,
             .traits = THEME_TRAIT_STAGE},
            {.colors =
                 {
                     .background = COLOR_RGB(0, 0, 0),
                     .text = COLOR_RGB(240, 236, 248),
                     .text_secondary = COLOR_RGB(168, 160, 184),
                     .text_tertiary = COLOR_RGB(136, 128, 152),
                     .selection_background = COLOR_RGB(240, 72, 176),
                     .selection_top = COLOR_RGB(240, 72, 176),
                     .selection_shine = COLOR_RGB(248, 200, 232),
                     .selection_text = COLOR_RGB(0, 0, 0),
                     .header_background = COLOR_RGB(0, 0, 0),
                     .header_top = COLOR_RGB(88, 0, 88),
                     .header_text = COLOR_RGB(240, 236, 248),
                     .divider = COLOR_RGB(104, 0, 72),
                     .scrollbar_thumb = COLOR_RGB(152, 48, 120),
                     .danger = COLOR_RGB(248, 96, 64),
                     .success = COLOR_RGB(0, 232, 200),
                     .chevron = COLOR_RGB(136, 104, 128),
                 },
             .selection = THEME_SELECTION_OUTLINE,
             .traits = THEME_TRAIT_STAGE},
            {.colors =
                 {
                     .background = COLOR_RGB(0, 12, 0),
                     .text = COLOR_RGB(112, 252, 136),
                     .text_secondary = COLOR_RGB(56, 200, 88),
                     .text_tertiary = COLOR_RGB(40, 144, 64),
                     .selection_background = COLOR_RGB(40, 216, 96),
                     .selection_top = COLOR_RGB(72, 248, 128),
                     .selection_shine = COLOR_RGB(16, 112, 48),
                     .selection_text = COLOR_RGB(0, 20, 8),
                     .header_background = COLOR_RGB(0, 24, 8),
                     .header_top = COLOR_RGB(0, 48, 8),
                     .header_text = COLOR_RGB(112, 252, 136),
                     .divider = COLOR_RGB(24, 88, 40),
                     .scrollbar_thumb = COLOR_RGB(40, 136, 64),
                     .danger = COLOR_RGB(248, 112, 64),
                     .success = COLOR_RGB(208, 252, 72),
                     .chevron = COLOR_RGB(48, 168, 80),
                 },
             .selection = THEME_SELECTION_FLAT,
             .traits = THEME_TRAIT_STAGE},
            {.colors =
                 {
                     .background = COLOR_RGB(240, 232, 216),
                     .text = COLOR_RGB(40, 32, 24),
                     .text_secondary = COLOR_RGB(88, 64, 48),
                     .text_tertiary = COLOR_RGB(120, 96, 72),
                     .selection_background = COLOR_RGB(152, 48, 32),
                     .selection_top = COLOR_RGB(176, 72, 56),
                     .selection_shine = COLOR_RGB(216, 128, 96),
                     .selection_text = COLOR_RGB(248, 248, 240),
                     .header_background = COLOR_RGB(224, 212, 184),
                     .header_top = COLOR_RGB(208, 196, 168),
                     .header_text = COLOR_RGB(40, 32, 24),
                     .divider = COLOR_RGB(176, 160, 136),
                     .scrollbar_thumb = COLOR_RGB(136, 112, 88),
                     .danger = COLOR_RGB(168, 32, 24),
                     .success = COLOR_RGB(40, 112, 48),
                     .chevron = COLOR_RGB(136, 112, 88),
                 },
             .selection = THEME_SELECTION_GRADIENT,
             .traits = 0},
            {.colors =
                 {
                     .background = COLOR_RGB(176, 188, 104),
                     .text = COLOR_RGB(16, 32, 8),
                     .text_secondary = COLOR_RGB(32, 52, 16),
                     .text_tertiary = COLOR_RGB(56, 76, 24),
                     .selection_background = COLOR_RGB(32, 52, 24),
                     .selection_top = COLOR_RGB(56, 76, 48),
                     .selection_shine = COLOR_RGB(128, 148, 64),
                     .selection_text = COLOR_RGB(200, 212, 128),
                     .header_background = COLOR_RGB(160, 176, 88),
                     .header_top = COLOR_RGB(144, 160, 72),
                     .header_text = COLOR_RGB(16, 32, 8),
                     .divider = COLOR_RGB(112, 128, 56),
                     .scrollbar_thumb = COLOR_RGB(64, 84, 32),
                     .danger = COLOR_RGB(136, 24, 16),
                     .success = COLOR_RGB(16, 72, 104),
                     .chevron = COLOR_RGB(48, 68, 24),
                 },
             .selection = THEME_SELECTION_FLAT,
             .traits = 0},
            {.colors =
                 {
                     .background = COLOR_RGB(0, 0, 0),
                     .text = COLOR_RGB(248, 252, 248),
                     .text_secondary = COLOR_RGB(232, 232, 232),
                     .text_tertiary = COLOR_RGB(200, 200, 200),
                     .selection_background = COLOR_RGB(248, 224, 0),
                     .selection_top = COLOR_RGB(248, 224, 0),
                     .selection_shine = COLOR_RGB(152, 88, 0),
                     .selection_text = COLOR_RGB(0, 0, 0),
                     .header_background = COLOR_RGB(64, 64, 64),
                     .header_top = COLOR_RGB(64, 64, 64),
                     .header_text = COLOR_RGB(248, 252, 248),
                     .divider = COLOR_RGB(112, 112, 112),
                     .scrollbar_thumb = COLOR_RGB(248, 252, 248),
                     .danger = COLOR_RGB(248, 128, 112),
                     .success = COLOR_RGB(64, 252, 128),
                     .chevron = COLOR_RGB(248, 252, 248),
                 },
             .selection = THEME_SELECTION_FLAT,
             .traits = THEME_TRAIT_STAGE},
};

static const ThemePalette *palette_of(int theme)
{
    return theme > THEME_DARK && theme < THEME_COUNT ? &PALETTES[theme - THEME_LIGHT] : NULL;
}

static int traits_of(int theme)
{
    const ThemePalette *palette = palette_of(theme);
    return palette != NULL ? palette->traits : DARK_TRAITS;
}

static ThemeSelection selection_of(int theme)
{
    const ThemePalette *palette = palette_of(theme);
    return palette != NULL ? (ThemeSelection)palette->selection : THEME_SELECTION_GRADIENT;
}

bool default_theme_takes_accent(int theme)
{
    return (traits_of(theme) & THEME_TRAIT_TINTABLE) != 0;
}

bool default_theme_takes_highlight(int theme)
{
    return selection_of(theme) == THEME_SELECTION_GRADIENT;
}

static void apply_accent(Theme *theme, ThemeChoice choice)
{
    int accent = choice.accent >= 0 && choice.accent < ACCENTS ? choice.accent : 0;
    const Color *shades = ACCENT_COLORS[accent];
    theme->colors.selection_background = shades[0];
    theme->colors.selection_top = shades[choice.flat ? 0 : 1];
    theme->colors.selection_shine = shades[2];
}

Theme default_theme_styled(bool portrait, ThemeChoice choice)
{
    Theme theme = default_theme_for(portrait);
    const ThemePalette *palette = palette_of(choice.theme);
    if (palette != NULL) {
        theme.colors = palette->colors;
    }
    theme.selection = selection_of(choice.theme);
    if (default_theme_takes_accent(choice.theme)) {
        apply_accent(&theme, choice);
    }
    if (choice.flat && theme.selection == THEME_SELECTION_GRADIENT) {
        theme.selection = THEME_SELECTION_FLAT;
    }
    return theme;
}

Theme default_theme_stage(bool portrait, ThemeChoice choice)
{
    if ((traits_of(choice.theme) & THEME_TRAIT_STAGE) == 0) {
        choice.theme = THEME_DARK;
    }
    return default_theme_styled(portrait, choice);
}
