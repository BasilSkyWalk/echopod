#include "ui/box.h"

enum {
    CORNER_SAMPLES = 4,
    CORNER_SCALE = 2 * CORNER_SAMPLES
};

static Rect inset_area(Rect area, int across, int down)
{
    return (Rect){area.x + across, area.y + down, area.width - 2 * across, area.height - 2 * down};
}

static int edge_width(const BoxStyle *style, int edge)
{
    return (style->border_edges & edge) != 0 ? style->border_width : 0;
}

static Rect inner_area(Rect area, const BoxStyle *style)
{
    int left = edge_width(style, BOX_EDGE_LEFT);
    int top = edge_width(style, BOX_EDGE_TOP);
    int across = left + edge_width(style, BOX_EDGE_RIGHT);
    int down = top + edge_width(style, BOX_EDGE_BOTTOM);
    return (Rect){area.x + left, area.y + top, area.width - across, area.height - down};
}

static void draw_edges(Canvas *canvas, Rect area, Rect inner, Color color)
{
    int inner_bottom = inner.y + inner.height;
    int inner_right = inner.x + inner.width;
    const Rect bands[] = {
        {area.x, area.y, area.width, inner.y - area.y},
        {area.x, inner_bottom, area.width, area.y + area.height - inner_bottom},
        {area.x, inner.y, inner.x - area.x, inner.height},
        {inner_right, inner.y, area.x + area.width - inner_right, inner.height},
    };
    for (unsigned band = 0; band < sizeof bands / sizeof bands[0]; band++) {
        canvas_fill_rect(canvas, bands[band], color);
    }
}

static void draw_fill(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style, Rect area)
{
    Color top = theme_color(colors, style->fill_top);
    Color bottom = theme_color(colors, style->fill_bottom);
    int shine = style->shine_height < area.height ? style->shine_height : area.height;
    Rect body = {area.x, area.y + shine, area.width, area.height - shine};
    canvas_fill_rect(canvas, (Rect){area.x, area.y, area.width, shine},
                     theme_color(colors, style->shine));
    if (top == bottom) {
        canvas_fill_rect(canvas, body, top);
    } else {
        canvas_fill_gradient(canvas, body, top, bottom);
    }
}

typedef struct Corner {
    int radius;
    int row;
} Corner;

typedef struct Shape {
    Color top;
    Color bottom;
    Color shine;
    int shine_height;
    int radius;
} Shape;

static uint8_t corner_coverage(Corner corner, int column)
{
    int reach = CORNER_SCALE * corner.radius;
    int inside = 0;
    for (int sample = 0; sample < CORNER_SAMPLES * CORNER_SAMPLES; sample++) {
        int across = reach - (CORNER_SCALE * column + 2 * (sample % CORNER_SAMPLES) + 1);
        int down = reach - (CORNER_SCALE * corner.row + 2 * (sample / CORNER_SAMPLES) + 1);
        inside += across * across + down * down <= reach * reach ? 1 : 0;
    }
    return (uint8_t)(inside * COLOR_ALPHA_OPAQUE / (CORNER_SAMPLES * CORNER_SAMPLES));
}

static void draw_corner_row(Canvas *canvas, Rect line, Corner corner, Color color)
{
    for (int column = 0; column < corner.radius; column++) {
        uint8_t alpha = corner_coverage(corner, column);
        int mirrored = line.x + line.width - 1 - column;
        canvas_blend_pixel(canvas, (Point){line.x + column, line.y}, color, alpha);
        canvas_blend_pixel(canvas, (Point){mirrored, line.y}, color, alpha);
    }
    Rect middle = {line.x + corner.radius, line.y, line.width - 2 * corner.radius, 1};
    canvas_fill_rect(canvas, middle, color);
}

static Color shape_row_color(const Shape *shape, int height, int row)
{
    if (row < shape->shine_height) {
        return shape->shine;
    }
    int steps = height - shape->shine_height - 1;
    return color_between(shape->top, shape->bottom, row - shape->shine_height, steps);
}

static void draw_shape(Canvas *canvas, Rect area, const Shape *shape)
{
    int shorter = area.width < area.height ? area.width : area.height;
    int radius = shape->radius < shorter / 2 ? shape->radius : shorter / 2;
    for (int row = 0; row < area.height; row++) {
        Rect line = {area.x, area.y + row, area.width, 1};
        int from_edge = row < area.height - 1 - row ? row : area.height - 1 - row;
        Color color = shape_row_color(shape, area.height, row);
        if (rect_is_empty(rect_intersection(canvas->clip, line))) {
            continue;
        }
        if (from_edge < radius) {
            draw_corner_row(canvas, line, (Corner){radius, from_edge}, color);
        } else {
            canvas_fill_rect(canvas, line, color);
        }
    }
}

static void draw_rounded(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style,
                         Rect outer)
{
    Color border = theme_color(colors, style->border);
    int width = style->border_width;
    Shape rim = {.top = border, .bottom = border, .radius = style->radius};
    Shape body = {.top = theme_color(colors, style->fill_top),
                  .bottom = theme_color(colors, style->fill_bottom),
                  .shine = theme_color(colors, style->shine),
                  .shine_height = style->shine_height,
                  .radius = style->radius > width ? style->radius - width : 0};
    if (width > 0 && style->border_edges != 0) {
        draw_shape(canvas, outer, &rim);
    }
    draw_shape(canvas, inner_area(outer, style), &body);
}

void box_draw(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style, Rect area)
{
    Rect outer = inset_area(area, style->inset_across, style->inset_down);
    if (style->radius > 0) {
        draw_rounded(canvas, colors, style, outer);
        return;
    }
    Rect inner = inner_area(outer, style);
    if (style->border_width > 0) {
        draw_edges(canvas, outer, inner, theme_color(colors, style->border));
    }
    draw_fill(canvas, colors, style, inner);
}
