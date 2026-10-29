#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct HalBattery {
    bool known;
    uint8_t percent;
    bool charging;
    bool usb_power;
    uint16_t millivolts;
} HalBattery;

HalBattery hal_power_battery(void);

typedef struct HalPowerHold {
    uint8_t function;
    bool output;
    bool high;
    bool usb_power;
    bool key_down;
} HalPowerHold;

HalPowerHold hal_power_boot_hold(void);
bool hal_power_take_usb_start(void);
void hal_power_off(void);
