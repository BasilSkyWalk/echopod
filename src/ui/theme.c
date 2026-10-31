#include "ui/theme.h"

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
