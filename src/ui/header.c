#include "ui/header.h"

#include "ui/box.h"
#include "ui/label.h"

enum {
    DIVIDER_HEIGHT = 1
};

void header_draw(Canvas *canvas, const Theme *theme, const char *title)
{
    Rect area = theme_header_area(theme);
    int inset = theme->metrics.header_inset > theme->metrics.padding ? theme->metrics.header_inset
                                                                     : theme->metrics.padding;
    Rect title_row = theme_title_area(theme);
    Rect title_bounds = {title_row.x + inset, title_row.y, title_row.width - 2 * inset,
                         title_row.height - DIVIDER_HEIGHT};
    TextStyle style = {.font = theme->fonts.title, .color = theme->colors->header_text};
    box_draw(canvas, theme->colors, &theme->boxes->header, area);
    Rect title_area = label_centered_area(style.font, title_bounds, title);
    label_draw(canvas, style, (Label){.area = title_area, .text = title});
}
