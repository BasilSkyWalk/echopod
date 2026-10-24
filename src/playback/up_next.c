#include "playback/up_next.h"

#include "core/hash.h"

enum {
    VERSION = 1,
    MAGIC_FIELD = 0,
    VERSION_FIELD = 4,
    COUNT_FIELD = 6,
    FINGERPRINT_FIELD = 8,
    TRACKS_FIELD = UP_NEXT_HEADER_SIZE,
    CHECKSUM_FIELD = UP_NEXT_STATE_SIZE - UP_NEXT_CHECKSUM_SIZE,
    MAGIC_SIZE = 4,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'U', 'N'};

static void put_u16(uint8_t *bytes, int offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> BITS_PER_BYTE);
}

static void put_u32(uint8_t *bytes, int offset, uint32_t value)
{
    put_u16(bytes, offset, value & 0xFFFFu);
    put_u16(bytes, offset + 2, value >> 16);
}

static uint16_t get_u16(const uint8_t *bytes, int offset)
{
    return (uint16_t)(bytes[offset] | bytes[offset + 1] << BITS_PER_BYTE);
}

static uint32_t get_u32(const uint8_t *bytes, int offset)
{
    return (uint32_t)get_u16(bytes, offset) | (uint32_t)get_u16(bytes, offset + 2) << 16;
}

static uint32_t checksum_of(const uint8_t *bytes)
{
    return hash_update(HASH_START, bytes, CHECKSUM_FIELD);
}

void up_next_clear(UpNext *up_next)
{
    up_next->count = 0;
}

bool up_next_add(UpNext *up_next, uint16_t track)
{
    if (up_next->count == UP_NEXT_CAPACITY) {
        return false;
    }
    up_next->tracks[up_next->count++] = track;
    return true;
}

bool up_next_play_next(UpNext *up_next, uint16_t track)
{
    if (up_next->count == UP_NEXT_CAPACITY) {
        return false;
    }
    for (uint16_t index = up_next->count; index > 0; index--) {
        up_next->tracks[index] = up_next->tracks[index - 1];
    }
    up_next->tracks[0] = track;
    up_next->count++;
    return true;
}

bool up_next_first(const UpNext *up_next, uint16_t *track)
{
    if (up_next->count == 0) {
        return false;
    }
    *track = up_next->tracks[0];
    return true;
}

void up_next_drop(UpNext *up_next, uint16_t count)
{
    if (count >= up_next->count) {
        up_next->count = 0;
        return;
    }
    for (uint16_t index = count; index < up_next->count; index++) {
        up_next->tracks[index - count] = up_next->tracks[index];
    }
    up_next->count = (uint16_t)(up_next->count - count);
}

void up_next_encode(const UpNext *up_next, uint32_t fingerprint, uint8_t *bytes)
{
    for (int index = 0; index < UP_NEXT_STATE_SIZE; index++) {
        bytes[index] = index < MAGIC_SIZE ? MAGIC[index] : 0;
    }
    put_u16(bytes, VERSION_FIELD, VERSION);
    put_u16(bytes, COUNT_FIELD, up_next->count);
    put_u32(bytes, FINGERPRINT_FIELD, fingerprint);
    for (uint16_t index = 0; index < up_next->count; index++) {
        put_u16(bytes, TRACKS_FIELD + 2 * index, up_next->tracks[index]);
    }
    put_u32(bytes, CHECKSUM_FIELD, checksum_of(bytes));
}

static bool header_valid(const uint8_t *bytes, uint32_t fingerprint)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (bytes[MAGIC_FIELD + index] != MAGIC[index]) {
            return false;
        }
    }
    return get_u16(bytes, VERSION_FIELD) == VERSION &&
           get_u16(bytes, COUNT_FIELD) <= UP_NEXT_CAPACITY &&
           get_u32(bytes, FINGERPRINT_FIELD) == fingerprint &&
           get_u32(bytes, CHECKSUM_FIELD) == checksum_of(bytes);
}

bool up_next_decode(const uint8_t *bytes, uint32_t length, uint32_t fingerprint, UpNext *up_next)
{
    if (length < UP_NEXT_STATE_SIZE || !header_valid(bytes, fingerprint)) {
        return false;
    }
    up_next->count = get_u16(bytes, COUNT_FIELD);
    for (uint16_t index = 0; index < up_next->count; index++) {
        up_next->tracks[index] = get_u16(bytes, TRACKS_FIELD + 2 * index);
    }
    return true;
}
