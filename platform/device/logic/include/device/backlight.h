#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/display.h"

enum {
    BACKLIGHT_RAMP_MS = 200
};

typedef struct BacklightPwm {
    bool full;
    uint32_t period;
    uint32_t duty;
} BacklightPwm;

uint32_t backlight_percent(uint8_t level);
uint32_t backlight_duty(uint32_t period, uint32_t percent);
BacklightPwm backlight_pwm(uint32_t pclk_hz, uint32_t percent);
uint32_t backlight_ramp_percent(uint32_t target_percent, uint32_t elapsed_ms);
