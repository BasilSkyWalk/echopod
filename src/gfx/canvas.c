#include "gfx/canvas.h"

static Color *pixel_address(const Canvas *canvas, int x, int y)
{
    int column = x - canvas->bounds.x;
    int row = y - canvas->bounds.y;
    return &canvas->pixels[row * canvas->bounds.width + column];
}

static bool contains(const Canvas *canvas, int x, int y)
{
    Rect point = {x, y, 1, 1};
    return !rect_is_empty(rect_intersection(canvas->bounds, point));
}

Canvas canvas_make(Color *pixels, Rect bounds)
{
    return (Canvas){.pixels = pixels, .bounds = bounds};
}

void canvas_clear(Canvas *canvas, Color color)
{
    canvas_fill_rect(canvas, canvas->bounds, color);
}

void canvas_fill_rect(Canvas *canvas, Rect area, Color color)
{
    Rect visible = rect_intersection(canvas->bounds, area);
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

Color canvas_pixel_at(const Canvas *canvas, int x, int y)
{
    return contains(canvas, x, y) ? *pixel_address(canvas, x, y) : 0;
}
