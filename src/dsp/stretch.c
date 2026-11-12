#include "dsp/stretch.h"

enum {
    MILLISECONDS_PER_SECOND = 1000,
    QUARTERS_PER_FRAME = 4,
    MID_SHIFT = 9
};

static uint32_t whole_steps(uint32_t rate, uint32_t milliseconds, uint32_t step)
{
    return rate * milliseconds / MILLISECONDS_PER_SECOND / step * step;
}

__attribute__((noinline)) void stretch_configure(Stretch *stretch, SpeedFifo *fifo, uint32_t rate,
                                                 uint32_t quarters)
{
    uint32_t step = (rate + STRETCH_COARSE_RATE - 1) / STRETCH_COARSE_RATE;
    StretchShape *shape = &stretch->shape;
    shape->decimation = step;
    shape->overlap = whole_steps(rate, STRETCH_OVERLAP_MS, step);
    shape->seek = whole_steps(rate, STRETCH_SEEK_MS, step);
    shape->sequence = whole_steps(rate, STRETCH_SEQUENCE_MS, step);
    shape->quarters = quarters;
    stretch->carry = 0;
    stretch->active = false;
    stretch->primed = false;
    fifo->limit = shape->seek + shape->sequence;
}

static void mid(const int32_t *frames, int16_t *target, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        target[index] =
            (int16_t)((frames[SPEED_CHANNELS * index] + frames[SPEED_CHANNELS * index + 1]) >>
                      MID_SHIFT);
    }
}

static void average(const int16_t *source, int16_t *target, uint32_t count, uint32_t step)
{
    for (uint32_t index = 0; index < count; index++) {
        int32_t sum = 0;
        for (uint32_t part = 0; part < step; part++) {
            sum += source[index * step + part];
        }
        target[index] = (int16_t)(sum / (int32_t)step);
    }
}

static uint32_t difference(const int16_t *first, const int16_t *second, uint32_t length)
{
    uint32_t sum = 0;
    for (uint32_t index = 0; index < length; index++) {
        int32_t gap = first[index] - second[index];
        sum += (uint32_t)(gap < 0 ? -gap : gap);
    }
    return sum;
}

static uint32_t closest(const int16_t *reference, const int16_t *window, uint32_t count,
                        uint32_t length)
{
    uint32_t best = 0;
    uint32_t best_sum = UINT32_MAX;
    for (uint32_t offset = 0; offset < count; offset++) {
        uint32_t sum = difference(reference, window + offset, length);
        if (sum < best_sum) {
            best_sum = sum;
            best = offset;
        }
    }
    return best;
}

static uint32_t best_offset(Stretch *stretch, const SpeedFifo *fifo)
{
    const StretchShape *shape = &stretch->shape;
    uint32_t step = shape->decimation;
    uint32_t overlap = shape->overlap;
    int16_t *window = stretch->mids + overlap;
    mid(stretch->tail, stretch->mids, overlap);
    mid(speed_fifo_at(fifo, 0), window, shape->seek + overlap);
    average(stretch->mids, stretch->coarse, (shape->seek + 2 * overlap) / step, step);
    uint32_t coarse = closest(stretch->coarse, stretch->coarse + overlap / step, shape->seek / step,
                              overlap / step) *
                      step;
    uint32_t first = coarse > step ? coarse - step : 0;
    uint32_t end = coarse + step + 1;
    return first + closest(stretch->mids, window + first,
                           (end < shape->seek ? end : shape->seek) - first, overlap);
}

static bool start_sequence(Stretch *stretch, SpeedFifo *fifo)
{
    if (fifo->finished) {
        if (fifo->real == 0) {
            return false;
        }
        speed_fifo_pad(fifo, fifo->limit);
    }
    if (fifo->count < fifo->limit) {
        return false;
    }
    if (!stretch->primed) {
        speed_fifo_copy(stretch->tail, speed_fifo_at(fifo, 0), stretch->shape.overlap);
    }
    stretch->offset = best_offset(stretch, fifo);
    stretch->emitted = 0;
    stretch->active = true;
    return true;
}

static int32_t crossfade(int32_t from, int32_t to, uint32_t weight)
{
    return from + (int32_t)(((int64_t)to - from) * (int64_t)weight / STRETCH_WEIGHT_ONE);
}

static uint32_t emit(Stretch *stretch, const SpeedFifo *fifo, int32_t *frames, uint32_t capacity)
{
    uint32_t overlap = stretch->shape.overlap;
    uint32_t left = stretch->shape.sequence - overlap - stretch->emitted;
    uint32_t made = left < capacity ? left : capacity;
    uint32_t first = SPEED_CHANNELS * stretch->emitted;
    const int32_t *input = speed_fifo_at(fifo, stretch->offset);
    for (uint32_t index = first; index < first + SPEED_CHANNELS * made; index++) {
        uint32_t position = index / SPEED_CHANNELS;
        uint32_t weight = (position + 1) * STRETCH_WEIGHT_ONE / (overlap + 1);
        *frames++ = position < overlap ? crossfade(stretch->tail[index], input[index], weight)
                                       : input[index];
    }
    stretch->emitted += made;
    return made;
}

static void finish_sequence(Stretch *stretch, SpeedFifo *fifo)
{
    const StretchShape *shape = &stretch->shape;
    uint32_t last = shape->sequence - shape->overlap;
    speed_fifo_copy(stretch->tail, speed_fifo_at(fifo, stretch->offset + last), shape->overlap);
    uint32_t advance = stretch->carry + shape->quarters * last;
    stretch->carry = advance % QUARTERS_PER_FRAME;
    speed_fifo_drop(fifo, advance / QUARTERS_PER_FRAME);
    stretch->active = false;
    stretch->primed = true;
}

uint32_t stretch_pull(Stretch *stretch, SpeedFifo *fifo, int32_t *frames, uint32_t capacity)
{
    uint32_t made = 0;
    while (made < capacity && (stretch->active || start_sequence(stretch, fifo))) {
        made += emit(stretch, fifo, frames + SPEED_CHANNELS * made, capacity - made);
        if (stretch->emitted == stretch->shape.sequence - stretch->shape.overlap) {
            finish_sequence(stretch, fifo);
        }
    }
    return made;
}

bool stretch_drained(const Stretch *stretch, const SpeedFifo *fifo)
{
    return fifo->finished && fifo->real == 0 && !stretch->active;
}
