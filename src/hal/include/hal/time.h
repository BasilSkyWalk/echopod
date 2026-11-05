#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct HalTime {
    bool present;
    bool valid;
    bool low_voltage;
    bool stopped;
    bool century;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
} HalTime;

bool hal_time_read(HalTime *time);
bool hal_time_write(const HalTime *time);
