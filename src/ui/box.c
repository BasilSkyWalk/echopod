#include "ui/box.h"

#include "core/draw_profile.h"

enum {
    CORNER_SAMPLES = 4,
    CORNER_SCALE = 2 * CORNER_SAMPLES,
    ROOT_TOP_BIT = 1 << 14
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

static uint8_t opacity_of(const BoxStyle *style)
{
    return (uint8_t)(COLOR_ALPHA_OPAQUE - style->see_through);
}

static void draw_edges(Canvas *canvas, Rect area, Rect inner, Color color, uint8_t alpha)
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
        canvas_blend_rect(canvas, bands[band], color, alpha);
    }
}

static void draw_fill(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style, Rect area)
{
    Color top = theme_color(colors, style->fill_top);
    Color bottom = theme_color(colors, style->fill_bottom);
    int shine = style->shine_height < area.height ? style->shine_height : area.height;
    Rect body = {area.x, area.y + shine, area.width, area.height - shine};
    uint8_t alpha = opacity_of(style);
    canvas_blend_rect(canvas, (Rect){area.x, area.y, area.width, shine},
                      theme_color(colors, style->shine), alpha);
    if (top == bottom) {
        canvas_blend_rect(canvas, body, top, alpha);
    } else if (alpha == COLOR_ALPHA_OPAQUE) {
        canvas_fill_gradient(canvas, body, top, bottom);
    } else {
        for (int row = 0; row < body.height; row++) {
            Color color = color_between(top, bottom, row, body.height - 1);
            canvas_blend_rect(canvas, (Rect){body.x, body.y + row, body.width, 1}, color, alpha);
        }
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
    uint8_t alpha;
} Shape;

static int root(int value)
{
    int found = 0;
    for (int bit = ROOT_TOP_BIT; bit > 0; bit >>= 1) {
        int trial = found | bit;
        if (trial * trial <= value) {
            found = trial;
        }
    }
    return found;
}

static void corner_reaches(Corner corner, int *reaches)
{
    int reach = CORNER_SCALE * corner.radius;
    for (int sub = 0; sub < CORNER_SAMPLES; sub++) {
        int down = reach - (CORNER_SCALE * corner.row + 2 * sub + 1);
        int room = reach * reach - down * down;
        reaches[sub] = room < 0 ? -1 : root(room);
    }
}

static uint8_t corner_coverage(Corner corner, const int *reaches, int column)
{
    int reach = CORNER_SCALE * corner.radius;
    int inside = 0;
    for (int sub = 0; sub < CORNER_SAMPLES; sub++) {
        int first = reach - CORNER_SCALE * column - 1 - reaches[sub];
        int skipped = first <= 0 ? 0 : (first + 1) / 2;
        inside += reaches[sub] < 0 || skipped >= CORNER_SAMPLES ? 0 : CORNER_SAMPLES - skipped;
    }
    return (uint8_t)(inside * COLOR_ALPHA_OPAQUE / (CORNER_SAMPLES * CORNER_SAMPLES));
}

static void blend_around(Canvas *canvas, Rect span, Color color, uint8_t alpha, Rect hole)
{
    bool crosses = span.y >= hole.y && span.y < hole.y + hole.height && hole.width > 0;
    if (!crosses) {
        canvas_blend_rect(canvas, span, color, alpha);
        return;
    }
    int right = span.x + span.width;
    int hole_right = hole.x + hole.width;
    canvas_blend_rect(canvas, (Rect){span.x, span.y, hole.x - span.x, 1}, color, alpha);
    canvas_blend_rect(canvas, (Rect){hole_right, span.y, right - hole_right, 1}, color, alpha);
}

static void draw_corner_row(Canvas *canvas, Rect line, Corner corner, Color color, uint8_t opacity,
                            Rect hole)
{
    int reaches[CORNER_SAMPLES];
    corner_reaches(corner, reaches);
    for (int column = 0; column < corner.radius; column++) {
        uint8_t alpha =
            (uint8_t)(corner_coverage(corner, reaches, column) * opacity / COLOR_ALPHA_OPAQUE);
        int mirrored = line.x + line.width - 1 - column;
        canvas_blend_pixel(canvas, (Point){line.x + column, line.y}, color, alpha);
        canvas_blend_pixel(canvas, (Point){mirrored, line.y}, color, alpha);
    }
    Rect middle = {line.x + corner.radius, line.y, line.width - 2 * corner.radius, 1};
    blend_around(canvas, middle, color, opacity, hole);
}

static Color shape_row_color(const Shape *shape, int height, int row)
{
    if (row < shape->shine_height) {
        return shape->shine;
    }
    int steps = height - shape->shine_height - 1;
    return color_between(shape->top, shape->bottom, row - shape->shine_height, steps);
}

static void draw_shape(Canvas *canvas, Rect area, const Shape *shape, Rect hole)
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
            draw_corner_row(canvas, line, (Corner){radius, from_edge}, color, shape->alpha, hole);
        } else {
            blend_around(canvas, line, color, shape->alpha, hole);
        }
    }
}

static void draw_rounded(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style,
                         Rect outer)
{
    Color border = theme_color(colors, style->border);
    int width = style->border_width;
    Shape rim = {
        .top = border, .bottom = border, .radius = style->radius, .alpha = opacity_of(style)};
    Shape body = {.top = theme_color(colors, style->fill_top),
                  .bottom = theme_color(colors, style->fill_bottom),
                  .shine = theme_color(colors, style->shine),
                  .shine_height = style->shine_height,
                  .radius = style->radius > width ? style->radius - width : 0,
                  .alpha = opacity_of(style)};
    Rect inner = inner_area(outer, style);
    int shorter = inner.width < inner.height ? inner.width : inner.height;
    int curve = body.radius < shorter / 2 ? body.radius : shorter / 2;
    Rect hole = {inner.x + curve, inner.y, inner.width - 2 * curve, inner.height};
    if (width > 0 && style->border_edges != 0) {
        draw_shape(canvas, outer, &rim, hole);
    }
    draw_shape(canvas, inner, &body, (Rect){0, 0, 0, 0});
}

static void draw_box(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style, Rect area)
{
    Rect outer = inset_area(area, style->inset_across, style->inset_down);
    if (style->radius > 0) {
        draw_rounded(canvas, colors, style, outer);
        return;
    }
    Rect inner = inner_area(outer, style);
    if (style->border_width > 0) {
        draw_edges(canvas, outer, inner, theme_color(colors, style->border), opacity_of(style));
    }
    draw_fill(canvas, colors, style, inner);
}

void box_draw(Canvas *canvas, const ThemeColors *colors, const BoxStyle *style, Rect area)
{
    uint32_t started_us = draw_profile_now();
    draw_box(canvas, colors, style, area);
    draw_profile_since(DRAW_PART_BOXES, started_us);
}
