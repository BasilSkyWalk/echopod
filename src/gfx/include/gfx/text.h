#pragma once

#include <stddef.h>

#include "gfx/canvas.h"
#include "gfx/font.h"

typedef struct TextStyle {
    const Font *font;
    Color color;
} TextStyle;

typedef struct TextSpan {
    const char *start;
    size_t byte_count;
} TextSpan;

int text_width(const Font *font, const char *text);
int text_draw(Canvas *canvas, TextStyle style, Point pen, const char *text);
int text_draw_span(Canvas *canvas, TextStyle style, Point pen, TextSpan span);
