#include "ui/label.h"

#include "core/utf8.h"

enum {
    ELLIPSIS_CODEPOINT = 0x2026
};

static const char ELLIPSIS_TEXT[] = "\xE2\x80\xA6";

static int advance_of(const Font *font, uint32_t codepoint)
{
    const Glyph *glyph = font_find_glyph(font, codepoint);
    return glyph != NULL ? glyph->advance : 0;
}

LabelFit label_fit(const Font *font, const char *text, int max_width)
{
    int ellipsis_width = advance_of(font, ELLIPSIS_CODEPOINT);
    int used = 0;
    const char *cursor = text;
    const char *cut = text;
    for (uint32_t codepoint = utf8_next(&cursor); codepoint != 0; codepoint = utf8_next(&cursor)) {
        used += advance_of(font, codepoint);
        if (used + ellipsis_width <= max_width) {
            cut = cursor;
        }
        if (used > max_width) {
            return (LabelFit){.byte_count = (size_t)(cut - text), .ellipsis = true};
        }
    }
    return (LabelFit){.byte_count = (size_t)(cursor - text), .ellipsis = false};
}

int label_baseline(const Font *font, Rect area)
{
    int top = area.y + (area.height - font_line_height(font)) / 2;
    return top + font->ascent;
}

Rect label_centered_area(const Font *font, Rect bounds, const char *text)
{
    int width = text_width(font, text);
    if (width > bounds.width) {
        width = bounds.width;
    }
    return (Rect){bounds.x + (bounds.width - width) / 2, bounds.y, width, bounds.height};
}

void label_draw(Canvas *canvas, TextStyle style, Label label)
{
    Canvas clipped = canvas_clipped(canvas, label.area);
    if (rect_is_empty(clipped.clip)) {
        return;
    }
    LabelFit fit = label_fit(style.font, label.text, label.area.width);
    Point pen = {label.area.x, label_baseline(style.font, label.area)};
    pen.x = text_draw_span(&clipped, style, pen, (TextSpan){label.text, fit.byte_count});
    if (fit.ellipsis) {
        text_draw(&clipped, style, pen, ELLIPSIS_TEXT);
    }
}

void label_draw_scrolling(Canvas *canvas, TextStyle style, Label label, Marquee marquee)
{
    Canvas clipped = canvas_clipped(canvas, label.area);
    if (rect_is_empty(clipped.clip)) {
        return;
    }
    int baseline = label_baseline(style.font, label.area);
    int start = label.area.x - marquee.offset;
    int repeat_start = start + text_width(style.font, label.text) + marquee.gap;
    text_draw(&clipped, style, (Point){start, baseline}, label.text);
    text_draw(&clipped, style, (Point){repeat_start, baseline}, label.text);
}
