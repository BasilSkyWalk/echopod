#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    LOW_BATTERY_EMPTY_MV = 3450,
    LOW_BATTERY_HOLD_MS = 2000
};

typedef struct LowBatteryReading {
    bool known;
    uint32_t millivolts;
    bool usb_power;
} LowBatteryReading;

typedef struct LowBattery {
    bool below;
    uint32_t below_since_ms;
} LowBattery;

void low_battery_init(LowBattery *low);
bool low_battery_update(LowBattery *low, LowBatteryReading reading, uint32_t now_ms);
