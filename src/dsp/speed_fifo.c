#include "dsp/speed_fifo.h"

void speed_fifo_reset(SpeedFifo *fifo, uint32_t limit)
{
    fifo->count = 0;
    fifo->limit = limit;
    fifo->discard = 0;
    fifo->real = 0;
    fifo->finished = false;
}

static uint32_t room(const SpeedFifo *fifo)
{
    return fifo->limit > fifo->count ? fifo->limit - fifo->count : 0;
}

uint32_t speed_fifo_space(const SpeedFifo *fifo)
{
    return fifo->finished ? 0 : fifo->discard + room(fifo);
}

uint32_t speed_fifo_append(SpeedFifo *fifo, const int32_t *frames, uint32_t count)
{
    if (fifo->finished) {
        return 0;
    }
    uint32_t skipped = count < fifo->discard ? count : fifo->discard;
    uint32_t stored = count - skipped < room(fifo) ? count - skipped : room(fifo);
    int32_t *target = fifo->samples + SPEED_CHANNELS * fifo->count;
    for (uint32_t index = 0; index < SPEED_CHANNELS * stored; index++) {
        target[index] = frames[SPEED_CHANNELS * skipped + index];
    }
    fifo->discard -= skipped;
    fifo->count += stored;
    fifo->real += stored;
    return skipped + stored;
}

void speed_fifo_drop(SpeedFifo *fifo, uint32_t frames)
{
    if (frames >= fifo->count) {
        fifo->discard += fifo->finished ? 0 : frames - fifo->count;
        fifo->count = 0;
        fifo->real = 0;
        return;
    }
    for (uint32_t index = 0; index < SPEED_CHANNELS * (fifo->count - frames); index++) {
        fifo->samples[index] = fifo->samples[SPEED_CHANNELS * frames + index];
    }
    fifo->count -= frames;
    fifo->real -= frames < fifo->real ? frames : fifo->real;
}

void speed_fifo_pad(SpeedFifo *fifo, uint32_t frames)
{
    for (uint32_t index = SPEED_CHANNELS * fifo->count; index < SPEED_CHANNELS * frames; index++) {
        fifo->samples[index] = 0;
    }
    fifo->count = frames > fifo->count ? frames : fifo->count;
}

const int32_t *speed_fifo_at(const SpeedFifo *fifo, uint32_t frame)
{
    return fifo->samples + SPEED_CHANNELS * frame;
}
