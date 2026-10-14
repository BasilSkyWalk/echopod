#include "ui/header.h"

#include "ui/label.h"

enum {
    DIVIDER_HEIGHT = 1
};

void header_draw(Canvas *canvas, const Theme *theme, const char *title)
{
    Rect area = theme_header_area(theme);
    Rect divider = {area.x, area.y + area.height - DIVIDER_HEIGHT, area.width, DIVIDER_HEIGHT};
    Rect title_bounds = {area.x + theme->metrics.padding, area.y,
                         area.width - 2 * theme->metrics.padding, area.height - DIVIDER_HEIGHT};
    TextStyle style = {.font = theme->fonts.title, .color = theme->colors.header_text};
    canvas_fill_rect(canvas, area, theme->colors.header_background);
    canvas_fill_rect(canvas, divider, theme->colors.divider);
    Rect title_area = label_centered_area(style.font, title_bounds, title);
    label_draw(canvas, style, (Label){.area = title_area, .text = title});
}
