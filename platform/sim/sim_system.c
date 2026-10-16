#include "hal/system.h"

enum {
    BOOT_CLOCK_MHZ = 24
};

static uint32_t clock_mhz = BOOT_CLOCK_MHZ;

const char *hal_system_version(void)
{
    return "simulator";
}

HalBootClocks hal_system_boot_clocks(void)
{
    return (HalBootClocks){0};
}

uint32_t hal_system_clock_mhz(void)
{
    return clock_mhz;
}

bool hal_system_set_clock_mhz(uint32_t mhz)
{
    clock_mhz = mhz;
    return true;
}

HalLoopCycles hal_system_loop_cycles(void)
{
    return (HalLoopCycles){0};
}

HalCore1Test hal_system_core1_test(void)
{
    return (HalCore1Test){.status = HAL_CORE1_UNAVAILABLE};
}
