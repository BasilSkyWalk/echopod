#include "dsp/equalizer.h"

#include <stddef.h>

enum {
    COEFFICIENT_BITS = 14,
    COEFFICIENT_MAX = (1 << 16) - 1,
    M1_BITS = 20,
    M1_MAX = (1 << 27) - 1,
    PRODUCT_SHIFT = 8,
    TABLE_BITS = 30,
    GAIN_TABLE_OFFSET = 24,
    SINE_LAST_TERM = 12,
    COSINE_LAST_TERM = 13,
    AUDIBLE_NUMERATOR = 9,
    AUDIBLE_DENOMINATOR = 20,
    TENTHS = 10
};

static const int64_t ONE = INT64_C(1) << TABLE_BITS;
static const int64_t HALF = INT64_C(1) << (TABLE_BITS - 1);
static const uint64_t PI_Q30 = UINT64_C(3373259426);

static const int32_t GAIN_Q30[] = {
    269711752,  285693178,  302621563,  320553018,  339546978,  359666402,  380977976,  403552340,
    427464319,  452793173,  479622855,  508042296,  538145694,  570032831,  603809400,  639587356,
    677485290,  717628817,  760150998,  805192776,  852903448,  903441154,  956973408,  1013677647,
    1073741824, 1137365027, 1204758142, 1276144550, 1351760868, 1431857735, 1516700640, 1606570803,
    1701766107, 1802602089, 1909412977, 2022552809, 2142396597};

typedef struct Angle {
    int64_t sine;
    int64_t cosine;
} Angle;

static int64_t multiply(int64_t first, int64_t second)
{
    return (first * second + HALF) >> TABLE_BITS;
}

static int64_t divided(int64_t numerator, int64_t denominator)
{
    return (numerator + denominator / 2) / denominator;
}

static int64_t product(int64_t first, int64_t second)
{
    return divided((first >> PRODUCT_SHIFT) * (second >> PRODUCT_SHIFT),
                   INT64_C(1) << (TABLE_BITS - 2 * PRODUCT_SHIFT));
}

static int clamped_gain(int gain_tenths)
{
    return gain_tenths < EQUALIZER_GAIN_MIN_TENTHS   ? EQUALIZER_GAIN_MIN_TENTHS
           : gain_tenths > EQUALIZER_GAIN_MAX_TENTHS ? EQUALIZER_GAIN_MAX_TENTHS
                                                     : gain_tenths;
}

static int clamped_q(int q_tenths)
{
    return q_tenths < EQUALIZER_Q_MIN_TENTHS   ? EQUALIZER_Q_MIN_TENTHS
           : q_tenths > EQUALIZER_Q_MAX_TENTHS ? EQUALIZER_Q_MAX_TENTHS
                                               : q_tenths;
}

static int clamped_preamp(int preamp_tenths)
{
    return preamp_tenths < EQUALIZER_PREAMP_MIN_TENTHS   ? EQUALIZER_PREAMP_MIN_TENTHS
           : preamp_tenths > EQUALIZER_PREAMP_MAX_TENTHS ? EQUALIZER_PREAMP_MAX_TENTHS
                                                         : preamp_tenths;
}

static int64_t amplitude_for_gain_tenths(int gain_tenths)
{
    int scaled = clamped_gain(gain_tenths) + GAIN_TABLE_OFFSET * TENTHS;
    int index = scaled / TENTHS;
    int fraction = scaled % TENTHS;
    int64_t low = GAIN_Q30[index];
    if (fraction == 0) {
        return low;
    }
    int64_t high = GAIN_Q30[index + 1];
    return low + divided((high - low) * fraction, TENTHS);
}

static int64_t linear_for_preamp_tenths(int preamp_tenths)
{
    int value = clamped_preamp(preamp_tenths);
    int64_t lower = multiply(amplitude_for_gain_tenths(EQUALIZER_GAIN_MIN_TENTHS),
                             amplitude_for_gain_tenths(EQUALIZER_GAIN_MIN_TENTHS));
    int64_t factor = ONE;
    while (value < EQUALIZER_GAIN_MIN_TENTHS) {
        factor = multiply(factor, lower);
        value -= EQUALIZER_GAIN_MIN_TENTHS;
    }
    int64_t amplitude = amplitude_for_gain_tenths(value);
    return multiply(factor, multiply(amplitude, amplitude));
}

static int64_t series(int64_t square, int last)
{
    int64_t term = ONE;
    for (int k = last; k > 0; k -= 2) {
        term = ONE - (int32_t)multiply(square, term) / (k * (k + 1));
    }
    return term;
}

static Angle half_angle_of(uint32_t frequency, uint32_t rate)
{
    int64_t half = (int64_t)(frequency * PI_Q30 / rate);
    int64_t square = multiply(half, half);
    return (Angle){.sine = multiply(half, series(square, SINE_LAST_TERM)),
                   .cosine = series(square, COSINE_LAST_TERM)};
}

bool equalizer_band_audible(EqualizerBand band, uint32_t rate)
{
    return band.gain_tenths != 0 && band.frequency_hz > 0 && band.q_tenths > 0 &&
           (uint32_t)band.frequency_hz * AUDIBLE_DENOMINATOR < rate * AUDIBLE_NUMERATOR;
}

bool equalizer_curve_valid(const EqualizerCurve *curve, uint32_t rate)
{
    if (curve->band_count > EQUALIZER_MAX_BANDS || rate == 0 || rate > EQUALIZER_MAX_RATE) {
        return false;
    }
    for (int index = 0; index < curve->band_count; index++) {
        EqualizerBand band = curve->bands[index];
        if (band.gain_tenths != 0 && !equalizer_band_audible(band, rate)) {
            return false;
        }
    }
    return true;
}

int equalizer_preamp_tenths(const EqualizerCurve *curve, uint32_t rate)
{
    int boost = 0;
    for (int index = 0; index < curve->band_count; index++) {
        EqualizerBand band = curve->bands[index];
        int gain = clamped_gain(band.gain_tenths);
        if (equalizer_band_audible(band, rate) && gain > boost) {
            boost = gain;
        }
    }
    int automatic = -boost;
    if (!curve->preamp_set) {
        return automatic;
    }
    int supplied = clamped_preamp(curve->preamp_tenths);
    return supplied < automatic ? supplied : automatic;
}

static int32_t coefficient(int64_t value)
{
    int64_t rounded = divided(value, INT64_C(1) << (TABLE_BITS - COEFFICIENT_BITS));
    return rounded > COEFFICIENT_MAX    ? COEFFICIENT_MAX
           : rounded < -COEFFICIENT_MAX ? -COEFFICIENT_MAX
                                        : (int32_t)rounded;
}

static int32_t coefficient_m1(int64_t value)
{
    int64_t rounded = divided(value, INT64_C(1) << (TABLE_BITS - M1_BITS));
    return rounded > M1_MAX ? M1_MAX : rounded < -M1_MAX ? -M1_MAX : (int32_t)rounded;
}

static int64_t widened(int32_t value)
{
    return (int64_t)value << (TABLE_BITS - COEFFICIENT_BITS);
}

static int64_t effective_damping(int32_t a1, int64_t g)
{
    int64_t inverse = divided(INT64_C(1) << (TABLE_BITS + COEFFICIENT_BITS), a1);
    return divided((inverse - ONE) << (TABLE_BITS - 4), g >> 4) - g;
}

static EqualizerStage design_stage(EqualizerBand band, uint32_t rate)
{
    Angle half = half_angle_of(band.frequency_hz, rate);
    int64_t amplitude = amplitude_for_gain_tenths(band.gain_tenths);
    int q_tenths = clamped_q(band.q_tenths);
    int64_t damping = divided(ONE << TABLE_BITS, divided(amplitude * q_tenths, TENTHS));
    int32_t g = coefficient(divided(half.sine << TABLE_BITS, half.cosine));
    int64_t g_wide = widened(g);
    int64_t denominator = ONE + product(g_wide, g_wide + damping);
    int32_t a1 = (int32_t)divided(INT64_C(1) << (TABLE_BITS + COEFFICIENT_BITS), denominator);
    int64_t squared_minus_one = product(amplitude, amplitude) - ONE;
    return (EqualizerStage){
        .g = g,
        .a1 = a1,
        .m1 = coefficient_m1(product(effective_damping(a1, g_wide), squared_minus_one)),
    };
}

static void clear_histories(EqualizerChain *chain)
{
    for (int band = 0; band < EQUALIZER_MAX_BANDS; band++) {
        for (int index = 0; index < EQUALIZER_HISTORY; index++) {
            chain->left[band][index] = 0;
            chain->right[band][index] = 0;
        }
    }
}

static void design_chain(EqualizerChain *chain, const EqualizerCurve *curve, uint32_t rate,
                         bool keep_history)
{
    chain->curve = *curve;
    if (chain->curve.band_count > EQUALIZER_MAX_BANDS) {
        chain->curve.band_count = EQUALIZER_MAX_BANDS;
    }
    chain->stage_count = 0;
    chain->preamp = (int32_t)ONE;
    chain->valid = 0;
    if (!keep_history) {
        clear_histories(chain);
    }
    if (rate == 0 || rate > EQUALIZER_MAX_RATE) {
        return;
    }
    if (!equalizer_curve_valid(&chain->curve, rate)) {
        return;
    }
    chain->valid = 1;
    chain->preamp = (int32_t)linear_for_preamp_tenths(equalizer_preamp_tenths(&chain->curve, rate));
    for (int band = 0; band < chain->curve.band_count; band++) {
        if (!equalizer_band_audible(chain->curve.bands[band], rate)) {
            continue;
        }
        EqualizerStage *stage = &chain->stages[chain->stage_count++];
        *stage = design_stage(chain->curve.bands[band], rate);
        stage->band = (uint8_t)band;
    }
}

static bool same_layout(const EqualizerCurve *first, const EqualizerCurve *second)
{
    if (first->band_count != second->band_count) {
        return false;
    }
    for (int index = 0; index < first->band_count; index++) {
        if (first->bands[index].frequency_hz != second->bands[index].frequency_hz) {
            return false;
        }
    }
    return true;
}

static bool chain_active(const EqualizerChain *chain)
{
    return chain->stage_count > 0 || chain->preamp != (int32_t)ONE;
}

void equalizer_init(Equalizer *equalizer)
{
    *equalizer = (Equalizer){0};
}

void equalizer_set_curve(Equalizer *equalizer, const EqualizerCurve *curve)
{
    bool keep_history = !equalizer->has_pending && chain_active(&equalizer->current) &&
                        same_layout(&equalizer->current.curve, curve);
    if (keep_history) {
        design_chain(&equalizer->current, curve, equalizer->rate, true);
        return;
    }
    if (!equalizer->has_pending && !chain_active(&equalizer->current)) {
        design_chain(&equalizer->current, curve, equalizer->rate, false);
        return;
    }
    equalizer->pending_curve = *curve;
    equalizer->has_pending = 1;
    equalizer->fading_out = 1;
    equalizer->fade_remaining = EQUALIZER_FADE_FRAMES;
}

void equalizer_apply_pending(Equalizer *equalizer)
{
    design_chain(&equalizer->current, &equalizer->pending_curve, equalizer->rate, false);
    equalizer->fading_out = 0;
    equalizer->fade_remaining = EQUALIZER_FADE_FRAMES;
}

void equalizer_set_rate(Equalizer *equalizer, uint32_t rate)
{
    if (equalizer->rate == rate) {
        return;
    }
    equalizer->rate = rate;
    if (equalizer->has_pending) {
        design_chain(&equalizer->current, &equalizer->pending_curve, rate, false);
        equalizer->has_pending = 0;
        equalizer->fading_out = 0;
        equalizer->fade_remaining = 0;
        return;
    }
    design_chain(&equalizer->current, &equalizer->current.curve, rate, false);
}

bool equalizer_active(const Equalizer *equalizer)
{
    return chain_active(&equalizer->current) || equalizer->has_pending;
}

bool equalizer_valid(const Equalizer *equalizer)
{
    return equalizer->current.valid != 0;
}
