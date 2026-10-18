#include "core/low_battery.h"

void low_battery_init(LowBattery *low)
{
    *low = (LowBattery){.below = false};
}

static bool is_empty(LowBatteryReading reading)
{
    return reading.known && !reading.usb_power && reading.millivolts < LOW_BATTERY_EMPTY_MV;
}

bool low_battery_update(LowBattery *low, LowBatteryReading reading, uint32_t now_ms)
{
    if (!is_empty(reading)) {
        low->below = false;
        return false;
    }
    if (!low->below) {
        low->below = true;
        low->below_since_ms = now_ms;
    }
    return now_ms - low->below_since_ms >= LOW_BATTERY_HOLD_MS;
}
