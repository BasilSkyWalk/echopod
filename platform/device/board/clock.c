#include "hal/clock.h"
#include "board.h"
#include "device/tick_clock.h"
#include "registers.h"

#define SYSTICK_ENABLE_CORE_CLOCK 0x5u

enum {
    TICKS_PER_MICROSECOND = 24
};

static TickClock clock;

void board_clock_start(void)
{
    register_write(SYSTICK_RELOAD, TICK_CLOCK_COUNTER_MASK);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SYSTICK_CTRL, SYSTICK_ENABLE_CORE_CLOCK);
    tick_clock_init(&clock, TICKS_PER_MICROSECOND, register_read(SYSTICK_CURRENT));
}

static void update(void)
{
    tick_clock_update(&clock, register_read(SYSTICK_CURRENT));
}

uint32_t hal_clock_milliseconds(void)
{
    update();
    return tick_clock_milliseconds(&clock);
}

uint32_t hal_clock_microseconds(void)
{
    update();
    return tick_clock_microseconds(&clock);
}

void board_delay_ms(uint32_t milliseconds)
{
    uint32_t start = hal_clock_milliseconds();
    while (hal_clock_milliseconds() - start < milliseconds) {
    }
}
