#pragma once

#include <stdint.h>

#include "gfx/color.h"
#include "gfx/rect.h"

typedef struct Canvas {
    Color *pixels;
    Rect bounds;
    Rect clip;
} Canvas;

Canvas canvas_make(Color *pixels, Rect bounds);
Canvas canvas_clipped(const Canvas *canvas, Rect area);
void canvas_clear(Canvas *canvas, Color color);
void canvas_fill_rect(Canvas *canvas, Rect area, Color color);
void canvas_blend_pixel(Canvas *canvas, Point point, Color color, uint8_t alpha);
Color canvas_pixel_at(const Canvas *canvas, int x, int y);
