#include "sim_time.h"

#include <time.h>

#include "hal/time.h"

enum {
    TM_YEAR_BASE = 1900,
    LAST_SECOND = 59
};

static bool clock_absent;
static bool clock_unset;
static time_t clock_shift;

void sim_time_configure(bool absent, bool unset)
{
    clock_absent = absent;
    clock_unset = unset;
    clock_shift = 0;
}

void sim_time_start_on(uint16_t year, uint8_t month, uint8_t day)
{
    time_t now = time(NULL);
    const struct tm *local = localtime(&now);
    if (local == NULL) {
        return;
    }
    struct tm wanted = *local;
    wanted.tm_year = year - TM_YEAR_BASE;
    wanted.tm_mon = month - 1;
    wanted.tm_mday = day;
    wanted.tm_isdst = -1;
    time_t shifted = mktime(&wanted);
    if (shifted != (time_t)-1) {
        clock_shift = shifted - now;
    }
}

bool hal_time_read(HalTime *reading)
{
    *reading = (HalTime){.present = false};
    if (clock_absent) {
        return false;
    }
    time_t now = time(NULL) + clock_shift;
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

bool hal_time_write(const HalTime *wanted_time)
{
    if (clock_absent) {
        return false;
    }
    struct tm local = {
        .tm_year = wanted_time->year - TM_YEAR_BASE,
        .tm_mon = wanted_time->month - 1,
        .tm_mday = wanted_time->day,
        .tm_hour = wanted_time->hour,
        .tm_min = wanted_time->minute,
        .tm_sec = wanted_time->second,
        .tm_isdst = -1,
    };
    time_t wanted = mktime(&local);
    if (wanted == (time_t)-1) {
        return false;
    }
    clock_shift = wanted - time(NULL);
    clock_unset = false;
    return true;
}
