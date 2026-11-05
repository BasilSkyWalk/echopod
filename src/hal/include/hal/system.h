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

typedef struct HalUsbCore {
    bool captured;
    uint32_t hwcfg2;
    uint32_t hwcfg3;
    uint32_t hwcfg4;
    uint32_t usbcfg;
    uint32_t rx_fifo;
    uint32_t nptx_fifo;
} HalUsbCore;

HalUsbCore hal_system_usb_core(void);

typedef enum HalUsbEnd {
    HAL_USB_END_NONE,
    HAL_USB_END_EJECTED,
    HAL_USB_END_UNPLUGGED,
    HAL_USB_END_NO_HOST
} HalUsbEnd;

typedef struct HalUsbLast {
    HalUsbEnd end;
    uint8_t left_mounted;
    uint32_t internal_written;
    uint32_t card_written;
    uint32_t writes_after_eject;
    uint32_t internal_errors;
    uint32_t card_errors;
    uint32_t card_retries;
    uint32_t card_starts;
    uint8_t card_step;
    uint32_t card_interrupts;
    uint32_t host_resets;
    uint32_t vbus_dips;
    bool fell_back;
    bool full_speed;
} HalUsbLast;

HalUsbLast hal_system_usb_last(void);

typedef enum HalUsbMode {
    HAL_USB_MODE_STORAGE,
    HAL_USB_MODE_DAC
} HalUsbMode;

void hal_system_set_usb_mode(HalUsbMode mode);
void hal_system_set_dac_volume(uint8_t level);
void hal_system_set_usb_prompt(bool ask);

typedef struct HalLoopCycles {
    uint32_t register_centi;
    uint32_t memory_centi;
} HalLoopCycles;

HalLoopCycles hal_system_loop_cycles(void);
void hal_system_stall_test(void);
void hal_system_watchdog_test(void);
void hal_system_keep_alive(void);
typedef enum HalHighRamStatus {
    HAL_HIGH_RAM_OK,
    HAL_HIGH_RAM_UNAVAILABLE,
    HAL_HIGH_RAM_BAD_MEMORY,
    HAL_HIGH_RAM_BAD_CALL
} HalHighRamStatus;

typedef struct HalHighRamTest {
    HalHighRamStatus status;
    uint32_t size;
    uint32_t bad_address;
    uint32_t call_result;
} HalHighRamTest;

HalHighRamTest hal_system_high_ram_test(void);
bool hal_system_restart_pending(void);
const void *hal_system_load_rescan_code(void);
void *hal_system_borrow_scratch(uint32_t *size);
void *hal_system_borrow_listing(uint32_t *size);
void hal_system_return_scratch(void);

typedef enum HalStartStep {
    HAL_START_STEP_NONE,
    HAL_START_STEP_BACKLIGHT_ON,
    HAL_START_STEP_BACKLIGHT_OFF,
    HAL_START_STEP_CLOCK_SWITCH,
    HAL_START_STEP_CORE1_START,
    HAL_START_STEP_CORE1_STOP,
    HAL_START_STEP_CARD_WRITE,
    HAL_START_STEP_WATCHDOG_TEST,
    HAL_START_STEP_COUNT
} HalStartStep;

typedef enum HalUpdateResult {
    HAL_UPDATE_NONE,
    HAL_UPDATE_INSTALLED,
    HAL_UPDATE_FAILED,
    HAL_UPDATE_REFUSED,
    HAL_UPDATE_ALREADY_INSTALLED,
    HAL_UPDATE_RESULT_COUNT
} HalUpdateResult;

typedef struct HalUpdateRecord {
    HalUpdateResult result;
    uint8_t detail;
    bool emmc_replied;
    uint32_t emmc_status;
} HalUpdateRecord;

enum {
    HAL_STALL_STACK_WORDS = 3
};

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
    bool stalled;
    uint32_t stall_lr;
    uint32_t stall_sp;
    uint32_t stall_stack[HAL_STALL_STACK_WORDS];
    uint32_t stall_stack_used;
    HalUpdateRecord last_update;
} HalStartInfo;

HalStartInfo hal_system_start_info(void);

typedef struct HalStackUsage {
    uint32_t used;
    uint32_t capacity;
} HalStackUsage;

HalStackUsage hal_system_stack_usage(void);

typedef enum HalCore1Status {
    HAL_CORE1_RAN,
    HAL_CORE1_UNAVAILABLE,
    HAL_CORE1_NEEDS_PLL,
    HAL_CORE1_NO_POWER,
    HAL_CORE1_NO_REPLY,
    HAL_CORE1_BAD_REPLY,
    HAL_CORE1_NO_STOP,
    HAL_CORE1_BUSY
} HalCore1Status;

typedef struct HalCore1Test {
    HalCore1Status status;
    uint32_t mhz;
    HalLoopCycles cycles;
} HalCore1Test;

HalCore1Test hal_system_core1_test(void);

typedef struct HalCore1Accel {
    HalCore1Status status;
    uint32_t mhz;
    bool narrow_ok;
    bool wide_ok;
    bool timed_out;
    uint8_t failed_vector;
    uint32_t failed_sample;
    uint32_t hardware_us;
    uint32_t software_us;
} HalCore1Accel;

HalCore1Accel hal_system_core1_accel_test(void);
