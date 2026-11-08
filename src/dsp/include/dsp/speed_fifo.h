#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    SPEED_CHANNELS = 2,
    SPEED_FIFO_FRAMES = 2640
};

typedef struct SpeedFifo {
    int32_t samples[SPEED_CHANNELS * SPEED_FIFO_FRAMES];
    uint32_t count;
    uint32_t limit;
    uint32_t discard;
    uint32_t real;
    bool finished;
} SpeedFifo;

void speed_fifo_reset(SpeedFifo *fifo, uint32_t limit);
uint32_t speed_fifo_space(const SpeedFifo *fifo);
uint32_t speed_fifo_append(SpeedFifo *fifo, const int32_t *frames, uint32_t count);
void speed_fifo_drop(SpeedFifo *fifo, uint32_t frames);
void speed_fifo_pad(SpeedFifo *fifo, uint32_t frames);
const int32_t *speed_fifo_at(const SpeedFifo *fifo, uint32_t frame);
