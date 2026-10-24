#include "ui_test_canvas.h"

#include "ui_test_theme.h"

static Color pixels[TEST_SCREEN_WIDTH * TEST_SCREEN_HEIGHT];

Canvas ui_test_canvas(void)
{
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, TEST_SCREEN_WIDTH, TEST_SCREEN_HEIGHT});
    canvas_clear(&canvas, 0x0821);
    return canvas;
}

static int count_color(const Canvas *canvas, Rect area, Color color)
{
    int count = 0;
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            count += canvas_pixel_at(canvas, x, y) == color ? 1 : 0;
        }
    }
    return count;
}

bool ui_test_area_has_color(const Canvas *canvas, Rect area, Color color)
{
    return count_color(canvas, area, color) > 0;
}

bool ui_test_area_has_only(const Canvas *canvas, Rect area, Color color)
{
    return count_color(canvas, area, color) == area.width * area.height;
}
