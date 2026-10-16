#include "hal/clock.h"
#include "board.h"
#include "device/tick_clock.h"
#include "hal/system.h"
#include "irq.h"
#include "loop_bench.h"
#include "registers.h"

#define SYSTICK_ENABLE_INTERRUPT_CORE_CLOCK 0x7u

enum {
    MICROSECONDS_PER_PERIOD = 1000,
    PERIODS_PER_KEY_SCAN = 5
};

static TickClock clock;
static uint32_t periods_since_scan;

static uint32_t period_of(uint32_t ticks_per_microsecond)
{
    return ticks_per_microsecond * MICROSECONDS_PER_PERIOD;
}

static bool reload_pending(void)
{
    return (register_read(SCB_ICSR) & SCB_ICSR_PENDSTSET) != 0;
}

static TickReading read_counter(void)
{
    bool pending = reload_pending();
    uint32_t counter = register_read(SYSTICK_CURRENT);
    if (!pending && reload_pending()) {
        pending = true;
        counter = register_read(SYSTICK_CURRENT);
    }
    return (TickReading){.counter = counter, .reload_pending = pending};
}

void board_clock_start(void)
{
    uint32_t ticks_per_microsecond = board_boot_ticks_per_microsecond();
    uint32_t period = period_of(ticks_per_microsecond);
    register_write(SYSTICK_CTRL, 0);
    register_write(SYSTICK_RELOAD, period - 1);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SCB_ICSR, SCB_ICSR_PENDSTCLR);
    tick_clock_init(&clock, ticks_per_microsecond, period);
    register_write(SYSTICK_CTRL, SYSTICK_ENABLE_INTERRUPT_CORE_CLOCK);
    irq_enable();
}

void board_clock_set_rate(uint32_t ticks_per_microsecond)
{
    uint32_t primask = irq_save();
    uint32_t period = period_of(ticks_per_microsecond);
    tick_clock_set_rate(&clock, read_counter(), ticks_per_microsecond, period);
    register_write(SYSTICK_RELOAD, period - 1);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SCB_ICSR, SCB_ICSR_PENDSTCLR);
    irq_restore(primask);
}

void board_systick_handler(void)
{
    tick_clock_reload(&clock);
    periods_since_scan++;
    if (periods_since_scan >= PERIODS_PER_KEY_SCAN) {
        periods_since_scan = 0;
        uint32_t now_ms = tick_clock_milliseconds(&clock, read_counter());
        board_input_sample(now_ms);
        board_start_record_tick(now_ms);
    }
}

uint32_t hal_clock_milliseconds(void)
{
    uint32_t primask = irq_save();
    uint32_t milliseconds = tick_clock_milliseconds(&clock, read_counter());
    irq_restore(primask);
    return milliseconds;
}

uint32_t hal_clock_microseconds(void)
{
    uint32_t primask = irq_save();
    uint32_t microseconds = tick_clock_microseconds(&clock, read_counter());
    irq_restore(primask);
    return microseconds;
}

uint32_t loop_bench_now(void)
{
    return hal_clock_microseconds();
}

uint32_t loop_bench_since(uint32_t start)
{
    return (hal_clock_microseconds() - start) * hal_system_clock_mhz();
}

void board_delay_ms(uint32_t milliseconds)
{
    uint32_t start = hal_clock_milliseconds();
    while (hal_clock_milliseconds() - start < milliseconds) {
    }
}

void board_delay_us(uint32_t microseconds)
{
    uint32_t start = hal_clock_microseconds();
    while (hal_clock_microseconds() - start < microseconds) {
    }
}
