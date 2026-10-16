#pragma once

#include <stdint.h>

enum {
    TICK_CLOCK_COUNTER_MASK = 0xFFFFFF
};

typedef struct TickClock {
    uint32_t ticks_per_microsecond;
    uint32_t period;
    uint32_t last_counter;
    uint32_t microsecond_ticks;
    uint32_t millisecond_ticks;
    uint32_t microseconds;
    uint32_t milliseconds;
} TickClock;

void tick_clock_init(TickClock *clock, uint32_t ticks_per_microsecond, uint32_t period,
                     uint32_t counter);
void tick_clock_update(TickClock *clock, uint32_t counter);
void tick_clock_set_rate(TickClock *clock, uint32_t ticks_per_microsecond, uint32_t period,
                         uint32_t counter);
uint32_t tick_clock_milliseconds(const TickClock *clock);
uint32_t tick_clock_microseconds(const TickClock *clock);
