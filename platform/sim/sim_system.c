#include "core/wall_clock.h"
#include "hal/system.h"
#include "library/library_scan.h"

enum {
    BOOT_CLOCK_MHZ = 24,
    BUILD_YEAR_DIVISOR = 10000,
    BUILD_MONTH_DIVISOR = 100
};

static uint32_t clock_mhz = BOOT_CLOCK_MHZ;
static uint32_t savings = HAL_SAVINGS_DEFAULT;

void hal_system_set_power_savings(uint32_t wanted)
{
    savings = wanted;
}

uint32_t hal_system_power_savings(void)
{
    return savings;
}

HalPowerInfo hal_system_power_info(void)
{
    return (HalPowerInfo){.clock_mhz = clock_mhz, .panel_on = true};
}

const char *hal_system_version(void)
{
    return "simulator";
}

uint32_t hal_system_build_day(void)
{
    uint32_t date = ECHO_SIM_BUILD_DATE;
    return wall_clock_days_from_civil(date / BUILD_YEAR_DIVISOR,
                                      date / BUILD_MONTH_DIVISOR % BUILD_MONTH_DIVISOR,
                                      date % BUILD_MONTH_DIVISOR);
}

HalBootClocks hal_system_boot_clocks(void)
{
    return (HalBootClocks){0};
}

HalUsbCore hal_system_usb_core(void)
{
    return (HalUsbCore){.captured = false};
}

HalUsbLast hal_system_usb_last(void)
{
    return (HalUsbLast){.end = HAL_USB_END_NONE};
}

void hal_system_set_dac_volume(uint8_t level)
{
    (void)level;
}

void hal_system_set_usb_mode(HalUsbMode mode)
{
    (void)mode;
}

void hal_system_set_usb_prompt(bool ask)
{
    (void)ask;
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

bool hal_system_restart_pending(void)
{
    return false;
}

const void *hal_system_load_rescan_code(void)
{
    return &LIBRARY_SCAN_ENTRY;
}

HalHighRamTest hal_system_high_ram_test(void)
{
    return (HalHighRamTest){.status = HAL_HIGH_RAM_UNAVAILABLE};
}

void *hal_system_borrow_scratch(uint32_t *size)
{
    static uint8_t scratch[384 * 1024];
    *size = sizeof scratch;
    return scratch;
}

void *hal_system_borrow_listing(uint32_t *size)
{
    static uint8_t listing[96 * 1024];
    *size = sizeof listing;
    return listing;
}

void hal_system_return_scratch(void)
{
}

void hal_system_stall_test(void)
{
}

void hal_system_keep_alive(void)
{
}

HalLoopCycles hal_system_loop_cycles(void)
{
    return (HalLoopCycles){0};
}

HalCore1Test hal_system_core1_test(void)
{
    return (HalCore1Test){.status = HAL_CORE1_UNAVAILABLE};
}

HalCore1Accel hal_system_core1_accel_test(void)
{
    return (HalCore1Accel){.status = HAL_CORE1_UNAVAILABLE};
}

HalStartInfo hal_system_start_info(void)
{
    return (HalStartInfo){.warm = false, .starts = 1};
}

HalStackUsage hal_system_stack_usage(void)
{
    return (HalStackUsage){.used = 0, .capacity = 0};
}

void hal_system_watchdog_test(void)
{
}
