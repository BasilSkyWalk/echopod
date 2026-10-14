#pragma once

#include <stdbool.h>

typedef struct Rect {
    int x;
    int y;
    int width;
    int height;
} Rect;

bool rect_is_empty(Rect rect);
Rect rect_intersection(Rect first, Rect second);
