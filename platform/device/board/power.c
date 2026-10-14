#include "hal/power.h"
#include "board.h"
#include "gpio.h"

static const GpioPin POWER_KEY = {2, 5};
static const GpioPin POWER_HOLD = {2, 6};

HalBattery hal_power_battery(void)
{
    return (HalBattery){.known = false, .percent = 0, .charging = false};
}

void hal_power_off(void)
{
    while (gpio_read(POWER_KEY)) {
    }
    __asm__ volatile("cpsid i" ::: "memory");
    gpio_mux(POWER_HOLD, GPIO_FUNCTION_GPIO);
    gpio_output(POWER_HOLD, false);
    for (;;) {
    }
}
