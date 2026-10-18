#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/display.h"

typedef struct BacklightPwm {
    bool full;
    uint32_t period;
    uint32_t duty;
} BacklightPwm;

uint32_t backlight_percent(uint8_t level);
BacklightPwm backlight_pwm(uint32_t pclk_hz, uint32_t percent);
