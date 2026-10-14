#pragma once

#include <stdint.h>

#include "core/key_tracker.h"
#include "gfx/canvas.h"

typedef struct Screen {
    void *context;
    void (*handle_key)(void *context, KeyEvent event, uint32_t now_ms);
    void (*update)(void *context, uint32_t now_ms);
    void (*draw)(void *context, Canvas *canvas);
} Screen;
