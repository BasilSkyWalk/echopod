#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dsp/speed_fifo.h"

enum {
    STRETCH_SEQUENCE_MS = 40,
    STRETCH_OVERLAP_MS = 10,
    STRETCH_SEEK_MS = 15,
    STRETCH_COARSE_RATE = 6000,
    STRETCH_MAX_SEQUENCE = 1920,
    STRETCH_MAX_OVERLAP = 480,
    STRETCH_MAX_SEEK = 720,
    STRETCH_MAX_COARSE_WINDOW = 150,
    STRETCH_MAX_COARSE_OVERLAP = 60,
    STRETCH_WEIGHT_ONE = 65536
};

_Static_assert(SPEED_FIFO_FRAMES >= STRETCH_MAX_SEEK + STRETCH_MAX_SEQUENCE,
               "the fifo must hold a seek window and a sequence");

typedef struct StretchShape {
    uint32_t overlap;
    uint32_t seek;
    uint32_t sequence;
    uint32_t decimation;
    uint32_t quarters;
} StretchShape;

typedef struct Stretch {
    StretchShape shape;
    uint32_t offset;
    uint32_t emitted;
    uint32_t carry;
    bool active;
    bool primed;
    int16_t coarse[STRETCH_MAX_COARSE_OVERLAP + STRETCH_MAX_COARSE_WINDOW];
    int16_t mids[STRETCH_MAX_OVERLAP + STRETCH_MAX_SEEK + STRETCH_MAX_OVERLAP];
    int32_t tail[SPEED_CHANNELS * STRETCH_MAX_OVERLAP];
} Stretch;

void stretch_configure(Stretch *stretch, SpeedFifo *fifo, uint32_t rate, uint32_t quarters);
uint32_t stretch_pull(Stretch *stretch, SpeedFifo *fifo, int32_t *frames, uint32_t capacity);
bool stretch_drained(const Stretch *stretch, const SpeedFifo *fifo);
