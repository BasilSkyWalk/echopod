#include <stdint.h>

#include "board.h"
#include "device/start_record.h"
#include "hal/system.h"
#include "registers.h"

#define SCB_CFSR 0xE000ED28u
#define SCB_HFSR 0xE000ED2Cu

enum {
    STACKED_PC_WORD = 6
};

__attribute__((section(".noinit"))) static StartRecord record;
static StartInfo info;

void board_start_record_boot(void)
{
    info = start_record_boot(&record);
}

void board_start_record_tick(uint32_t uptime_ms)
{
    start_record_tick(&record, uptime_ms);
}

void board_start_record_power_off(void)
{
    start_record_power_off(&record);
}

void board_start_record_step(HalStartStep step)
{
    start_record_step_begin(&record, step);
}

void board_start_record_step_done(void)
{
    start_record_step_end(&record);
}

void board_start_record_battery(uint32_t millivolts)
{
    start_record_battery(&record, millivolts);
}

void device_fault_handler(const uint32_t *frame)
{
    start_record_fault(&record, register_read(SCB_CFSR), register_read(SCB_HFSR),
                       frame[STACKED_PC_WORD]);
    for (;;) {
    }
}

__attribute__((naked)) void device_fault_entry(void)
{
    __asm__ volatile("mrs r0, msp\n\tb device_fault_handler");
}

HalStartInfo hal_system_start_info(void)
{
    return (HalStartInfo){.warm = info.warm,
                          .starts = info.starts,
                          .previous_uptime_ms = info.previous_uptime_ms,
                          .previous_power_off = info.previous_power_off,
                          .fault_status = info.fault_status,
                          .hard_fault_status = info.hard_fault_status,
                          .fault_pc = info.fault_address,
                          .last_step = info.last_step,
                          .last_step_finished = info.last_step_finished,
                          .last_step_ms = info.last_step_ms,
                          .last_millivolts = info.last_millivolts};
}
