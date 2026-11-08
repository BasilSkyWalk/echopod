#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dsp/speed_fifo.h"

enum {
    RESAMPLE_TAPS = 16,
    RESAMPLE_PHASES = 4,
    RESAMPLE_HISTORY = 7,
    RESAMPLE_AHEAD = 8,
    RESAMPLE_LIMIT = 128
};

typedef struct Resample {
    const int16_t (*taps)[RESAMPLE_TAPS];
    uint32_t quarters;
    uint32_t position;
} Resample;

void resample_configure(Resample *resample, SpeedFifo *fifo, uint32_t quarters);
uint32_t resample_pull(Resample *resample, SpeedFifo *fifo, int32_t *frames, uint32_t capacity);
bool resample_drained(const Resample *resample, const SpeedFifo *fifo);
