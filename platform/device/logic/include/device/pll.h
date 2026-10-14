#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct PllSetting {
    uint32_t con0;
    uint32_t con1;
    uint32_t clock_select;
    uint32_t pclk_select;
    uint32_t core_speed_bits;
    uint32_t bus_speed_bits;
    uint32_t core_hz;
    uint32_t pclk_hz;
} PllSetting;

bool pll_setting_for(uint32_t core_mhz, PllSetting *setting);
