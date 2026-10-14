#include "sim_power.h"

#include "hal/power.h"

typedef struct SimPower {
    HalBattery battery;
    bool off_requested;
} SimPower;

static SimPower power;

void sim_power_configure(uint8_t battery_percent, bool charging)
{
    power = (SimPower){
        .battery = {.known = true, .percent = battery_percent, .charging = charging},
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
