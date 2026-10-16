#include <stdint.h>

#include "device/core1_control.h"
#include "device/tick_clock.h"
#include "hal/system.h"
#include "registers.h"

#define SYSTICK_ENABLE_CORE_CLOCK 0x5u

void core1_main(void);

static void start_systick(void)
{
    register_write(SYSTICK_RELOAD, TICK_CLOCK_COUNTER_MASK);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SYSTICK_CTRL, SYSTICK_ENABLE_CORE_CLOCK);
}

void core1_main(void)
{
    start_systick();
    HalLoopCycles cycles = hal_system_loop_cycles();
    register_write(CORE1_SHARED_REGISTER_CENTI, cycles.register_centi);
    register_write(CORE1_SHARED_MEMORY_CENTI, cycles.memory_centi);
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
}
