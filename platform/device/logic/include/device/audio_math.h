#pragma once

#include <stdint.h>

uint32_t i2c_clock_divider(uint32_t pclk_hz, uint32_t bus_khz);
uint32_t i2s_divider_truncated(uint32_t mclk_hz, uint32_t rate_hz);
uint32_t i2s_divider_rounded(uint32_t mclk_hz, uint32_t rate_hz);
uint32_t i2s_clock_register(uint32_t divider);
void tone_fill(int16_t *samples, int count, int16_t amplitude);
