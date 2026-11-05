#include "sim_time.h"

#include <time.h>

#include "hal/time.h"

enum {
    TM_YEAR_BASE = 1900,
    LAST_SECOND = 59
};

static bool clock_absent;
static bool clock_unset;

void sim_time_configure(bool absent, bool unset)
{
    clock_absent = absent;
    clock_unset = unset;
}

bool hal_time_read(HalTime *reading)
{
    *reading = (HalTime){.present = false};
    if (clock_absent) {
        return false;
    }
    time_t now = time(NULL);
    const struct tm *local = localtime(&now);
    if (local == NULL) {
        return false;
    }
    int second = local->tm_sec > LAST_SECOND ? LAST_SECOND : local->tm_sec;
    *reading = (HalTime){
        .present = true,
        .valid = !clock_unset,
        .low_voltage = clock_unset,
        .year = (uint16_t)(TM_YEAR_BASE + local->tm_year),
        .month = (uint8_t)(local->tm_mon + 1),
        .day = (uint8_t)local->tm_mday,
        .hour = (uint8_t)local->tm_hour,
        .minute = (uint8_t)local->tm_min,
        .second = (uint8_t)second,
        .weekday = (uint8_t)local->tm_wday,
    };
    return true;
}
