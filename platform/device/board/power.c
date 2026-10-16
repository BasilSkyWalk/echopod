#include "hal/power.h"
#include "board.h"
#include "gpio.h"
#include "usb.h"

static const GpioPin POWER_KEY = {2, 5};
static const GpioPin POWER_HOLD = {2, 6};
static const GpioPin BACKLIGHT = {2, 4};
static const GpioPin CHARGE_STATUS = {2, 3};

static bool charge_status_ready;
static HalPowerHold boot_hold;

void board_power_hold(void)
{
    boot_hold = (HalPowerHold){.function = (uint8_t)gpio_function(POWER_HOLD),
                               .output = gpio_is_output(POWER_HOLD),
                               .high = gpio_output_level(POWER_HOLD)};
    gpio_mux(POWER_HOLD, GPIO_FUNCTION_GPIO);
    gpio_output(POWER_HOLD, true);
}

HalPowerHold hal_power_boot_hold(void)
{
    return boot_hold;
}
static BatteryDisplay display;

static bool charging_now(bool usb_power)
{
    if (!charge_status_ready) {
        gpio_mux(CHARGE_STATUS, GPIO_FUNCTION_GPIO);
        gpio_input(CHARGE_STATUS);
        charge_status_ready = true;
    }
    return usb_power && !gpio_read(CHARGE_STATUS);
}

HalBattery hal_power_battery(void)
{
    BatteryGauge gauge = board_battery_gauge();
    bool usb_power = usb_vbus_present();
    bool charging = charging_now(usb_power);
    if (!battery_gauge_known(&gauge)) {
        return (HalBattery){.known = false, .usb_power = usb_power, .charging = charging};
    }
    return (HalBattery){
        .known = true,
        .percent = battery_display_update(&display, battery_gauge_percent(&gauge, charging)),
        .charging = charging,
        .usb_power = usb_power,
        .millivolts = (uint16_t)battery_gauge_millivolts(&gauge)};
}

void hal_power_off(void)
{
    while (gpio_read(POWER_KEY)) {
    }
    gpio_output(BACKLIGHT, false);
    __asm__ volatile("cpsid i" ::: "memory");
    gpio_mux(POWER_HOLD, GPIO_FUNCTION_GPIO);
    gpio_output(POWER_HOLD, false);
    for (;;) {
    }
}
