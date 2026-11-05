#include "clock_store.h"

#include "core/hash.h"
#include "hal/storage.h"
#include "hal/time.h"

const char CLOCK_RECORD_PATH[] = "/.echo/clock.bin";

_Static_assert((int)TIME_ZONE_OPTION_COUNT == (int)WALL_CLOCK_ZONE_COUNT,
               "the time zone setting counts every zone step and unset");

enum {
    RECORD_MAGIC_BYTES = 4,
    RECORD_VERSION_AT = 4,
    RECORD_VERSION = 1,
    RECORD_TIME_AT = 8,
    RECORD_OFFSET_AT = 12,
    RECORD_CHECK_AT = 16,
    RECORD_EARLIEST_YEAR = 2024,
    BITS_PER_BYTE = 8,
    SECONDS_PER_MINUTE = 60,
    SECONDS_PER_DAY = 86400
};

static const uint8_t RECORD_MAGIC[RECORD_MAGIC_BYTES] = {'E', 'C', 'L', 'K'};

static uint32_t get_u32(const uint8_t *bytes)
{
    uint32_t value = 0;
    for (int index = 3; index >= 0; index--) {
        value = value << BITS_PER_BYTE | bytes[index];
    }
    return value;
}

static uint32_t earliest_unix_time(void)
{
    return wall_clock_days_from_civil(RECORD_EARLIEST_YEAR, 1, 1) * SECONDS_PER_DAY;
}

static uint32_t latest_unix_time(void)
{
    return wall_clock_days_from_civil(WALL_CLOCK_LAST_YEAR, 12, 31) * SECONDS_PER_DAY;
}

bool clock_store_record_decode(const uint8_t *bytes, uint32_t *unix_time, int32_t *offset_minutes)
{
    for (int index = 0; index < RECORD_MAGIC_BYTES; index++) {
        if (bytes[index] != RECORD_MAGIC[index]) {
            return false;
        }
    }
    *unix_time = get_u32(bytes + RECORD_TIME_AT);
    *offset_minutes =
        (int16_t)(bytes[RECORD_OFFSET_AT] | bytes[RECORD_OFFSET_AT + 1] << BITS_PER_BYTE);
    return bytes[RECORD_VERSION_AT] == RECORD_VERSION &&
           get_u32(bytes + RECORD_CHECK_AT) == hash_update(HASH_START, bytes, RECORD_CHECK_AT) &&
           *unix_time >= earliest_unix_time() && *unix_time < latest_unix_time();
}

static bool read_record(uint32_t *unix_time, int32_t *offset_minutes)
{
    HalFile file;
    uint8_t bytes[CLOCK_RECORD_BYTES];
    if (!hal_file_open(&file, CLOCK_RECORD_PATH)) {
        return false;
    }
    bool read = file.size >= CLOCK_RECORD_BYTES && hal_file_read(file, 0, bytes, sizeof bytes);
    hal_file_close(&file);
    return read && clock_store_record_decode(bytes, unix_time, offset_minutes);
}

static void read_chip(ClockStore *store, uint32_t now_ms)
{
    HalTime time;
    store->chip_answered = hal_time_read(&time);
    store->chip_valid = store->chip_answered && time.valid && time.year >= WALL_CLOCK_FIRST_YEAR &&
                        time.year <= WALL_CLOCK_LAST_YEAR;
    if (store->chip_valid) {
        WallCivil civil = {time.year, time.month, time.day, time.hour, time.minute, time.second};
        wall_clock_set(&store->clock, wall_clock_seconds_of(&civil), now_ms);
    } else if (store->clock.known) {
        wall_clock_set(&store->clock, wall_clock_local(&store->clock, now_ms), now_ms);
    }
    store->next_read_ms = now_ms + CLOCK_READ_INTERVAL_MS;
}

void clock_store_start(ClockStore *store, const Settings *settings, uint32_t now_ms)
{
    if (!store->started) {
        int32_t offset_minutes;
        store->started = true;
        store->clock.known = false;
        if (!read_record(&store->seen_record, &offset_minutes)) {
            store->seen_record = 0;
        }
    }
    store->clock.zone = settings->options[OPTION_TIME_ZONE];
    read_chip(store, now_ms);
}

void clock_store_step(ClockStore *store, uint32_t now_ms)
{
    if ((int32_t)(now_ms - store->next_read_ms) >= 0) {
        read_chip(store, now_ms);
    }
}

void clock_store_wake(ClockStore *store, uint32_t now_ms)
{
    store->next_read_ms = now_ms;
}

static bool write_chip(ClockStore *store, uint32_t local_seconds)
{
    WallCivil civil;
    wall_clock_civil_of(local_seconds, &civil);
    HalTime time = {.present = true,
                    .valid = true,
                    .year = civil.year,
                    .month = civil.month,
                    .day = civil.day,
                    .hour = civil.hour,
                    .minute = civil.minute,
                    .second = civil.second,
                    .weekday = wall_clock_weekday(local_seconds)};
    bool written = hal_time_write(&time);
    store->chip_answered = written;
    store->chip_valid = written;
    return written;
}

static bool adopt(ClockStore *store, Settings *settings, uint32_t local_seconds, uint8_t zone,
                  uint32_t now_ms)
{
    settings->options[OPTION_TIME_ZONE] = zone;
    store->clock.zone = zone;
    wall_clock_set(&store->clock, local_seconds, now_ms);
    store->next_read_ms = now_ms + CLOCK_READ_INTERVAL_MS;
    return write_chip(store, local_seconds);
}

bool clock_store_after_usb(ClockStore *store, Settings *settings, uint32_t now_ms)
{
    uint32_t unix_time;
    int32_t offset_minutes;
    if (!read_record(&unix_time, &offset_minutes) || unix_time == store->seen_record) {
        return false;
    }
    store->seen_record = unix_time;
    uint8_t zone = wall_clock_zone_of(offset_minutes);
    adopt(store, settings, wall_clock_local_of_unix(unix_time, zone), zone, now_ms);
    return true;
}

bool clock_store_set(ClockStore *store, Settings *settings, const WallCivil *civil, uint8_t zone,
                     uint32_t now_ms)
{
    return adopt(store, settings, wall_clock_seconds_of(civil), zone, now_ms);
}

bool clock_store_now(const ClockStore *store, uint32_t now_ms, WallCivil *civil)
{
    if (!store->clock.known) {
        return false;
    }
    wall_clock_civil_of(wall_clock_local(&store->clock, now_ms), civil);
    return true;
}
