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

void image_draw_quad(Canvas *canvas, Image image, Quad quad);
void image_fill_quad(Canvas *canvas, Quad quad, Color color);
