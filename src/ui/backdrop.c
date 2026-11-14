#include "ui/backdrop.h"

#include <stddef.h>

typedef struct Run {
    int column;
    int count;
} Run;

static Color single(const Wallpaper *wallpaper, const uint8_t *row, int column)
{
    uint32_t both = wallpaper->pairs[row[column / 2]];
    return (Color)(column % 2 == 0 ? both : both >> 16);
}

static Color *paint_tile(Color *target, const Wallpaper *wallpaper, const uint8_t *row, Run run)
{
    int column = run.column;
    int end = run.column + run.count;
    if (column % 2 != 0 && column < end) {
        *target++ = single(wallpaper, row, column++);
    }
    for (; column + 1 < end; column += 2) {
        uint32_t both = wallpaper->pairs[row[column / 2]];
        *target++ = (Color)both;
        *target++ = (Color)(both >> 16);
    }
    if (column < end) {
        *target++ = single(wallpaper, row, column);
    }
    return target;
}

static void paint_row(Color *target, const Wallpaper *wallpaper, const uint8_t *row, Run run)
{
    int width = wallpaper->width;
    int column = (run.column % width + width) % width;
    for (int left = run.count; left > 0; column = 0) {
        int count = left < width - column ? left : width - column;
        target = paint_tile(target, wallpaper, row, (Run){column, count});
        left -= count;
    }
}

static void paint_wallpaper(Canvas *canvas, const Wallpaper *wallpaper, Rect area)
{
    Rect visible = rect_intersection(area, canvas->clip);
    int stride = wallpaper->width / 2;
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        int across = visible.x - canvas->bounds.x;
        Color *target = &canvas->pixels[(y - canvas->bounds.y) * canvas->bounds.width + across];
        const uint8_t *row = wallpaper->rows + (y % wallpaper->height) * stride;
        paint_row(target, wallpaper, row, (Run){visible.x, visible.width});
    }
}

void backdrop_paint(Canvas *canvas, const Theme *theme, Rect area)
{
    const Wallpaper *wallpaper = theme->wallpaper;
    if (wallpaper == NULL || !wallpaper->ready) {
        canvas_fill_rect(canvas, area, theme->colors->background);
    } else if (!canvas->backdrop_painted) {
        paint_wallpaper(canvas, wallpaper, area);
    }
}
