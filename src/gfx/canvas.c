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
    uint32_t pair = (uint32_t)color << 16 | color;
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        Color *row = pixel_address(canvas, visible.x, y);
        int count = visible.width;
        if (((uintptr_t)row & sizeof(Color)) != 0) {
            *row++ = color;
            count--;
        }
        for (; count >= 2; count -= 2) {
            __builtin_memcpy(row, &pair, sizeof pair);
            row += 2;
        }
        if (count > 0) {
            *row = color;
        }
    }
}

enum {
    BLEND_RED_LEVELS = 32,
    BLEND_GREEN_LEVELS = 64,
    BLEND_BLUE_LEVELS = 32
};

typedef struct BlendTable {
    Color red[BLEND_RED_LEVELS];
    Color green[BLEND_GREEN_LEVELS];
    Color blue[BLEND_BLUE_LEVELS];
    Color color;
    uint8_t alpha_after;
} BlendTable;

static BlendTable blend_table;

static unsigned blended(unsigned back, unsigned paint, unsigned alpha)
{
    unsigned weighted = back * (COLOR_ALPHA_OPAQUE - alpha) + paint * alpha;
    return (weighted + COLOR_ALPHA_OPAQUE / 2) / COLOR_ALPHA_OPAQUE;
}

__attribute__((cold, noinline)) static const BlendTable *table_for(Color color, uint8_t alpha)
{
    BlendTable *table = &blend_table;
    if (table->alpha_after == alpha + 1 && table->color == color) {
        return table;
    }
    for (unsigned level = 0; level < BLEND_GREEN_LEVELS; level++) {
        table->green[level] = (Color)(blended(level, color >> 5 & 0x3F, alpha) << 5);
        if (level < BLEND_RED_LEVELS) {
            table->red[level] = (Color)(blended(level, (unsigned)color >> 11, alpha) << 11);
            table->blue[level] = (Color)blended(level, color & 0x1F, alpha);
        }
    }
    table->color = color;
    table->alpha_after = (uint8_t)(alpha + 1);
    return table;
}

static void blend_row(Color *row, int count, const BlendTable *table)
{
    for (int column = 0; column < count; column++) {
        unsigned back = row[column];
        row[column] =
            table->red[back >> 11] | table->green[back >> 5 & 0x3F] | table->blue[back & 0x1F];
    }
}

void canvas_blend_rect(Canvas *canvas, Rect area, Color color, uint8_t alpha)
{
    Rect visible = rect_intersection(canvas->clip, area);
    if (alpha >= COLOR_ALPHA_OPAQUE || rect_is_empty(visible)) {
        canvas_fill_rect(canvas, visible, color);
        return;
    }
    const BlendTable *table = table_for(color, alpha);
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        blend_row(pixel_address(canvas, visible.x, y), visible.width, table);
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
