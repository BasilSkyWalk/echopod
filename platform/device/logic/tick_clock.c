#include "device/tick_clock.h"

enum {
    MICROSECONDS_PER_MILLISECOND = 1000
};

void tick_clock_init(TickClock *clock, uint32_t ticks_per_microsecond, uint32_t counter)
{
    *clock = (TickClock){.ticks_per_microsecond = ticks_per_microsecond,
                         .last_counter = counter & TICK_CLOCK_COUNTER_MASK};
}

void tick_clock_update(TickClock *clock, uint32_t counter)
{
    uint32_t now = counter & TICK_CLOCK_COUNTER_MASK;
    uint32_t elapsed = (clock->last_counter - now) & TICK_CLOCK_COUNTER_MASK;
    uint32_t ticks_per_millisecond = clock->ticks_per_microsecond * MICROSECONDS_PER_MILLISECOND;
    clock->last_counter = now;
    clock->microsecond_ticks += elapsed;
    clock->microseconds += clock->microsecond_ticks / clock->ticks_per_microsecond;
    clock->microsecond_ticks %= clock->ticks_per_microsecond;
    clock->millisecond_ticks += elapsed;
    clock->milliseconds += clock->millisecond_ticks / ticks_per_millisecond;
    clock->millisecond_ticks %= ticks_per_millisecond;
}

uint32_t tick_clock_milliseconds(const TickClock *clock)
{
    return clock->milliseconds;
}

uint32_t tick_clock_microseconds(const TickClock *clock)
{
    return clock->microseconds;
}
