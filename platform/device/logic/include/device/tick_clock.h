#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    TICK_CLOCK_COUNTER_MASK = 0xFFFFFF
};

typedef struct TickClock {
    uint32_t ticks_per_microsecond;
    uint32_t period;
    uint32_t microsecond_ticks;
    uint32_t millisecond_ticks;
    uint32_t microseconds;
    uint32_t milliseconds;
} TickClock;

typedef struct TickReading {
    uint32_t counter;
    bool reload_pending;
} TickReading;

void tick_clock_init(TickClock *clock, uint32_t ticks_per_microsecond, uint32_t period);
void tick_clock_reload(TickClock *clock);
void tick_clock_set_rate(TickClock *clock, TickReading reading, uint32_t ticks_per_microsecond,
                         uint32_t period);
uint32_t tick_clock_milliseconds(const TickClock *clock, TickReading reading);
uint32_t tick_clock_microseconds(const TickClock *clock, TickReading reading);
