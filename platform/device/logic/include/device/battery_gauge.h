#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    BATTERY_GAUGE_READINGS = 64,
    BATTERY_DISPLAY_STEP = 2,
    BATTERY_DISPLAY_REVERSE_STEP = 10,
    BATTERY_BOOT_CUTOFF_MILLIVOLTS = 3400,
    BATTERY_CHARGING_MAX_PERCENT = 99,
    BATTERY_RECENT_READINGS = 8
};

typedef struct BatteryGauge {
    uint32_t millivolts[BATTERY_GAUGE_READINGS];
    uint32_t count;
    uint32_t next;
} BatteryGauge;

uint32_t battery_millivolts(uint16_t raw);
uint8_t battery_percent_for(uint32_t millivolts);
bool battery_boot_cutoff(uint32_t millivolts, bool usb_power);
void battery_gauge_init(BatteryGauge *gauge);
void battery_gauge_add(BatteryGauge *gauge, uint16_t raw);
void battery_gauge_restart(BatteryGauge *gauge);
bool battery_gauge_known(const BatteryGauge *gauge);
uint32_t battery_gauge_millivolts(const BatteryGauge *gauge);
uint32_t battery_gauge_recent_millivolts(const BatteryGauge *gauge);
uint8_t battery_gauge_percent(const BatteryGauge *gauge, bool charging);

typedef struct BatteryDisplay {
    bool shown_known;
    uint8_t shown;
} BatteryDisplay;

void battery_display_init(BatteryDisplay *display);
uint8_t battery_display_update(BatteryDisplay *display, uint8_t percent, bool usb_power);
