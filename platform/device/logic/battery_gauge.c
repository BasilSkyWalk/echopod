#include "device/battery_gauge.h"

#include <stddef.h>

enum {
    FULL_SCALE_MILLIVOLTS = 5000,
    ADC_STEPS = 1024,
    PERCENT_PER_POINT = 10,
    CHARGING_OFFSET_MILLIVOLTS = 100
};

static const uint16_t CURVE_MILLIVOLTS[] = {3450, 3580, 3660, 3710, 3750, 3800,
                                            3850, 3920, 4000, 4100, 4200};

uint32_t battery_millivolts(uint16_t raw)
{
    return (uint32_t)raw * FULL_SCALE_MILLIVOLTS / ADC_STEPS;
}

uint8_t battery_percent_for(uint32_t millivolts)
{
    size_t points = sizeof CURVE_MILLIVOLTS / sizeof CURVE_MILLIVOLTS[0];
    if (millivolts <= CURVE_MILLIVOLTS[0]) {
        return 0;
    }
    for (size_t index = 1; index < points; index++) {
        uint32_t low = CURVE_MILLIVOLTS[index - 1];
        uint32_t high = CURVE_MILLIVOLTS[index];
        if (millivolts <= high) {
            uint32_t within = (millivolts - low) * PERCENT_PER_POINT / (high - low);
            return (uint8_t)((index - 1) * PERCENT_PER_POINT + within);
        }
    }
    return (uint8_t)((points - 1) * PERCENT_PER_POINT);
}

void battery_gauge_init(BatteryGauge *gauge)
{
    *gauge = (BatteryGauge){.count = 0, .next = 0};
}

void battery_gauge_add(BatteryGauge *gauge, uint16_t raw)
{
    gauge->millivolts[gauge->next] = battery_millivolts(raw);
    gauge->next = (gauge->next + 1) % BATTERY_GAUGE_READINGS;
    if (gauge->count < BATTERY_GAUGE_READINGS) {
        gauge->count++;
    }
}

bool battery_gauge_known(const BatteryGauge *gauge)
{
    return gauge->count > 0;
}

uint32_t battery_gauge_millivolts(const BatteryGauge *gauge)
{
    if (gauge->count == 0) {
        return 0;
    }
    uint32_t total = 0;
    for (uint32_t index = 0; index < gauge->count; index++) {
        total += gauge->millivolts[index];
    }
    return total / gauge->count;
}

uint8_t battery_gauge_percent(const BatteryGauge *gauge, bool charging)
{
    uint32_t millivolts = battery_gauge_millivolts(gauge);
    if (charging) {
        millivolts =
            millivolts > CHARGING_OFFSET_MILLIVOLTS ? millivolts - CHARGING_OFFSET_MILLIVOLTS : 0;
    }
    return battery_percent_for(millivolts);
}
