#include "gfx/image.h"

#include <stdbool.h>

typedef struct Column {
    int x;
    int offset;
    int top;
    int height;
} Column;

static int max_of(int first, int second)
{
    return first > second ? first : second;
}

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static int interpolate(int from, int to, int offset, int span)
{
    return from + (to - from) * offset / span;
}

static Column column_at(Quad quad, int x)
{
    int offset = x - quad.left;
    return (Column){
        .x = x,
        .offset = offset,
        .top = interpolate(quad.left_top, quad.right_top, offset, quad.width),
        .height = interpolate(quad.left_height, quad.right_height, offset, quad.width),
    };
}

static Color *pixel_address(Canvas *canvas, int x, int y)
{
    Rect bounds = canvas->bounds;
    return &canvas->pixels[(y - bounds.y) * bounds.width + (x - bounds.x)];
}

typedef struct Sampler {
    const Color *source;
    int whole_step;
    int remainder_step;
    int remainder;
    int rows;
} Sampler;

static Sampler sampler_at(const Column *column, const Image *image, int quad_width, int skip)
{
    int scaled = skip * image->height;
    return (Sampler){
        .source = &image->pixels[column->offset * image->width / quad_width] +
                  scaled / column->height * image->width,
        .whole_step = image->height / column->height * image->width,
        .remainder_step = image->height % column->height,
        .remainder = scaled % column->height,
        .rows = column->height,
    };
}

static void sampler_down(Sampler *sampler, int width)
{
    sampler->source += sampler->whole_step;
    sampler->remainder += sampler->remainder_step;
    if (sampler->remainder >= sampler->rows) {
        sampler->remainder -= sampler->rows;
        sampler->source += width;
    }
}

static void draw_column(Canvas *canvas, Column column, const Image *image, int quad_width)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int stride = canvas->bounds.width;
    Sampler sampler = sampler_at(&column, image, quad_width, first - column.top);
    Color *target = pixel_address(canvas, column.x, first);
    for (int y = first; y < last; y++) {
        *target = *sampler.source;
        target += stride;
        sampler_down(&sampler, image->width);
    }
}

static unsigned light_at(QuadLight light, Column column, int quad_width)
{
    return (unsigned)interpolate(light.left, light.right, column.offset, quad_width);
}

static void draw_lit_column(Canvas *canvas, Column column, const Image *image, int quad_width,
                            unsigned light)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int stride = canvas->bounds.width;
    Sampler sampler = sampler_at(&column, image, quad_width, first - column.top);
    Color *target = pixel_address(canvas, column.x, first);
    for (int y = first; y < last; y++) {
        *target = color_scale(*sampler.source, light);
        target += stride;
        sampler_down(&sampler, image->width);
    }
}

static void draw_reflected_column(Canvas *canvas, Column column, const Image *image, int quad_width,
                                  unsigned light, int rows)
{
    Rect clip = canvas->clip;
    int top = column.top + column.height;
    int first = max_of(top, clip.y);
    int last = min_of(top + rows, clip.y + clip.height);
    if (first >= last) {
        return;
    }
    int skip = first - top;
    int stride = canvas->bounds.width;
    const Color *column_start = &image->pixels[column.offset * image->width / quad_width];
    int last_row = (image->height - 1) * image->width;
    Sampler sampler = sampler_at(&column, image, quad_width, skip);
    uint32_t fade_step = ((uint32_t)light << COLOR_LIGHT_SHIFT) / (uint32_t)rows;
    uint32_t faded = ((uint32_t)light << COLOR_LIGHT_SHIFT) - fade_step * (uint32_t)skip;
    Color *target = pixel_address(canvas, column.x, first);
    for (int y = first; y < last; y++) {
        int mirrored = max_of(last_row - (int)(sampler.source - column_start), 0);
        *target = color_scale(column_start[mirrored], faded >> COLOR_LIGHT_SHIFT);
        target += stride;
        sampler_down(&sampler, image->width);
        faded -= fade_step;
    }
}

static void fill_column(Canvas *canvas, Column column, Color color)
{
    Rect clip = canvas->clip;
    int first = max_of(column.top, clip.y);
    int last = min_of(column.top + column.height, clip.y + clip.height);
    for (int y = first; y < last; y++) {
        *pixel_address(canvas, column.x, y) = color;
    }
}

static bool quad_visible(const Canvas *canvas, Quad quad, int *first, int *last)
{
    Rect clip = canvas->clip;
    *first = max_of(quad.left, clip.x);
    *last = min_of(quad.left + quad.width, clip.x + clip.width);
    return quad.width > 0 && *first < *last;
}

void image_draw_quad(Canvas *canvas, Image image, Quad quad)
{
    int first = 0;
    int last = 0;
    if (image.width <= 0 || image.height <= 0 || !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_column(canvas, column, &image, quad.width);
        }
    }
}

void image_draw_quad_lit(Canvas *canvas, Image image, Quad quad, QuadLight light)
{
    if (light.left >= COLOR_LIGHT_FULL && light.right >= COLOR_LIGHT_FULL) {
        image_draw_quad(canvas, image, quad);
        return;
    }
    int first = 0;
    int last = 0;
    if (image.width <= 0 || image.height <= 0 || !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_lit_column(canvas, column, &image, quad.width,
                            light_at(light, column, quad.width));
        }
    }
}

void image_draw_reflection(Canvas *canvas, Image image, Quad quad, QuadLight light, int rows)
{
    int first = 0;
    int last = 0;
    if (rows <= 0 || image.width <= 0 || image.height <= 0 ||
        !quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            draw_reflected_column(canvas, column, &image, quad.width,
                                  light_at(light, column, quad.width), rows);
        }
    }
}

void image_fill_quad(Canvas *canvas, Quad quad, Color color)
{
    int first = 0;
    int last = 0;
    if (!quad_visible(canvas, quad, &first, &last)) {
        return;
    }
    for (int x = first; x < last; x++) {
        Column column = column_at(quad, x);
        if (column.height > 0) {
            fill_column(canvas, column, color);
        }
    }
}
