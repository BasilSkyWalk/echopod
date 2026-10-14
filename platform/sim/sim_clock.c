#include <SDL.h>

#include "hal/clock.h"

enum {
    MICROSECONDS_PER_SECOND = 1000000
};

uint32_t hal_clock_milliseconds(void)
{
    return (uint32_t)SDL_GetTicks64();
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
