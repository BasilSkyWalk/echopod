#include "device/tick_clock.h"

enum {
    MICROSECONDS_PER_MILLISECOND = 1000
};

static uint32_t ticks_per_millisecond(const TickClock *clock)
{
    return clock->ticks_per_microsecond * MICROSECONDS_PER_MILLISECOND;
}

static uint32_t ticks_into_period(const TickClock *clock, TickReading reading)
{
    uint32_t position = (clock->period - reading.counter % clock->period) % clock->period;
    return reading.reload_pending ? clock->period + position : position;
}

static void add_ticks(TickClock *clock, uint32_t ticks)
{
    clock->microsecond_ticks += ticks;
    clock->microseconds += clock->microsecond_ticks / clock->ticks_per_microsecond;
    clock->microsecond_ticks %= clock->ticks_per_microsecond;
    clock->millisecond_ticks += ticks;
    clock->milliseconds += clock->millisecond_ticks / ticks_per_millisecond(clock);
    clock->millisecond_ticks %= ticks_per_millisecond(clock);
}

void tick_clock_init(TickClock *clock, uint32_t ticks_per_microsecond, uint32_t period)
{
    *clock = (TickClock){.ticks_per_microsecond = ticks_per_microsecond, .period = period};
}

void tick_clock_reload(TickClock *clock)
{
    add_ticks(clock, clock->period);
}

void tick_clock_set_rate(TickClock *clock, TickReading reading, uint32_t ticks_per_microsecond,
                         uint32_t period)
{
    add_ticks(clock, ticks_into_period(clock, reading));
    uint32_t old_rate = clock->ticks_per_microsecond;
    clock->microsecond_ticks = clock->microsecond_ticks * ticks_per_microsecond / old_rate;
    clock->millisecond_ticks = clock->millisecond_ticks * ticks_per_microsecond / old_rate;
    clock->ticks_per_microsecond = ticks_per_microsecond;
    clock->period = period;
}

uint32_t tick_clock_milliseconds(const TickClock *clock, TickReading reading)
{
    uint32_t ticks = clock->millisecond_ticks + ticks_into_period(clock, reading);
    return clock->milliseconds + ticks / ticks_per_millisecond(clock);
}

uint32_t tick_clock_microseconds(const TickClock *clock, TickReading reading)
{
    uint32_t ticks = clock->microsecond_ticks + ticks_into_period(clock, reading);
    return clock->microseconds + ticks / clock->ticks_per_microsecond;
}
