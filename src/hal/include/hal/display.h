#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    HAL_DISPLAY_WIDTH = 320,
    HAL_DISPLAY_HEIGHT = 170,
    HAL_DISPLAY_LONG_SIDE = HAL_DISPLAY_WIDTH,
    HAL_DISPLAY_SHORT_SIDE = HAL_DISPLAY_HEIGHT,
    HAL_DISPLAY_BRIGHTNESS_MIN = 1,
    HAL_DISPLAY_BRIGHTNESS_MAX = 10
};

typedef enum HalDisplayRotation {
    HAL_DISPLAY_LANDSCAPE,
    HAL_DISPLAY_PORTRAIT,
    HAL_DISPLAY_PORTRAIT_FLIPPED
} HalDisplayRotation;

void hal_display_set_rotation(HalDisplayRotation rotation);
void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels);
void hal_display_present(void);
void hal_display_set_power(bool on);
void hal_display_set_brightness(uint8_t level);
