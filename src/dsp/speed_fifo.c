#include "dsp/speed_fifo.h"

static uint32_t smaller(uint32_t first, uint32_t second)
{
    return first < second ? first : second;
}

void speed_fifo_reset(SpeedFifo *fifo, uint32_t limit)
{
    fifo->count = 0;
    fifo->limit = limit;
    fifo->discard = 0;
    fifo->real = 0;
    fifo->finished = false;
}

uint32_t speed_fifo_space(const SpeedFifo *fifo)
{
    return fifo->finished ? 0 : fifo->discard + fifo->limit - fifo->count;
}

uint32_t speed_fifo_append(SpeedFifo *fifo, const int32_t *frames, uint32_t count)
{
    uint32_t taken = smaller(count, speed_fifo_space(fifo));
    uint32_t skipped = smaller(taken, fifo->discard);
    uint32_t stored = taken - skipped;
    speed_fifo_copy(fifo->samples + SPEED_CHANNELS * fifo->count, frames + SPEED_CHANNELS * skipped,
                    stored);
    fifo->discard -= skipped;
    fifo->count += stored;
    fifo->real += stored;
    return taken;
}

void speed_fifo_drop(SpeedFifo *fifo, uint32_t frames)
{
    uint32_t removed = smaller(frames, fifo->count);
    fifo->discard += frames - removed;
    fifo->count -= removed;
    fifo->real -= smaller(frames, fifo->real);
    speed_fifo_copy(fifo->samples, fifo->samples + SPEED_CHANNELS * removed, fifo->count);
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

void speed_fifo_copy(int32_t *target, const int32_t *source, uint32_t frames)
{
    for (uint32_t index = 0; index < SPEED_CHANNELS * frames; index++) {
        target[index] = source[index];
    }
}
