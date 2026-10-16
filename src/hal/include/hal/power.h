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
void hal_power_off(void);
