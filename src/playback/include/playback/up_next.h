#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    UP_NEXT_CAPACITY = 100,
    UP_NEXT_HEADER_SIZE = 12,
    UP_NEXT_CHECKSUM_SIZE = 4,
    UP_NEXT_STATE_SIZE = UP_NEXT_HEADER_SIZE + 2 * UP_NEXT_CAPACITY + UP_NEXT_CHECKSUM_SIZE
};

typedef struct UpNext {
    uint16_t tracks[UP_NEXT_CAPACITY];
    uint16_t count;
} UpNext;

void up_next_clear(UpNext *up_next);
bool up_next_add(UpNext *up_next, uint16_t track);
bool up_next_play_next(UpNext *up_next, uint16_t track);
bool up_next_first(const UpNext *up_next, uint16_t *track);
void up_next_drop(UpNext *up_next, uint16_t count);
void up_next_encode(const UpNext *up_next, uint32_t fingerprint, uint8_t *bytes);
bool up_next_decode(const uint8_t *bytes, uint32_t length, uint32_t fingerprint, UpNext *up_next);
