#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "gfx/rect.h"

enum {
    SHAPE_TURN = 256,
    SHAPE_UNIT = 255,
    SHAPE_Q4 = 16
};

typedef struct ShapeSpan {
    int16_t y;
    int16_t left_q4;
    int16_t right_q4;
} ShapeSpan;

typedef struct ShapeRing {
    int16_t x_q4;
    int16_t y_q4;
    int16_t outer_q4;
    int16_t inner_q4;
} ShapeRing;

typedef struct ShapeSpoke {
    Point pivot;
    uint8_t angle;
    uint8_t width_q4;
    int16_t from;
    int16_t to;
} ShapeSpoke;

typedef struct SegmentStyle {
    uint8_t width;
    uint8_t height;
    uint8_t thickness;
    uint8_t lean;
    Color lit;
    Color unlit;
} SegmentStyle;

int shape_sine(uint8_t angle);
int shape_cosine(uint8_t angle);
uint32_t shape_isqrt(uint32_t value);
Point shape_polar(Point centre, uint8_t angle, int radius);
void shape_span(Canvas *canvas, ShapeSpan span, Color color);
void shape_ring(Canvas *canvas, ShapeRing ring, Color color);
void shape_rounded(Canvas *canvas, Rect area, int radius, Color color);
void shape_trapezoid(Canvas *canvas, Rect area, int top_inset, Color color);
void shape_spoke(Canvas *canvas, const ShapeSpoke *spoke, Color color);
void shape_digit(Canvas *canvas, const SegmentStyle *style, Point origin, uint8_t mask);
void shape_colon(Canvas *canvas, const SegmentStyle *style, Point origin, bool lit);
uint8_t shape_digit_mask(unsigned digit);
void shape_lattice(Canvas *canvas, Rect area, int pitch, Color color);
