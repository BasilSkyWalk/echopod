#include "sim_clock.h"

#include <SDL.h>
#include <stdbool.h>

#include "hal/clock.h"

enum {
    MICROSECONDS_PER_SECOND = 1000000
};

typedef struct SimClock {
    bool virtual_time;
    uint32_t virtual_ms;
} SimClock;

static SimClock clock_state;

void sim_clock_use_virtual_time(uint32_t start_ms)
{
    clock_state = (SimClock){.virtual_time = true, .virtual_ms = start_ms};
}

void sim_clock_advance(uint32_t milliseconds)
{
    clock_state.virtual_ms += milliseconds;
}

uint32_t hal_clock_milliseconds(void)
{
    return clock_state.virtual_time ? clock_state.virtual_ms : (uint32_t)SDL_GetTicks64();
}

uint32_t hal_clock_microseconds(void)
{
    uint64_t counter = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    uint64_t whole_seconds = counter / frequency;
    uint64_t remainder = counter % frequency;
    return (uint32_t)(whole_seconds * MICROSECONDS_PER_SECOND +
                      remainder * MICROSECONDS_PER_SECOND / frequency);
}
