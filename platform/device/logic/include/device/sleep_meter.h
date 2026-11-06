#pragma once

#include <stdint.h>

enum {
    SLEEP_METER_WINDOW_US = 2000000
};

typedef struct SleepMeter {
    uint32_t window_start_us;
    uint32_t slept_us;
    uint8_t percent;
} SleepMeter;

void sleep_meter_init(SleepMeter *meter, uint32_t now_us);
void sleep_meter_slept(SleepMeter *meter, uint32_t slept_us);
void sleep_meter_advance(SleepMeter *meter, uint32_t now_us);
uint8_t sleep_meter_percent(const SleepMeter *meter);
