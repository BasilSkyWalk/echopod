#include "device/watchdog.h"

enum {
    TOP_BASE_BITS = 16
};

uint32_t watchdog_top_for(uint32_t pclk_hz)
{
    uint64_t wanted = (uint64_t)pclk_hz * WATCHDOG_MIN_SECONDS;
    for (uint32_t top = 0; top < WATCHDOG_TOP_MAX; top++) {
        if ((UINT64_C(1) << (TOP_BASE_BITS + top)) >= wanted) {
            return top;
        }
    }
    return WATCHDOG_TOP_MAX;
}
