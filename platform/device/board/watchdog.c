#include "device/watchdog.h"
#include "board.h"
#include "hal/system.h"
#include "registers.h"

#define WDT_BASE 0x40120000u
#define WDT_CONTROL (WDT_BASE + 0x00u)
#define WDT_TIMEOUT (WDT_BASE + 0x04u)
#define WDT_RESTART (WDT_BASE + 0x0Cu)
#define CRU_WATCHDOG_GATE (CRU_BASE + 0xA0u)
#define CRU_WATCHDOG_RESET (CRU_BASE + 0xC4u)
#define GATE_OPEN 0x80000000u
#define RESET_ASSERT 0x80008000u
#define RESET_RELEASE 0x80000000u
#define CONTROL_STOPPED 0x14u
#define CONTROL_RUNNING 0x15u
#define RESTART_KEY 0x76u

enum {
    RESET_PULSE_US = 1000
};

static bool running;

void board_watchdog_start(void)
{
    register_write(CRU_WATCHDOG_GATE, GATE_OPEN);
    register_write(CRU_WATCHDOG_RESET, RESET_ASSERT);
    board_delay_us(RESET_PULSE_US);
    register_write(CRU_WATCHDOG_RESET, RESET_RELEASE);
    register_write(WDT_CONTROL, CONTROL_STOPPED);
    register_write(WDT_TIMEOUT, watchdog_top_for(board_pclk_hz()));
    register_write(WDT_RESTART, RESTART_KEY);
    register_write(WDT_CONTROL, CONTROL_RUNNING);
    running = true;
}

void board_watchdog_kick(void)
{
    if (running) {
        register_write(WDT_RESTART, RESTART_KEY);
    }
}

void board_watchdog_after_clock_change(void)
{
    if (running) {
        register_write(WDT_TIMEOUT, watchdog_top_for(board_pclk_hz()));
        register_write(WDT_RESTART, RESTART_KEY);
    }
}

void hal_system_watchdog_test(void)
{
    board_start_record_step(HAL_START_STEP_WATCHDOG_TEST);
    __asm__ volatile("cpsid i" ::: "memory");
    for (;;) {
    }
}
