#include "dsp/stretch.h"

enum {
    MILLISECONDS_PER_SECOND = 1000,
    QUARTERS_PER_FRAME = 4,
    MID_SHIFT = 9
};

typedef struct StretchRange {
    uint32_t first;
    uint32_t end;
    uint32_t length;
} StretchRange;

void stretch_configure(Stretch *stretch, SpeedFifo *fifo, uint32_t rate, uint32_t quarters)
{
    uint32_t step = (rate + STRETCH_COARSE_RATE - 1) / STRETCH_COARSE_RATE;
    step = step > 0 ? step : 1;
    StretchShape *shape = &stretch->shape;
    shape->decimation = step;
    shape->overlap = rate * STRETCH_OVERLAP_MS / MILLISECONDS_PER_SECOND / step * step;
    shape->seek = rate * STRETCH_SEEK_MS / MILLISECONDS_PER_SECOND / step * step;
    shape->sequence = rate * STRETCH_SEQUENCE_MS / MILLISECONDS_PER_SECOND / step * step;
    shape->quarters = quarters;
    stretch->offset = 0;
    stretch->emitted = 0;
    stretch->carry = 0;
    stretch->active = false;
    stretch->primed = false;
    speed_fifo_reset(fifo, shape->seek + shape->sequence);
}

static void copy_frames(int32_t *target, const int32_t *source, uint32_t frames)
{
    for (uint32_t index = 0; index < SPEED_CHANNELS * frames; index++) {
        target[index] = source[index];
    }
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

static uint32_t closest(const int16_t *reference, const int16_t *window, StretchRange range)
{
    uint32_t best = range.first;
    uint32_t best_sum = UINT32_MAX;
    for (uint32_t offset = range.first; offset < range.end; offset++) {
        uint32_t sum = difference(reference, window + offset, range.length);
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
    mid(stretch->tail, stretch->reference, shape->overlap);
    mid(speed_fifo_at(fifo, 0), stretch->window, shape->seek + shape->overlap);
    average(stretch->reference, stretch->coarse_reference, shape->overlap / step, step);
    average(stretch->window, stretch->coarse_window, (shape->seek + shape->overlap) / step, step);
    uint32_t coarse = closest(stretch->coarse_reference, stretch->coarse_window,
                              (StretchRange){0, shape->seek / step, shape->overlap / step}) *
                      step;
    uint32_t end = coarse + step + 1;
    return closest(stretch->reference, stretch->window,
                   (StretchRange){coarse > step ? coarse - step : 0,
                                  end < shape->seek ? end : shape->seek, shape->overlap});
}

static bool start_sequence(Stretch *stretch, SpeedFifo *fifo)
{
    uint32_t need = stretch->shape.seek + stretch->shape.sequence;
    if (fifo->finished) {
        if (fifo->real == 0) {
            return false;
        }
        speed_fifo_pad(fifo, need);
    }
    if (fifo->count < need) {
        return false;
    }
    if (stretch->primed) {
        stretch->offset = best_offset(stretch, fifo);
    } else {
        stretch->offset = 0;
        copy_frames(stretch->tail, speed_fifo_at(fifo, 0), stretch->shape.overlap);
    }
    stretch->emitted = 0;
    stretch->active = true;
    return true;
}

static int32_t crossfade(int32_t from, int32_t to, uint32_t weight)
{
    return from + (int32_t)(((int64_t)to - from) * (int64_t)weight / STRETCH_WEIGHT_ONE);
}

static void blend_frame(const Stretch *stretch, const int32_t *input, uint32_t position,
                        int32_t *frame)
{
    uint32_t overlap = stretch->shape.overlap;
    uint32_t weight = (position + 1) * STRETCH_WEIGHT_ONE / (overlap + 1);
    for (uint32_t channel = 0; channel < SPEED_CHANNELS; channel++) {
        frame[channel] = position < overlap
                             ? crossfade(stretch->tail[SPEED_CHANNELS * position + channel],
                                         input[channel], weight)
                             : input[channel];
    }
}

static uint32_t emit(Stretch *stretch, const SpeedFifo *fifo, int32_t *frames, uint32_t capacity)
{
    uint32_t last = stretch->shape.sequence - stretch->shape.overlap;
    uint32_t stop = last - stretch->emitted < capacity ? last : stretch->emitted + capacity;
    uint32_t made = 0;
    for (uint32_t position = stretch->emitted; position < stop; position++) {
        blend_frame(stretch, speed_fifo_at(fifo, stretch->offset + position), position,
                    frames + SPEED_CHANNELS * made);
        made++;
    }
    stretch->emitted = stop;
    return made;
}

static void finish_sequence(Stretch *stretch, SpeedFifo *fifo)
{
    const StretchShape *shape = &stretch->shape;
    uint32_t last = shape->sequence - shape->overlap;
    copy_frames(stretch->tail, speed_fifo_at(fifo, stretch->offset + last), shape->overlap);
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
