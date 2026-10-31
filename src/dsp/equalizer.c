#include "dsp/equalizer.h"

#include <stddef.h>

enum {
    COEFFICIENT_BITS = 14,
    LOW_MASK = (1 << COEFFICIENT_BITS) - 1,
    HALF = 1 << (COEFFICIENT_BITS - 1),
    PREAMP_BITS = 30,
    HEADROOM_BITS = 4,
    SAMPLE_MAX = 8388607,
    SAMPLE_MIN = -8388608,
    STEREO = 2
};

static const int32_t UNITY = INT32_C(1) << PREAMP_BITS;

static int32_t scaled(int32_t coefficient, int32_t value)
{
    uint32_t low = (uint32_t)value & LOW_MASK;
    int32_t high = value >> COEFFICIENT_BITS;
    int32_t low_product = (int32_t)((uint32_t)coefficient * low);
    return (int32_t)((uint32_t)coefficient * (uint32_t)high +
                     (uint32_t)(low_product >> COEFFICIENT_BITS));
}

static int32_t plus(int32_t first, int32_t second)
{
    return (int32_t)((uint32_t)first + (uint32_t)second);
}

static int32_t minus(int32_t first, int32_t second)
{
    return (int32_t)((uint32_t)first - (uint32_t)second);
}

static int32_t doubled_minus(int32_t value, int32_t previous)
{
    return (int32_t)(2U * (uint32_t)value - (uint32_t)previous);
}

static int32_t scaled_carried(int32_t coefficient, int32_t value, uint32_t *carry)
{
    uint32_t low = (uint32_t)value & LOW_MASK;
    int32_t high = value >> COEFFICIENT_BITS;
    uint32_t low_product = (uint32_t)coefficient * low + *carry;
    *carry = low_product & LOW_MASK;
    return (int32_t)((uint32_t)coefficient * (uint32_t)high +
                     (uint32_t)((int32_t)low_product >> COEFFICIENT_BITS));
}

typedef struct Run {
    int32_t *samples;
    uint32_t count;
    uint32_t stride;
} Run;

static void filter_channel(EqualizerStage stage, int32_t *history, Run run)
{
    int32_t band = history[0];
    int32_t low = history[1];
    uint32_t carry_band = (uint32_t)history[2];
    uint32_t carry_low = (uint32_t)history[3];
    int32_t *samples = run.samples;
    for (uint32_t index = 0; index < run.count * run.stride; index += run.stride) {
        int32_t x = samples[index];
        int32_t drive = scaled_carried(stage.g, minus(x, low), &carry_band);
        int32_t next_band = scaled(stage.a1, plus(band, drive));
        int32_t next_low = plus(low, scaled_carried(stage.g, next_band, &carry_low));
        band = doubled_minus(next_band, band);
        low = doubled_minus(next_low, low);
        samples[index] = plus(x, scaled(stage.m1, next_band));
    }
    history[0] = band;
    history[1] = low;
    history[2] = (int32_t)carry_band;
    history[3] = (int32_t)carry_low;
}

static void to_internal(Run run, int32_t preamp)
{
    for (uint32_t index = 0; index < run.count * run.stride; index += run.stride) {
        int32_t scaled_sample = run.samples[index] * (1 << HEADROOM_BITS);
        run.samples[index] = preamp == UNITY
                                 ? scaled_sample
                                 : (int32_t)(((int64_t)scaled_sample * preamp) >> PREAMP_BITS);
    }
}

static int32_t limited(int32_t sample)
{
    return sample > SAMPLE_MAX ? SAMPLE_MAX : sample < SAMPLE_MIN ? SAMPLE_MIN : sample;
}

static void to_samples(Run run)
{
    for (uint32_t index = 0; index < run.count * run.stride; index += run.stride) {
        run.samples[index] =
            limited(plus(run.samples[index], 1 << (HEADROOM_BITS - 1)) >> HEADROOM_BITS);
    }
}

static void process_channel(Equalizer *equalizer, int32_t (*histories)[EQUALIZER_HISTORY], Run run)
{
    to_internal(run, equalizer->preamp);
    for (int index = 0; index < equalizer->stage_count; index++) {
        const EqualizerStage stage = equalizer->stages[index];
        filter_channel(stage, histories[stage.band], run);
    }
    to_samples(run);
}

bool equalizer_active(const Equalizer *equalizer)
{
    return equalizer->stage_count > 0;
}

void equalizer_process_block(Equalizer *equalizer, EqualizerBlock block)
{
    process_channel(equalizer, equalizer->left, (Run){block.left, block.count, block.stride});
    if (block.right != NULL) {
        process_channel(equalizer, equalizer->right, (Run){block.right, block.count, block.stride});
    }
}

void equalizer_process(Equalizer *equalizer, int32_t *frames, uint32_t count)
{
    equalizer_process_block(equalizer, (EqualizerBlock){frames, frames + 1, count, STEREO});
}
