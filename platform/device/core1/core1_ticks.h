#pragma once

#include <stdint.h>

#include "device/core1_stream.h"
#include "device/tick_clock.h"
#include "registers.h"

enum {
    CORE1_TICKS_ENABLE = 0x5u,
    CORE1_LOAD_WINDOW = 50000000u
};

static inline void core1_ticks_start(void)
{
    register_write(SYSTICK_RELOAD, TICK_CLOCK_COUNTER_MASK);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SYSTICK_CTRL, CORE1_TICKS_ENABLE);
}

static inline uint32_t core1_ticks(void)
{
    return ~register_read(SYSTICK_CURRENT) & TICK_CLOCK_COUNTER_MASK;
}

static inline void core1_ticks_measure(Core1Stream *stream)
{
    core1_stream_set_clock(stream, core1_ticks, TICK_CLOCK_COUNTER_MASK, CORE1_LOAD_WINDOW);
}
