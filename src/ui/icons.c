#include "ui/icons.h"

enum {
    PAUSE_BAR_TENTHS = 3,
    TENTHS = 10
};

void icon_draw_play(Canvas *canvas, Rect area, Color color)
{
    for (int row = 0; row < area.height; row++) {
        int reach = row < area.height / 2 ? row : area.height - 1 - row;
        int width = 2 * reach + 1 < area.width ? 2 * reach + 1 : area.width;
        canvas_fill_rect(canvas, (Rect){area.x, area.y + row, width, 1}, color);
    }
}

void icon_draw_pause(Canvas *canvas, Rect area, Color color)
{
    int bar = area.width * PAUSE_BAR_TENTHS / TENTHS;
    bar = bar > 0 ? bar : 1;
    canvas_fill_rect(canvas, (Rect){area.x, area.y, bar, area.height}, color);
    canvas_fill_rect(canvas, (Rect){area.x + area.width - bar, area.y, bar, area.height}, color);
}

void icon_draw_stop(Canvas *canvas, Rect area, Color color)
{
    canvas_fill_rect(canvas, (Rect){area.x, area.y + 1, area.width - 2, area.height - 2}, color);
}
