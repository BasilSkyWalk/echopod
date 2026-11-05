#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/time.h"

enum {
    PCF8563_WRITE_ADDRESS = 0xA2,
    PCF8563_FIRST_REGISTER = 0x00,
    PCF8563_REGISTER_COUNT = 9,
    PCF8563_CONTROL_REGISTER = 0x00,
    PCF8563_TIME_REGISTER = 0x02,
    PCF8563_TIME_REGISTER_COUNT = 7
};

uint8_t bcd_decode(uint8_t value);
uint8_t bcd_encode(uint8_t value);
bool civil_date_valid(uint16_t year, uint8_t month, uint8_t day);
void pcf8563_decode(const uint8_t *registers, HalTime *time);
void pcf8563_encode(const HalTime *time, uint8_t *registers);
