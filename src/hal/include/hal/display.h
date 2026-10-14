#pragma once

#include <stdint.h>

enum {
    HAL_DISPLAY_WIDTH = 320,
    HAL_DISPLAY_HEIGHT = 170
};

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels);
void hal_display_present(void);
