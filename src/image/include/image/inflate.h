#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "image/image_arena.h"

enum {
    INFLATE_WINDOW = 32768,
    INFLATE_FAST_BITS = 9,
    INFLATE_MAX_BITS = 15,
    INFLATE_LENGTH_CODES = 288,
    INFLATE_DISTANCE_CODES = 32
};

typedef enum InflateResult {
    INFLATE_DONE,
    INFLATE_STOPPED,
    INFLATE_ERROR
} InflateResult;

typedef struct InflateInput {
    bool (*next)(void *context, uint8_t *byte);
    void *context;
} InflateInput;

typedef struct InflateOutput {
    bool (*write)(void *context, const uint8_t *bytes, uint32_t length);
    void *context;
} InflateOutput;

typedef struct InflateCode {
    uint16_t fast[1 << INFLATE_FAST_BITS];
    uint16_t counts[INFLATE_MAX_BITS + 1];
    uint16_t symbols[INFLATE_LENGTH_CODES];
} InflateCode;

typedef struct Inflate {
    InflateInput input;
    InflateOutput output;
    uint32_t bits;
    uint32_t bit_count;
    uint32_t padding_bits;
    uint8_t *window;
    uint32_t position;
    uint32_t flushed;
    bool failed;
    bool stopped;
    InflateCode lengths;
    InflateCode distances;
} Inflate;

bool inflate_init(Inflate *inflate, InflateInput input, InflateOutput output, ImageArena *arena);
InflateResult inflate_run(Inflate *inflate);
