#include "hal/power.h"
#include "board.h"
#include "gpio.h"
#include "registers.h"
#include "usb.h"

#define GRF_PORT2_LOW_PULL (GRF_BASE + 0x50u)
#define POWER_KEY_PULL_OFF 0x00200000u

static const GpioPin POWER_KEY = {2, 5};
static const GpioPin POWER_HOLD = {2, 6};
static const GpioPin CHARGE_STATUS = {2, 3};

static bool charge_status_ready;
static HalPowerHold boot_hold;
static bool usb_start;

void board_power_hold(void)
{
    boot_hold = (HalPowerHold){.function = (uint8_t)gpio_function(POWER_HOLD),
                               .output = gpio_is_output(POWER_HOLD),
                               .high = gpio_output_level(POWER_HOLD)};
    gpio_mux(POWER_HOLD, GPIO_FUNCTION_GPIO);
    gpio_output(POWER_HOLD, true);
    gpio_mux(POWER_KEY, GPIO_FUNCTION_GPIO);
    gpio_input(POWER_KEY);
    register_write(GRF_PORT2_LOW_PULL, POWER_KEY_PULL_OFF);
    boot_hold.usb_power = usb_vbus_present();
    boot_hold.key_down = gpio_read(POWER_KEY);
    bool held_by_key = boot_hold.output && boot_hold.high;
    usb_start = boot_hold.usb_power && !boot_hold.key_down && !held_by_key;
}

HalPowerHold hal_power_boot_hold(void)
{
    return boot_hold;
}

bool hal_power_take_usb_start(void)
{
    bool started = usb_start;
    usb_start = false;
    return started;
}
static BatteryDisplay display;
static bool usb_seen;
static bool usb_before;

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
    bool usb_power = usb_vbus_present();
    if (usb_seen && usb_power != usb_before) {
        board_battery_restart();
    }
    usb_seen = true;
    usb_before = usb_power;
    BatteryGauge gauge = board_battery_gauge();
    bool charging = charging_now(usb_power);
    if (!battery_gauge_known(&gauge)) {
        return (HalBattery){.known = false, .usb_power = usb_power, .charging = charging};
    }
    return (HalBattery){.known = true,
                        .percent = battery_display_update(
                            &display, battery_gauge_percent(&gauge, charging), usb_power),
                        .charging = charging,
                        .usb_power = usb_power,
                        .millivolts = (uint16_t)battery_gauge_recent_millivolts(&gauge)};
}

void board_power_cut(void)
{
    board_backlight_off();
    board_bluetooth_off();
    __asm__ volatile("cpsid i" ::: "memory");
    board_start_record_power_off();
    gpio_mux(POWER_HOLD, GPIO_FUNCTION_GPIO);
    gpio_output(POWER_HOLD, false);
    for (;;) {
        board_watchdog_kick();
    }
}

bool board_power_key_down(void)
{
    return gpio_read(POWER_KEY);
}

void hal_power_off(void)
{
    board_stall_pause(true);
    while (gpio_read(POWER_KEY)) {
    }
    board_backlight_off();
    board_bluetooth_off();
    __asm__ volatile("cpsid i" ::: "memory");
    board_start_record_power_off();
    gpio_mux(POWER_HOLD, GPIO_FUNCTION_GPIO);
    gpio_output(POWER_HOLD, false);
    for (;;) {
        board_watchdog_kick();
    }
}
