#include "gfx/text.h"

#include <stddef.h>

#include "core/utf8.h"

static Rect glyph_area(const Glyph *glyph, Point pen)
{
    return (Rect){pen.x + glyph->left, pen.y - glyph->top, glyph->width, glyph->height};
}

static void draw_glyph(Canvas *canvas, TextStyle style, const Glyph *glyph, Point pen)
{
    Rect area = glyph_area(glyph, pen);
    Rect visible = rect_intersection(area, canvas->clip);
    if (rect_is_empty(visible)) {
        return;
    }
    int stride = canvas->bounds.width;
    Color *row =
        &canvas->pixels[(visible.y - canvas->bounds.y) * stride + (visible.x - canvas->bounds.x)];
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        Color *pixel = row;
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            uint8_t level = font_glyph_level(style.font, glyph, x - area.x, y - area.y);
            if (level > 0) {
                *pixel = color_blend(*pixel, style.color, level);
            }
            pixel++;
        }
        row += stride;
    }
}

int text_width(const Font *font, const char *text)
{
    int width = 0;
    const char *cursor = text;
    for (uint32_t codepoint = utf8_next(&cursor); codepoint != 0; codepoint = utf8_next(&cursor)) {
        const Glyph *glyph = font_find_glyph(font, codepoint);
        if (glyph != NULL) {
            width += glyph->advance;
        }
    }
    return width;
}

static size_t byte_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

int text_draw(Canvas *canvas, TextStyle style, Point pen, const char *text)
{
    return text_draw_span(canvas, style, pen, (TextSpan){text, byte_length(text)});
}

int text_draw_span(Canvas *canvas, TextStyle style, Point pen, TextSpan span)
{
    const char *cursor = span.start;
    const char *end = span.start + span.byte_count;
    while (cursor < end) {
        const Glyph *glyph = font_find_glyph(style.font, utf8_next(&cursor));
        if (glyph == NULL) {
            continue;
        }
        draw_glyph(canvas, style, glyph, pen);
        pen.x += glyph->advance;
    }
    return pen.x;
}
