#pragma once

#include <stdint.h>

#include "device/core1_stream.h"
#include "registers.h"

enum {
    CORE1_TICKS_ENABLE = 0x7u,
    CORE1_TICK_PERIOD = 100000u,
    CORE1_LOAD_WINDOW = 50000000u
};

extern volatile uint32_t core1_tick_periods;

void core1_systick(void);

static inline void core1_ticks_start(void)
{
    register_write(SYSTICK_CTRL, 0);
    register_write(SYSTICK_RELOAD, CORE1_TICK_PERIOD - 1u);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SCB_ICSR, SCB_ICSR_PENDSTCLR);
    core1_tick_periods = 0;
    register_write(SYSTICK_CTRL, CORE1_TICKS_ENABLE);
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline uint32_t core1_ticks(void)
{
    uint32_t periods = 0;
    uint32_t current = 0;
    uint32_t pending = 0;
    do {
        periods = core1_tick_periods;
        current = register_read(SYSTICK_CURRENT);
        pending = register_read(SCB_ICSR) & SCB_ICSR_PENDSTSET;
    } while (periods != core1_tick_periods);
    if (pending != 0 && current > CORE1_TICK_PERIOD / 2u) {
        periods++;
    }
    return periods * CORE1_TICK_PERIOD + (CORE1_TICK_PERIOD - 1u - current);
}

static inline void core1_wait_for_event(void)
{
    __asm__ volatile("wfe" ::: "memory");
}

static inline void core1_ticks_measure(Core1Stream *stream)
{
    core1_stream_set_clock(stream, core1_ticks, UINT32_MAX, CORE1_LOAD_WINDOW);
    core1_stream_set_wait(stream, core1_wait_for_event);
}
