#pragma once

#include "gfx/color.h"
#include "gfx/rect.h"

typedef struct Canvas {
    Color *pixels;
    Rect bounds;
} Canvas;

Canvas canvas_make(Color *pixels, Rect bounds);
void canvas_clear(Canvas *canvas, Color color);
void canvas_fill_rect(Canvas *canvas, Rect area, Color color);
Color canvas_pixel_at(const Canvas *canvas, int x, int y);
