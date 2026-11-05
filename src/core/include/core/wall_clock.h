#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WALL_CLOCK_FIRST_YEAR = 2000,
    WALL_CLOCK_LAST_YEAR = 2099,
    WALL_CLOCK_ZONE_UNSET = 0,
    WALL_CLOCK_ZONE_STEP_MINUTES = 15,
    WALL_CLOCK_ZONE_FIRST_MINUTES = -12 * 60,
    WALL_CLOCK_ZONE_LAST_MINUTES = 14 * 60,
    WALL_CLOCK_ZONE_COUNT = (WALL_CLOCK_ZONE_LAST_MINUTES - WALL_CLOCK_ZONE_FIRST_MINUTES) /
            WALL_CLOCK_ZONE_STEP_MINUTES +
        2,
    WALL_CLOCK_ZONE_UTC = -WALL_CLOCK_ZONE_FIRST_MINUTES / WALL_CLOCK_ZONE_STEP_MINUTES + 1,
    WALL_CLOCK_TIME_CAPACITY = 6,
    WALL_CLOCK_DATE_TIME_CAPACITY = 17,
    WALL_CLOCK_ZONE_CAPACITY = 10
};

typedef struct WallCivil {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} WallCivil;

typedef struct WallClock {
    bool known;
    uint32_t anchor_seconds;
    uint32_t anchor_ms;
    uint8_t zone;
} WallClock;

uint32_t wall_clock_days_from_civil(uint32_t year, uint32_t month, uint32_t day);
uint32_t wall_clock_seconds_of(const WallCivil *civil);
void wall_clock_civil_of(uint32_t seconds, WallCivil *civil);
uint8_t wall_clock_weekday(uint32_t seconds);
bool wall_clock_civil_valid(const WallCivil *civil);
int32_t wall_clock_zone_minutes(uint8_t zone);
uint8_t wall_clock_zone_of(int32_t offset_minutes);
void wall_clock_set(WallClock *clock, uint32_t local_seconds, uint32_t now_ms);
uint32_t wall_clock_local(const WallClock *clock, uint32_t now_ms);
bool wall_clock_unix(const WallClock *clock, uint32_t now_ms, uint32_t *unix_time);
uint32_t wall_clock_local_of_unix(uint32_t unix_time, uint8_t zone);
void wall_clock_time_text(const WallCivil *civil, char *text, size_t capacity);
void wall_clock_date_time_text(const WallCivil *civil, char *text, size_t capacity);
void wall_clock_zone_text(uint8_t zone, char *text, size_t capacity);
