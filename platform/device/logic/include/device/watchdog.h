#pragma once

#include <stdint.h>

enum {
    WATCHDOG_TOP_MAX = 15,
    WATCHDOG_MIN_SECONDS = 8
};

uint32_t watchdog_top_for(uint32_t pclk_hz);
