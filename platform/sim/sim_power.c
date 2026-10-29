#include "sim_power.h"

#include "hal/power.h"

typedef struct SimPower {
    HalBattery battery;
    bool off_requested;
} SimPower;

enum {
    EMPTY_MILLIVOLTS = 3450,
    MILLIVOLTS_PER_PERCENT_TENFOLD = 75
};

static SimPower power;

void sim_power_configure(uint8_t battery_percent, bool charging)
{
    uint32_t millivolts = EMPTY_MILLIVOLTS + battery_percent * MILLIVOLTS_PER_PERCENT_TENFOLD / 10;
    power = (SimPower){
        .battery = {.known = true,
                    .percent = battery_percent,
                    .charging = charging,
                    .usb_power = charging,
                    .millivolts = (uint16_t)millivolts},
        .off_requested = false,
    };
}

bool sim_power_off_requested(void)
{
    return power.off_requested;
}

HalBattery hal_power_battery(void)
{
    return power.battery;
}

void hal_power_off(void)
{
    power.off_requested = true;
}

HalPowerHold hal_power_boot_hold(void)
{
    return (HalPowerHold){.function = 0, .output = true, .high = true};
}

bool hal_power_take_usb_start(void)
{
    return false;
}
