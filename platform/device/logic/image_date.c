#include "device/image_date.h"

#include "core/wall_clock.h"

enum {
    BCD_DIGITS = 4,
    BCD_BITS = 4,
    BCD_DIGIT_MASK = 0xF,
    DECIMAL_BASE = 10,
    DATE_DAY_SHIFT = 16,
    DATE_MONTH_SHIFT = 24,
    DATE_BYTE_MASK = 0xFF,
    DATE_YEAR_MASK = 0xFFFF
};

static uint32_t from_bcd(uint32_t bcd)
{
    uint32_t value = 0;
    for (int digit = BCD_DIGITS - 1; digit >= 0; digit--) {
        uint32_t nibble = bcd >> (digit * BCD_BITS) & BCD_DIGIT_MASK;
        if (nibble >= DECIMAL_BASE) {
            return 0;
        }
        value = value * DECIMAL_BASE + nibble;
    }
    return value;
}

uint32_t image_date_day(uint32_t bcd_date)
{
    WallCivil civil = {
        .year = (uint16_t)from_bcd(bcd_date & DATE_YEAR_MASK),
        .month = (uint8_t)from_bcd(bcd_date >> DATE_MONTH_SHIFT),
        .day = (uint8_t)from_bcd(bcd_date >> DATE_DAY_SHIFT & DATE_BYTE_MASK),
    };
    if (!wall_clock_civil_valid(&civil)) {
        return 0;
    }
    return wall_clock_days_from_civil(civil.year, civil.month, civil.day);
}
