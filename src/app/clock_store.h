#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/wall_clock.h"
#include "settings/settings.h"

extern const char CLOCK_RECORD_PATH[];

enum {
    CLOCK_RECORD_BYTES = 20,
    CLOCK_READ_INTERVAL_MS = 10 * 60 * 1000
};

typedef struct ClockStore {
    WallClock clock;
    bool started;
    bool chip_answered;
    bool chip_valid;
    uint32_t next_read_ms;
    uint32_t seen_record;
} ClockStore;

void clock_store_start(ClockStore *store, const Settings *settings, uint32_t now_ms);
void clock_store_step(ClockStore *store, uint32_t now_ms);
void clock_store_wake(ClockStore *store, uint32_t now_ms);
bool clock_store_after_usb(ClockStore *store, Settings *settings, uint32_t now_ms);
bool clock_store_set(ClockStore *store, Settings *settings, const WallCivil *civil, uint8_t zone,
                     uint32_t now_ms);
bool clock_store_now(const ClockStore *store, uint32_t now_ms, WallCivil *civil);
bool clock_store_record_decode(const uint8_t *bytes, uint32_t *unix_time, int32_t *offset_minutes);
