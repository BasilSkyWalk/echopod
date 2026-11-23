#include "ui/icons.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    PAUSE_BAR_TENTHS = 3,
    TENTHS = 10,
    SKIP_BAR_PARTS = 6,
    BOLT_STROKE_TENTHS = 5,
    BOLT_UPPER_START_TENTHS = 5,
    BOLT_LOWER_END_TENTHS = 5
};

static void draw_triangle(Canvas *canvas, Rect area, Color color, bool pointing_right)
{
    for (int row = 0; row < area.height; row++) {
        int reach = row < area.height / 2 ? row : area.height - 1 - row;
        int width = 2 * reach + 1 < area.width ? 2 * reach + 1 : area.width;
        int left = pointing_right ? area.x : area.x + area.width - width;
        canvas_fill_rect(canvas, (Rect){left, area.y + row, width, 1}, color);
    }
}

void icon_draw_play(Canvas *canvas, Rect area, Color color)
{
    draw_triangle(canvas, area, color, true);
}

static int skip_bar_width(Rect area)
{
    int bar = area.width / SKIP_BAR_PARTS;
    return bar > 1 ? bar : 1;
}

void icon_draw_next(Canvas *canvas, Rect area, Color color)
{
    int bar = skip_bar_width(area);
    int half = (area.width - bar) / 2;
    draw_triangle(canvas, (Rect){area.x, area.y, half, area.height}, color, true);
    draw_triangle(canvas, (Rect){area.x + half, area.y, half, area.height}, color, true);
    canvas_fill_rect(canvas, (Rect){area.x + area.width - bar, area.y, bar, area.height}, color);
}

void icon_draw_previous(Canvas *canvas, Rect area, Color color)
{
    int bar = skip_bar_width(area);
    int half = (area.width - bar) / 2;
    canvas_fill_rect(canvas, (Rect){area.x, area.y, bar, area.height}, color);
    draw_triangle(canvas, (Rect){area.x + bar, area.y, half, area.height}, color, false);
    draw_triangle(canvas, (Rect){area.x + bar + half, area.y, half, area.height}, color, false);
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

static void fill_row(Canvas *canvas, Rect area, int row, int left, int width, Color color)
{
    int clipped_left = left < 0 ? 0 : left;
    int right = left + width > area.width ? area.width : left + width;
    if (right > clipped_left) {
        canvas_fill_rect(
            canvas, (Rect){area.x + clipped_left, area.y + row, right - clipped_left, 1}, color);
    }
}

void icon_draw_bolt(Canvas *canvas, Rect area, Color color)
{
    int middle = area.height / 2;
    int stroke = area.width * BOLT_STROKE_TENTHS / TENTHS;
    int upper_start = area.width * BOLT_UPPER_START_TENTHS / TENTHS;
    int lower_end = area.width * BOLT_LOWER_END_TENTHS / TENTHS;
    int stroke_width = stroke > 0 ? stroke : 1;
    for (int row = 0; row < area.height; row++) {
        if (row < middle) {
            int left = upper_start - upper_start * row / (middle > 0 ? middle : 1);
            fill_row(canvas, area, row, left, stroke_width, color);
        } else if (row == middle) {
            fill_row(canvas, area, row, 0, area.width, color);
        } else {
            int span = area.height - 1 - middle;
            int right =
                area.width - (area.width - lower_end) * (row - middle) / (span > 0 ? span : 1);
            fill_row(canvas, area, row, right - stroke_width, stroke_width, color);
        }
    }
}

enum {
    RUNE_ROWS = 9,
    RUNE_COLUMNS = 5
};

static const uint8_t RUNE[RUNE_ROWS] = {0x04, 0x0C, 0x15, 0x0E, 0x04, 0x0E, 0x15, 0x0C, 0x04};

void icon_draw_bluetooth(Canvas *canvas, Rect area, Color color)
{
    int left = area.x + (area.width - RUNE_COLUMNS) / 2;
    int top = area.y + (area.height - RUNE_ROWS) / 2;
    for (int row = 0; row < RUNE_ROWS; row++) {
        for (int column = 0; column < RUNE_COLUMNS; column++) {
            if ((RUNE[row] >> column & 1) != 0) {
                canvas_fill_rect(canvas, (Rect){left + column, top + row, 1, 1}, color);
            }
        }
    }
}

typedef void (*Shape)(Canvas *canvas, Rect area, Color color);

static const Shape SHAPES[THEME_ICON_COUNT] = {
    [THEME_ICON_STATUS_PLAY] = icon_draw_play,
    [THEME_ICON_STATUS_PAUSE] = icon_draw_pause,
    [THEME_ICON_STATUS_BLUETOOTH] = icon_draw_bluetooth,
    [THEME_ICON_STATUS_BOLT] = icon_draw_bolt,
    [THEME_ICON_BADGE_PLAY] = icon_draw_play,
    [THEME_ICON_BADGE_PAUSE] = icon_draw_pause,
    [THEME_ICON_BADGE_NEXT] = icon_draw_next,
    [THEME_ICON_BADGE_PREVIOUS] = icon_draw_previous,
    [THEME_ICON_STATUS_PLAY_LARGE] = icon_draw_play,
    [THEME_ICON_STATUS_PAUSE_LARGE] = icon_draw_pause,
    [THEME_ICON_STATUS_BLUETOOTH_LARGE] = icon_draw_bluetooth,
    [THEME_ICON_STATUS_BOLT_LARGE] = icon_draw_bolt,
};

static uint8_t mask_level(const IconMask *mask, int column, int row)
{
    uint8_t pair = mask->levels[row * ((mask->width + 1) / 2) + column / 2];
    return column % 2 == 0 ? pair >> 4 : pair & 0x0F;
}

static Point mask_origin(const IconMask *mask, Rect area)
{
    return (Point){area.x + (area.width - mask->width) / 2,
                   area.y + (area.height - mask->height) / 2};
}

static void draw_mask(Canvas *canvas, const IconMask *mask, Icon icon)
{
    Point origin = mask_origin(mask, icon.area);
    for (int row = 0; row < mask->height; row++) {
        for (int column = 0; column < mask->width; column++) {
            uint8_t level = mask_level(mask, column, row);
            if (level > 0) {
                Point point = {origin.x + column, origin.y + row};
                canvas_blend_pixel(canvas, point, icon.color, level);
            }
        }
    }
}

void icon_draw(Canvas *canvas, const Theme *theme, Icon icon)
{
    const IconMask *mask = theme_icon(theme, icon.id);
    if (mask != NULL) {
        draw_mask(canvas, mask, icon);
    } else if (SHAPES[icon.id] != NULL) {
        SHAPES[icon.id](canvas, icon.area, icon.color);
    }
}

static bool column_inked(const IconMask *mask, int column)
{
    for (int row = 0; row < mask->height; row++) {
        if (mask_level(mask, column, row) > 0) {
            return true;
        }
    }
    return false;
}

Rect icon_ink_area(const Theme *theme, Icon icon)
{
    const IconMask *mask = theme_icon(theme, icon.id);
    if (mask == NULL) {
        return (Rect){0, 0, 0, 0};
    }
    Point origin = mask_origin(mask, icon.area);
    int first = 0;
    int last = mask->width - 1;
    while (first < last && !column_inked(mask, first)) {
        first++;
    }
    while (last > first && !column_inked(mask, last)) {
        last--;
    }
    return (Rect){origin.x + first, origin.y, last - first + 1, mask->height};
}
