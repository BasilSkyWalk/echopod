#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "device/core1_control.h"
#include "hal/clock.h"
#include "hal/system.h"
#include "registers.h"

enum {
    LOWEST_PLL_MHZ = 96
};

extern const uint8_t _binary_core1_bin_start[];
extern const uint8_t _binary_core1_bin_end[];

static uint32_t bus_read(void *context, uint32_t address)
{
    (void)context;
    return register_read(address);
}

static void bus_write(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    register_write(address, value);
}

static void bus_delay_ms(void *context, uint32_t milliseconds)
{
    (void)context;
    board_delay_ms(milliseconds);
}

static uint32_t bus_now_ms(void *context)
{
    (void)context;
    return hal_clock_milliseconds();
}

static const Core1Bus BUS = {bus_read, bus_write, bus_delay_ms, bus_now_ms, NULL};

static HalCore1Status status_of(Core1Result result)
{
    switch (result) {
    case CORE1_OK:
        return HAL_CORE1_RAN;
    case CORE1_NO_POWER:
        return HAL_CORE1_NO_POWER;
    case CORE1_NO_REPLY:
        return HAL_CORE1_NO_REPLY;
    case CORE1_BAD_REPLY:
        return HAL_CORE1_BAD_REPLY;
    case CORE1_NO_STOP:
        return HAL_CORE1_NO_STOP;
    }
    return HAL_CORE1_BAD_REPLY;
}

HalCore1Test hal_system_core1_test(void)
{
    uint32_t pll_mhz = hal_system_clock_mhz();
    if (pll_mhz < LOWEST_PLL_MHZ) {
        return (HalCore1Test){.status = HAL_CORE1_NEEDS_PLL};
    }
    uint32_t divider = core1_divider_for(pll_mhz);
    HalCore1Test test = {.status = HAL_CORE1_RAN, .mhz = pll_mhz / divider};
    Core1Result result = core1_power_up(&BUS, pll_mhz, divider);
    if (result == CORE1_OK) {
        uint32_t length = (uint32_t)(_binary_core1_bin_end - _binary_core1_bin_start);
        result = core1_start(&BUS, _binary_core1_bin_start, length);
    }
    if (result == CORE1_OK) {
        test.cycles = (HalLoopCycles){.register_centi = register_read(CORE1_SHARED_REGISTER_CENTI),
                                      .memory_centi = register_read(CORE1_SHARED_MEMORY_CENTI)};
    }
    Core1Result stopped = core1_stop(&BUS);
    test.status = status_of(result != CORE1_OK ? result : stopped);
    return test;
}
