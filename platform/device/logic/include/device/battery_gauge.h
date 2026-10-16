#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    BATTERY_GAUGE_READINGS = 8
};

typedef struct BatteryGauge {
    uint32_t millivolts[BATTERY_GAUGE_READINGS];
    uint32_t count;
    uint32_t next;
} BatteryGauge;

uint32_t battery_millivolts(uint16_t raw);
uint8_t battery_percent_for(uint32_t millivolts);
void battery_gauge_init(BatteryGauge *gauge);
void battery_gauge_add(BatteryGauge *gauge, uint16_t raw);
bool battery_gauge_known(const BatteryGauge *gauge);
uint32_t battery_gauge_millivolts(const BatteryGauge *gauge);
uint8_t battery_gauge_percent(const BatteryGauge *gauge, bool charging);
