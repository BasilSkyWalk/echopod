#include "ui/theme.h"

const ThemeBoxes THEME_STANDARD_BOXES = {
    .selection = {.fill_top = THEME_COLOR_SELECTION_TOP,
                  .fill_bottom = THEME_COLOR_SELECTION_BACKGROUND,
                  .shine = THEME_COLOR_SELECTION_SHINE,
                  .ink = THEME_COLOR_SELECTION_TEXT,
                  .shine_height = 1},
    .selection_flat = {.fill_top = THEME_COLOR_SELECTION_BACKGROUND,
                       .fill_bottom = THEME_COLOR_SELECTION_BACKGROUND,
                       .ink = THEME_COLOR_SELECTION_TEXT},
    .selection_outline = {.fill_top = THEME_COLOR_BACKGROUND,
                          .fill_bottom = THEME_COLOR_BACKGROUND,
                          .border = THEME_COLOR_SELECTION_TOP,
                          .ink = THEME_COLOR_SELECTION_TOP,
                          .border_width = 1,
                          .border_edges = BOX_EDGE_ALL,
                          .inset_across = 6,
                          .inset_down = 1},
    .header = {.fill_top = THEME_COLOR_HEADER_TOP,
               .fill_bottom = THEME_COLOR_HEADER_BACKGROUND,
               .border = THEME_COLOR_DIVIDER,
               .ink = THEME_COLOR_HEADER_TEXT,
               .border_width = 1,
               .border_edges = BOX_EDGE_BOTTOM},
    .toast = {.fill_top = THEME_COLOR_HEADER_BACKGROUND,
              .fill_bottom = THEME_COLOR_HEADER_BACKGROUND,
              .border = THEME_COLOR_DIVIDER,
              .ink = THEME_COLOR_HEADER_TEXT,
              .border_width = 1,
              .border_edges = BOX_EDGE_ALL},
    .section = {.fill_top = THEME_COLOR_HEADER_BACKGROUND,
                .fill_bottom = THEME_COLOR_HEADER_BACKGROUND,
                .border = THEME_COLOR_DIVIDER,
                .ink = THEME_COLOR_HEADER_TEXT,
                .border_width = 1,
                .border_edges = BOX_EDGE_ALL},
    .scrollbar_thumb = {.fill_top = THEME_COLOR_SCROLLBAR_THUMB,
                        .fill_bottom = THEME_COLOR_SCROLLBAR_THUMB},
    .progress_track = {.fill_top = THEME_COLOR_DIVIDER, .fill_bottom = THEME_COLOR_DIVIDER},
    .progress_fill = {.fill_top = THEME_COLOR_TEXT, .fill_bottom = THEME_COLOR_TEXT},
    .progress_active = {.fill_top = THEME_COLOR_SELECTION_BACKGROUND,
                        .fill_bottom = THEME_COLOR_SELECTION_BACKGROUND},
};

Color theme_color(const ThemeColors *colors, uint8_t id)
{
    return ((const Color *)colors)[id];
}

void theme_space(ThemeMetrics *metrics, ThemeSpacingId id, int value)
{
    int *const fields[THEME_SPACING_COUNT] = {
#define THEME_SPACING(id, name, least, most) &metrics->name,
#include "ui/theme_spacing.def"
#undef THEME_SPACING
    };
    *fields[id] = value;
}

const BoxStyle *theme_selection_box(const Theme *theme)
{
    return &((const BoxStyle *)theme->boxes)[theme->selection];
}

Rect theme_header_area(const Theme *theme)
{
    return (Rect){0, 0, theme->metrics.screen_width, theme->metrics.header_height};
}

Rect theme_content_area(const Theme *theme)
{
    const ThemeMetrics *metrics = &theme->metrics;
    return (Rect){0, metrics->header_height, metrics->screen_width,
                  metrics->screen_height - metrics->header_height};
}

int theme_visible_rows(const Theme *theme)
{
    return theme_content_area(theme).height / theme->metrics.row_height;
}

bool theme_is_portrait(const Theme *theme)
{
    return theme->metrics.screen_height > theme->metrics.screen_width;
}

Rect theme_title_area(const Theme *theme)
{
    const ThemeMetrics *metrics = &theme->metrics;
    return (Rect){0, metrics->status_height, metrics->screen_width,
                  metrics->header_height - metrics->status_height};
}

Color theme_selected_text(const Theme *theme)
{
    return theme_color(theme->colors, theme_selection_box(theme)->ink);
}
