#include "dsp/equalizer.h"

#include <stddef.h>

enum {
    COEFFICIENT_BITS = 14,
    COEFFICIENT_MAX = (1 << 16) - 1,
    M1_BITS = 13,
    M1_MAX = (1 << 18) - 1,
    RESPONSE_BITS = 16,
    STATE_LIMIT = 12,
    DOWN_ONE_DB = 22,
    PRODUCT_SHIFT = 8,
    TABLE_BITS = 30,
    GAIN_TABLE_OFFSET = 24,
    SINE_LAST_TERM = 12,
    COSINE_LAST_TERM = 13,
    AUDIBLE_NUMERATOR = 9,
    AUDIBLE_DENOMINATOR = 20,
    TENTHS = 10,
    HUNDREDTHS = 100,
    TABLE_LAST = 36,
    PREAMP_FLOOR_TENTHS = -480
};

static const int64_t ONE = INT64_C(1) << TABLE_BITS;
static const int64_t R_ONE = INT64_C(1) << RESPONSE_BITS;
static const int64_t R_CAP = INT64_C(1) << 38;
static const int64_t R_WIDE = INT64_C(1) << 31;
static const int64_t OMEGA_MIN = INT64_C(1) << (RESPONSE_BITS - 6);
static const int64_t OMEGA_MAX = INT64_C(1) << (RESPONSE_BITS + 6);
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

static int clamped_q(int q_hundredths)
{
    return q_hundredths < EQUALIZER_Q_MIN_HUNDREDTHS   ? EQUALIZER_Q_MIN_HUNDREDTHS
           : q_hundredths > EQUALIZER_Q_MAX_HUNDREDTHS ? EQUALIZER_Q_MAX_HUNDREDTHS
                                                       : q_hundredths;
}

static int clamped_preamp(int preamp_tenths)
{
    return preamp_tenths < EQUALIZER_PREAMP_MIN_TENTHS   ? EQUALIZER_PREAMP_MIN_TENTHS
           : preamp_tenths > EQUALIZER_PREAMP_MAX_TENTHS ? EQUALIZER_PREAMP_MAX_TENTHS
                                                         : preamp_tenths;
}

static int64_t amplitude_for_hundredths(int hundredths)
{
    int scaled = hundredths + GAIN_TABLE_OFFSET * HUNDREDTHS;
    scaled = scaled < 0 ? 0 : scaled > TABLE_LAST * HUNDREDTHS ? TABLE_LAST * HUNDREDTHS : scaled;
    int index = scaled / HUNDREDTHS;
    int fraction = scaled % HUNDREDTHS;
    int64_t low = GAIN_Q30[index];
    if (fraction == 0) {
        return low;
    }
    int64_t high = GAIN_Q30[index + 1];
    return low + divided((high - low) * fraction, HUNDREDTHS);
}

static int64_t amplitude_for_gain_tenths(int gain_tenths)
{
    return amplitude_for_hundredths(clamped_gain(gain_tenths) * TENTHS);
}

static int64_t linear_for_preamp_tenths(int preamp_tenths)
{
    int value = preamp_tenths < PREAMP_FLOOR_TENTHS ? PREAMP_FLOOR_TENTHS
                : preamp_tenths > 0                 ? 0
                                                    : preamp_tenths;
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
    return band.gain_tenths != 0 && band.frequency_hz > 0 && band.q_hundredths > 0 &&
           band.filter <= EQUALIZER_HIGH_SHELF &&
           (uint32_t)band.frequency_hz * AUDIBLE_DENOMINATOR < rate * AUDIBLE_NUMERATOR;
}

bool equalizer_curve_valid(const EqualizerCurve *curve, uint32_t rate)
{
    return curve->band_count <= EQUALIZER_MAX_BANDS && rate > 0 && rate <= EQUALIZER_MAX_RATE;
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
    int64_t tangent = divided(half.sine << TABLE_BITS, half.cosine);
    int gain = clamped_gain(band.gain_tenths);
    if (band.filter == EQUALIZER_HIGH_SHELF) {
        gain = -gain;
    }
    int64_t amplitude = amplitude_for_gain_tenths(gain);
    int q_hundredths = clamped_q(band.q_hundredths);
    bool shelf = band.filter != EQUALIZER_PEAK;
    int64_t width = shelf ? divided(ONE * q_hundredths, HUNDREDTHS)
                          : divided(amplitude * q_hundredths, HUNDREDTHS);
    int64_t damping = divided(ONE << TABLE_BITS, width);
    if (shelf) {
        tangent = multiply(tangent, amplitude_for_hundredths(-gain * TENTHS / 2));
    }
    int32_t g = coefficient(tangent);
    int64_t g_wide = widened(g);
    int64_t denominator = ONE + product(g_wide, g_wide + damping);
    int32_t a1 = (int32_t)divided(INT64_C(1) << (TABLE_BITS + COEFFICIENT_BITS), denominator);
    int64_t squared_minus_one = product(amplitude, amplitude) - ONE;
    int64_t effective = effective_damping(a1, g_wide);
    if (!shelf) {
        return (EqualizerStage){
            .g = g, .a1 = a1, .m1 = coefficient_m1(product(effective, squared_minus_one))};
    }
    return (EqualizerStage){.g = g,
                            .a1 = a1,
                            .m1 = coefficient_m1(product(effective, amplitude - ONE)),
                            .m2 = coefficient(squared_minus_one)};
}

static int shelf_lift_tenths(const EqualizerCurve *curve, uint32_t rate)
{
    int lift = 0;
    for (int index = 0; index < curve->band_count; index++) {
        EqualizerBand band = curve->bands[index];
        if (band.filter == EQUALIZER_HIGH_SHELF && equalizer_band_audible(band, rate)) {
            lift += clamped_gain(band.gain_tenths);
        }
    }
    return lift;
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

typedef struct Shape {
    int64_t g;
    int64_t k;
    int64_t m1;
    int64_t m2;
} Shape;

static int64_t times(int64_t first, int64_t second)
{
    int64_t larger = first > second ? first : second;
    int64_t smaller = first > second ? second : first;
    larger = larger > R_CAP ? R_CAP : larger;
    int64_t result =
        larger < R_WIDE ? (larger * smaller) >> RESPONSE_BITS : (larger >> RESPONSE_BITS) * smaller;
    return result > R_CAP ? R_CAP : result;
}

static Shape shape_of(const EqualizerStage *stage)
{
    int64_t g = widened(stage->g);
    return (Shape){.g = g >> (TABLE_BITS - RESPONSE_BITS),
                   .k = effective_damping(stage->a1, g) >> (TABLE_BITS - RESPONSE_BITS),
                   .m1 = (int64_t)stage->m1 * (1 << (RESPONSE_BITS - M1_BITS)),
                   .m2 = (int64_t)stage->m2 * (1 << (RESPONSE_BITS - COEFFICIENT_BITS))};
}

static int64_t response_squared(const Shape *shape, int64_t tangent)
{
    int64_t omega = divided(tangent << RESPONSE_BITS, shape->g);
    if (omega < OMEGA_MIN) {
        return times(R_ONE + shape->m2, R_ONE + shape->m2);
    }
    if (omega > OMEGA_MAX) {
        return R_ONE;
    }
    int64_t w = (omega * omega) >> RESPONSE_BITS;
    int64_t real_top = R_ONE + shape->m2 - w;
    int64_t imaginary_top = ((shape->k + shape->m1) * omega) >> RESPONSE_BITS;
    int64_t real_bottom = R_ONE - w;
    int64_t imaginary_bottom = (shape->k * omega) >> RESPONSE_BITS;
    int64_t top = real_top * real_top + imaginary_top * imaginary_top;
    int64_t bottom =
        (real_bottom * real_bottom + imaginary_bottom * imaginary_bottom) >> RESPONSE_BITS;
    return top / (bottom > 0 ? bottom : 1);
}

static int64_t state_peak_squared(const Shape *shape)
{
    int64_t g_squared = (shape->g * shape->g) >> RESPONSE_BITS;
    int64_t k = shape->k > 0 ? shape->k : 1;
    int64_t peak = R_ONE + shape->g + divided((R_ONE + g_squared) << RESPONSE_BITS, k);
    return times(peak, peak);
}

static int64_t largest(int64_t first, int64_t second)
{
    return first > second ? first : second;
}

static int64_t worst_peak_squared(const EqualizerChain *chain)
{
    Shape shapes[EQUALIZER_MAX_BANDS];
    int64_t worst = R_ONE;
    for (int index = 0; index < chain->stage_count; index++) {
        shapes[index] = shape_of(&chain->stages[index]);
        worst = times(worst, times(R_ONE + shapes[index].m2, R_ONE + shapes[index].m2));
    }
    for (int stage = 0; stage < chain->stage_count; stage++) {
        int64_t gain = R_ONE;
        for (int index = 0; index < chain->stage_count; index++) {
            if (index == stage) {
                worst = largest(worst, times(gain, state_peak_squared(&shapes[stage])));
            }
            gain = times(gain, response_squared(&shapes[index], shapes[stage].g));
        }
        worst = largest(worst, gain);
    }
    return worst;
}

static int32_t within_headroom(const EqualizerChain *chain, int64_t preamp)
{
    int64_t worst = worst_peak_squared(chain);
    int64_t limit = (int64_t)STATE_LIMIT * STATE_LIMIT * R_ONE;
    while (preamp > 0) {
        int64_t level = preamp >> (TABLE_BITS - RESPONSE_BITS);
        if (times(times(level, level), worst) <= limit) {
            break;
        }
        preamp = multiply(preamp, GAIN_Q30[DOWN_ONE_DB]);
    }
    return (int32_t)preamp;
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
    for (int band = 0; band < chain->curve.band_count; band++) {
        if (!equalizer_band_audible(chain->curve.bands[band], rate)) {
            continue;
        }
        EqualizerStage *stage = &chain->stages[chain->stage_count++];
        *stage = design_stage(chain->curve.bands[band], rate);
        stage->band = (uint8_t)band;
    }
    int preamp =
        equalizer_preamp_tenths(&chain->curve, rate) + shelf_lift_tenths(&chain->curve, rate);
    chain->preamp = within_headroom(chain, linear_for_preamp_tenths(preamp));
}

static bool same_layout(const EqualizerCurve *first, const EqualizerCurve *second)
{
    if (first->band_count != second->band_count) {
        return false;
    }
    for (int index = 0; index < first->band_count; index++) {
        if (first->bands[index].frequency_hz != second->bands[index].frequency_hz ||
            first->bands[index].filter != second->bands[index].filter) {
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

void equalizer_reset(Equalizer *equalizer)
{
    EqualizerCurve curve =
        equalizer->has_pending ? equalizer->pending_curve : equalizer->current.curve;
    uint32_t rate = equalizer->rate;
    *equalizer = (Equalizer){0};
    equalizer->rate = rate;
    design_chain(&equalizer->current, &curve, rate, false);
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
    if (!equalizer->has_pending) {
        equalizer->fade_remaining = EQUALIZER_FADE_FRAMES;
    } else if (!equalizer->fading_out) {
        equalizer->fade_remaining = (uint16_t)(EQUALIZER_FADE_FRAMES - equalizer->fade_remaining);
    }
    equalizer->pending_curve = *curve;
    equalizer->has_pending = 1;
    equalizer->fading_out = 1;
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
