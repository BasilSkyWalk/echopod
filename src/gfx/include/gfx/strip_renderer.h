#pragma once

#include <stdbool.h>

#include "gfx/canvas.h"

typedef struct FrameDrawer {
    void (*draw)(void *context, Canvas *canvas);
    void *context;
} FrameDrawer;

typedef struct FrameSink {
    void (*write)(void *context, Rect area, const Color *pixels);
    void *context;
} FrameSink;

typedef struct StripRenderer {
    Color *strip_pixels;
    int screen_width;
    int screen_height;
    int strip_height;
    Color background;
    bool drawer_paints_everything;
} StripRenderer;

void strip_renderer_render(const StripRenderer *renderer, FrameDrawer drawer, FrameSink sink);
