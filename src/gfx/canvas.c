#include "gfx/canvas.h"

static Color *pixel_address(const Canvas *canvas, int x, int y)
{
    int column = x - canvas->bounds.x;
    int row = y - canvas->bounds.y;
    return &canvas->pixels[row * canvas->bounds.width + column];
}

static bool contains(Rect area, Point point)
{
    Rect single = {point.x, point.y, 1, 1};
    return !rect_is_empty(rect_intersection(area, single));
}

Canvas canvas_make(Color *pixels, Rect bounds)
{
    return (Canvas){.pixels = pixels, .bounds = bounds, .clip = bounds};
}

Canvas canvas_clipped(const Canvas *canvas, Rect area)
{
    Canvas clipped = *canvas;
    clipped.clip = rect_intersection(canvas->clip, area);
    return clipped;
}

Canvas canvas_shifted(const Canvas *canvas, int dx, Rect area)
{
    Canvas shifted = *canvas;
    shifted.bounds.x -= dx;
    shifted.clip.x -= dx;
    shifted.clip = rect_intersection(shifted.clip, area);
    return shifted;
}

void canvas_clear(Canvas *canvas, Color color)
{
    canvas_fill_rect(canvas, canvas->clip, color);
}

void canvas_fill_rect(Canvas *canvas, Rect area, Color color)
{
    Rect visible = rect_intersection(canvas->clip, area);
    if (rect_is_empty(visible)) {
        return;
    }
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        Color *row = pixel_address(canvas, visible.x, y);
        for (int column = 0; column < visible.width; column++) {
            row[column] = color;
        }
    }
}

void canvas_fill_gradient(Canvas *canvas, Rect area, Color top, Color bottom)
{
    for (int row = 0; row < area.height; row++) {
        Color color = color_between(top, bottom, row, area.height - 1);
        canvas_fill_rect(canvas, (Rect){area.x, area.y + row, area.width, 1}, color);
    }
}

void canvas_blend_pixel(Canvas *canvas, Point point, Color color, uint8_t alpha)
{
    if (!contains(canvas->clip, point)) {
        return;
    }
    Color *pixel = pixel_address(canvas, point.x, point.y);
    *pixel = color_blend(*pixel, color, alpha);
}

Color canvas_pixel_at(const Canvas *canvas, int x, int y)
{
    return contains(canvas->bounds, (Point){x, y}) ? *pixel_address(canvas, x, y) : 0;
}
