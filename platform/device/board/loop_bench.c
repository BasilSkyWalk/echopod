#include <stdint.h>

#include "device/tick_clock.h"
#include "hal/system.h"
#include "registers.h"

enum {
    LOOP_ITERATIONS = 100000,
    MEMORY_CELLS = 64,
    CENTI = 100
};

static volatile uint32_t cells[MEMORY_CELLS];

static uint32_t ticks_now(void)
{
    return register_read(SYSTICK_CURRENT);
}

static uint32_t elapsed_ticks(uint32_t start, uint32_t end)
{
    return (start - end) & TICK_CLOCK_COUNTER_MASK;
}

__attribute__((noinline)) static uint32_t time_register_loop(void)
{
    uint32_t start = ticks_now();
    for (uint32_t count = LOOP_ITERATIONS; count != 0; count--) {
        __asm__ volatile("");
    }
    return elapsed_ticks(start, ticks_now());
}

__attribute__((noinline)) static uint32_t time_memory_loop(void)
{
    uint32_t start = ticks_now();
    for (uint32_t count = 0; count < LOOP_ITERATIONS; count++) {
        cells[count % MEMORY_CELLS] += count;
    }
    return elapsed_ticks(start, ticks_now());
}

static uint32_t centi_per_iteration(uint32_t ticks)
{
    return (uint32_t)((uint64_t)ticks * CENTI / LOOP_ITERATIONS);
}

HalLoopCycles hal_system_loop_cycles(void)
{
    return (HalLoopCycles){.register_centi = centi_per_iteration(time_register_loop()),
                           .memory_centi = centi_per_iteration(time_memory_loop())};
}
