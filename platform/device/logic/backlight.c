#include "device/backlight.h"

enum {
    FULL_PERCENT = 100,
    PWM_HZ = 10000
};

static const uint8_t PERCENT_FOR_LEVEL[HAL_DISPLAY_BRIGHTNESS_MAX] = {8,  12, 17, 23, 30,
                                                                      39, 50, 63, 80, 100};

uint32_t backlight_percent(uint8_t level)
{
    if (level < HAL_DISPLAY_BRIGHTNESS_MIN) {
        level = HAL_DISPLAY_BRIGHTNESS_MIN;
    }
    if (level > HAL_DISPLAY_BRIGHTNESS_MAX) {
        level = HAL_DISPLAY_BRIGHTNESS_MAX;
    }
    return PERCENT_FOR_LEVEL[level - HAL_DISPLAY_BRIGHTNESS_MIN];
}

BacklightPwm backlight_pwm(uint32_t pclk_hz, uint32_t percent)
{
    if (percent >= FULL_PERCENT) {
        return (BacklightPwm){.full = true};
    }
    uint32_t period = pclk_hz / PWM_HZ;
    return (BacklightPwm){.full = false, .period = period, .duty = period * percent / FULL_PERCENT};
}

uint32_t backlight_ramp_percent(uint32_t target_percent, uint32_t elapsed_ms)
{
    if (elapsed_ms >= BACKLIGHT_RAMP_MS) {
        return target_percent;
    }
    uint32_t percent = target_percent * elapsed_ms / BACKLIGHT_RAMP_MS;
    return percent > 0 ? percent : 1;
}
