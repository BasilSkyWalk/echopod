#include "device/pcf8563.h"

enum {
    REGISTER_CONTROL1 = 0x00,
    REGISTER_SECONDS = 0x02,
    REGISTER_MINUTES = 0x03,
    REGISTER_HOURS = 0x04,
    REGISTER_DAYS = 0x05,
    REGISTER_WEEKDAYS = 0x06,
    REGISTER_MONTHS = 0x07,
    REGISTER_YEARS = 0x08,
    CONTROL1_STOP = 0x20,
    SECONDS_LOW_VOLTAGE = 0x80,
    MONTHS_CENTURY = 0x80,
    SECONDS_MASK = 0x7F,
    HOURS_MASK = 0x3F,
    DAYS_MASK = 0x3F,
    WEEKDAYS_MASK = 0x07,
    MONTHS_MASK = 0x1F,
    YEARS_MASK = 0xFF,
    NIBBLE_MASK = 0x0F,
    NIBBLE_BITS = 4,
    LAST_DIGIT = 9,
    CENTURY_BASE = 2000,
    LAST_SECOND = 59,
    LAST_HOUR = 23,
    LAST_WEEKDAY = 6,
    MONTHS_PER_YEAR = 12,
    FEBRUARY = 2,
    SHORT_MONTH_DAYS = 30,
    FEBRUARY_DAYS = 28,
    LONG_MONTHS = 0x15AA
};

uint8_t bcd_decode(uint8_t value)
{
    return (uint8_t)((value >> NIBBLE_BITS) * 10 + (value & NIBBLE_MASK));
}

uint8_t bcd_encode(uint8_t value)
{
    return (uint8_t)(value / 10 << NIBBLE_BITS | value % 10);
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    if (month == FEBRUARY) {
        bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        return (uint8_t)(FEBRUARY_DAYS + (leap ? 1 : 0));
    }
    return (uint8_t)(SHORT_MONTH_DAYS + ((LONG_MONTHS >> month) & 1));
}

bool civil_date_valid(uint16_t year, uint8_t month, uint8_t day)
{
    return month >= 1 && month <= MONTHS_PER_YEAR && day >= 1 && day <= days_in_month(year, month);
}

static uint8_t take(const uint8_t *registers, uint32_t index, uint8_t mask, bool *digits)
{
    uint8_t value = registers[index] & mask;
    if ((value >> NIBBLE_BITS) > LAST_DIGIT || (value & NIBBLE_MASK) > LAST_DIGIT) {
        *digits = false;
    }
    return bcd_decode(value);
}

void pcf8563_decode(const uint8_t *registers, HalTime *time)
{
    bool digits = true;
    *time = (HalTime){
        .present = true,
        .low_voltage = (registers[REGISTER_SECONDS] & SECONDS_LOW_VOLTAGE) != 0,
        .stopped = (registers[REGISTER_CONTROL1] & CONTROL1_STOP) != 0,
        .century = (registers[REGISTER_MONTHS] & MONTHS_CENTURY) != 0,
        .weekday = registers[REGISTER_WEEKDAYS] & WEEKDAYS_MASK,
    };
    time->second = take(registers, REGISTER_SECONDS, SECONDS_MASK, &digits);
    time->minute = take(registers, REGISTER_MINUTES, SECONDS_MASK, &digits);
    time->hour = take(registers, REGISTER_HOURS, HOURS_MASK, &digits);
    time->day = take(registers, REGISTER_DAYS, DAYS_MASK, &digits);
    time->month = take(registers, REGISTER_MONTHS, MONTHS_MASK, &digits);
    time->year = (uint16_t)(CENTURY_BASE + take(registers, REGISTER_YEARS, YEARS_MASK, &digits));
    time->valid = digits && !time->low_voltage && !time->stopped && time->second <= LAST_SECOND &&
                  time->minute <= LAST_SECOND && time->hour <= LAST_HOUR &&
                  time->weekday <= LAST_WEEKDAY &&
                  civil_date_valid(time->year, time->month, time->day);
}
