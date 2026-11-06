#include "dsp/equalizer.h"

#include <stddef.h>

enum {
    COEFFICIENT_BITS = 14,
    M1_BITS = 13,
    LOW_MASK = (1 << COEFFICIENT_BITS) - 1,
    HALF = 1 << (COEFFICIENT_BITS - 1),
    PREAMP_BITS = 30,
    HEADROOM_BITS = 4,
    SAMPLE_MAX = 8388607,
    SAMPLE_MIN = -8388608,
    STEREO = 2,
    MIX_BITS = 8,
    MIX_ONE = 1 << MIX_BITS
};

static const int32_t UNITY = INT32_C(1) << PREAMP_BITS;

static int32_t scaled_by(int32_t coefficient, int32_t value, int bits)
{
    uint32_t low = (uint32_t)value & ((1U << bits) - 1U);
    int32_t high = value >> bits;
    int32_t low_product = (int32_t)((uint32_t)coefficient * low);
    return (int32_t)((uint32_t)coefficient * (uint32_t)high + (uint32_t)(low_product >> bits));
}

static int32_t scaled(int32_t coefficient, int32_t value)
{
    return scaled_by(coefficient, value, COEFFICIENT_BITS);
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

static inline __attribute__((always_inline)) void filter_run(EqualizerStage stage, int32_t *history,
                                                             Run run, bool shelf)
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
        int32_t y = plus(x, scaled_by(stage.m1, next_band, M1_BITS));
        samples[index] = shelf ? plus(y, scaled(stage.m2, next_low)) : y;
    }
    history[0] = band;
    history[1] = low;
    history[2] = (int32_t)carry_band;
    history[3] = (int32_t)carry_low;
}

static void filter_channel(EqualizerStage stage, int32_t *history, Run run)
{
    if (stage.m2 != 0) {
        filter_run(stage, history, run, true);
    } else {
        filter_run(stage, history, run, false);
    }
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

static void process_channel(EqualizerChain *chain, int32_t (*histories)[EQUALIZER_HISTORY], Run run)
{
    to_internal(run, chain->preamp);
    for (int index = 0; index < chain->stage_count; index++) {
        const EqualizerStage stage = chain->stages[index];
        filter_channel(stage, histories[stage.band], run);
    }
    to_samples(run);
}

static void process_chain(EqualizerChain *chain, Run left, Run right)
{
    process_channel(chain, chain->left, left);
    if (right.samples != NULL) {
        process_channel(chain, chain->right, right);
    }
}

static void ramp_run(Run run, uint32_t elapsed, bool fading_in)
{
    for (uint32_t index = 0; index < run.count; index++) {
        uint32_t position = elapsed + index + 1;
        uint32_t weight =
            fading_in ? position * MIX_ONE / EQUALIZER_FADE_FRAMES
                      : (EQUALIZER_FADE_FRAMES - position) * MIX_ONE / EQUALIZER_FADE_FRAMES;
        weight = weight > MIX_ONE ? MIX_ONE : weight;
        int64_t value = (int64_t)run.samples[index * run.stride] * weight;
        int64_t rounded =
            value >= 0 ? (value + MIX_ONE / 2) >> MIX_BITS : -((-value + MIX_ONE / 2) >> MIX_BITS);
        run.samples[index * run.stride] = (int32_t)rounded;
    }
}

void equalizer_process_block(Equalizer *equalizer, EqualizerBlock block)
{
    if (!equalizer->has_pending) {
        process_chain(&equalizer->current, (Run){block.left, block.count, block.stride},
                      (Run){block.right, block.count, block.stride});
        return;
    }
    uint32_t done = 0;
    while (done < block.count) {
        uint32_t chunk =
            block.count - done < EQUALIZER_FADE_CHUNK ? block.count - done : EQUALIZER_FADE_CHUNK;
        if (equalizer->has_pending) {
            chunk = chunk < equalizer->fade_remaining ? chunk : equalizer->fade_remaining;
        }
        Run left = {block.left + done * block.stride, chunk, block.stride};
        Run right = {NULL, chunk, block.stride};
        if (block.right != NULL) {
            right.samples = block.right + done * block.stride;
        }
        process_chain(&equalizer->current, left, right);
        uint32_t elapsed = EQUALIZER_FADE_FRAMES - equalizer->fade_remaining;
        bool fading_in = equalizer->fading_out == 0;
        ramp_run(left, elapsed, fading_in);
        if (right.samples != NULL) {
            ramp_run(right, elapsed, fading_in);
        }
        equalizer->fade_remaining =
            chunk >= equalizer->fade_remaining ? 0 : (uint16_t)(equalizer->fade_remaining - chunk);
        if (equalizer->fade_remaining == 0) {
            if (equalizer->fading_out) {
                equalizer_apply_pending(equalizer);
            } else {
                equalizer->has_pending = 0;
            }
        }
        done += chunk;
    }
}

void equalizer_process(Equalizer *equalizer, int32_t *frames, uint32_t count)
{
    equalizer_process_block(equalizer, (EqualizerBlock){frames, frames + 1, count, STEREO});
}
