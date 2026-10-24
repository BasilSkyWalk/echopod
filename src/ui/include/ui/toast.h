#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "ui/theme.h"

enum {
    TOAST_DURATION_MS = 1500
};

typedef struct Toast {
    const char *text;
    uint32_t until_ms;
    bool visible;
} Toast;

void toast_show(Toast *toast, const char *text, uint32_t now_ms);
void toast_update(Toast *toast, uint32_t now_ms);
Rect toast_area(const Toast *toast, const Theme *theme);
void toast_draw(const Toast *toast, Canvas *canvas, const Theme *theme);
