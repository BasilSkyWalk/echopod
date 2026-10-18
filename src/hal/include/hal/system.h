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

typedef struct HalLoopCycles {
    uint32_t register_centi;
    uint32_t memory_centi;
} HalLoopCycles;

HalLoopCycles hal_system_loop_cycles(void);
void hal_system_stall_test(void);

typedef enum HalStartStep {
    HAL_START_STEP_NONE,
    HAL_START_STEP_BACKLIGHT_ON,
    HAL_START_STEP_BACKLIGHT_OFF,
    HAL_START_STEP_CLOCK_SWITCH,
    HAL_START_STEP_CORE1_START,
    HAL_START_STEP_CORE1_STOP,
    HAL_START_STEP_CARD_WRITE,
    HAL_START_STEP_COUNT
} HalStartStep;

typedef struct HalStartInfo {
    bool warm;
    uint32_t starts;
    uint32_t previous_uptime_ms;
    bool previous_power_off;
    uint32_t fault_status;
    uint32_t hard_fault_status;
    uint32_t fault_pc;
    HalStartStep last_step;
    bool last_step_finished;
    uint32_t last_step_ms;
    uint32_t last_millivolts;
} HalStartInfo;

HalStartInfo hal_system_start_info(void);

typedef enum HalCore1Status {
    HAL_CORE1_RAN,
    HAL_CORE1_UNAVAILABLE,
    HAL_CORE1_NEEDS_PLL,
    HAL_CORE1_NO_POWER,
    HAL_CORE1_NO_REPLY,
    HAL_CORE1_BAD_REPLY,
    HAL_CORE1_NO_STOP,
    HAL_CORE1_NO_TRACK,
    HAL_CORE1_NOT_FLAC,
    HAL_CORE1_READ_FAILED,
    HAL_CORE1_BUSY
} HalCore1Status;

typedef struct HalCore1Test {
    HalCore1Status status;
    uint32_t mhz;
    HalLoopCycles cycles;
} HalCore1Test;

HalCore1Test hal_system_core1_test(void);

typedef struct HalCore1Decode {
    HalCore1Status status;
    uint32_t mhz;
    uint32_t blocks;
    uint32_t average_us;
    uint32_t longest_us;
    uint32_t block_us;
} HalCore1Decode;

HalCore1Decode hal_system_core1_decode_test(void);
