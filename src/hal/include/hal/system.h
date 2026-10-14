#pragma once

#include <stdbool.h>
#include <stdint.h>

const char *hal_system_version(void);
uint32_t hal_system_clock_mhz(void);
bool hal_system_set_clock_mhz(uint32_t mhz);

typedef struct HalBootClocks {
    uint32_t mode;
    uint32_t clock_select;
    uint32_t pclk_select;
    uint32_t pll_con0;
    uint32_t pll_con1;
    uint32_t core_hz;
} HalBootClocks;

HalBootClocks hal_system_boot_clocks(void);
