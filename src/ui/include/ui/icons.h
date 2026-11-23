#pragma once

#include "gfx/canvas.h"
#include "ui/theme.h"

typedef struct Icon {
    ThemeIconId id;
    Rect area;
    Color color;
} Icon;

void icon_draw(Canvas *canvas, const Theme *theme, Icon icon);
Rect icon_ink_area(const Theme *theme, Icon icon);

void icon_draw_play(Canvas *canvas, Rect area, Color color);
void icon_draw_pause(Canvas *canvas, Rect area, Color color);
void icon_draw_stop(Canvas *canvas, Rect area, Color color);
void icon_draw_next(Canvas *canvas, Rect area, Color color);
void icon_draw_previous(Canvas *canvas, Rect area, Color color);
void icon_draw_bolt(Canvas *canvas, Rect area, Color color);
void icon_draw_bluetooth(Canvas *canvas, Rect area, Color color);
