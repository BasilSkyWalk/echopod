#pragma once

#include "gfx/canvas.h"

typedef struct Image {
    const Color *pixels;
    int width;
    int height;
} Image;

typedef struct Quad {
    int left;
    int width;
    int left_top;
    int left_height;
    int right_top;
    int right_height;
} Quad;

typedef struct QuadLight {
    uint16_t left;
    uint16_t right;
} QuadLight;

void image_draw_quad(Canvas *canvas, Image image, Quad quad);
void image_draw_quad_lit(Canvas *canvas, Image image, Quad quad, QuadLight light);
void image_draw_reflection(Canvas *canvas, Image image, Quad quad, QuadLight light, int rows);
void image_fill_quad(Canvas *canvas, Quad quad, Color color);
