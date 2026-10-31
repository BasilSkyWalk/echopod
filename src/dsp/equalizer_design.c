#include "dsp/equalizer.h"

enum {
    COEFFICIENT_BITS = 14,
    COEFFICIENT_MAX = (1 << 16) - 1,
    Q_TENTHS_MIN = 5,
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

static int clamped_gain(int gain)
{
    return gain < EQUALIZER_GAIN_MIN_DB   ? EQUALIZER_GAIN_MIN_DB
           : gain > EQUALIZER_GAIN_MAX_DB ? EQUALIZER_GAIN_MAX_DB
                                          : gain;
}

static int64_t power_of_ten(int fortieths)
{
    return GAIN_Q30[fortieths + GAIN_TABLE_OFFSET];
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
    return band.gain_db != 0 && band.frequency_hz > 0 && band.q_tenths > 0 &&
           (uint32_t)band.frequency_hz * AUDIBLE_DENOMINATOR < rate * AUDIBLE_NUMERATOR;
}

int equalizer_preamp_db(const EqualizerCurve *curve, uint32_t rate)
{
    int boost = 0;
    for (int index = 0; index < curve->band_count; index++) {
        EqualizerBand band = curve->bands[index];
        int gain = clamped_gain(band.gain_db);
        if (equalizer_band_audible(band, rate) && gain > boost) {
            boost = gain;
        }
    }
    return -boost;
}

static int32_t coefficient(int64_t value)
{
    int64_t rounded = divided(value, INT64_C(1) << (TABLE_BITS - COEFFICIENT_BITS));
    return rounded > COEFFICIENT_MAX    ? COEFFICIENT_MAX
           : rounded < -COEFFICIENT_MAX ? -COEFFICIENT_MAX
                                        : (int32_t)rounded;
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
    int64_t amplitude = power_of_ten(clamped_gain(band.gain_db));
    int q_tenths = band.q_tenths < Q_TENTHS_MIN ? Q_TENTHS_MIN : band.q_tenths;
    int64_t damping = divided(ONE << TABLE_BITS, divided(amplitude * q_tenths, TENTHS));
    int32_t g = coefficient(divided(half.sine << TABLE_BITS, half.cosine));
    int64_t g_wide = widened(g);
    int64_t denominator = ONE + product(g_wide, g_wide + damping);
    int32_t a1 = (int32_t)divided(INT64_C(1) << (TABLE_BITS + COEFFICIENT_BITS), denominator);
    int64_t squared_minus_one = product(amplitude, amplitude) - ONE;
    return (EqualizerStage){
        .g = g,
        .a1 = a1,
        .m1 = coefficient(product(effective_damping(a1, g_wide), squared_minus_one)),
    };
}

static uint32_t active_bands(const Equalizer *equalizer)
{
    uint32_t mask = 0;
    for (int index = 0; index < equalizer->stage_count; index++) {
        mask |= 1U << equalizer->stages[index].band;
    }
    return mask;
}

static void clear_history(Equalizer *equalizer, int band)
{
    for (int index = 0; index < EQUALIZER_HISTORY; index++) {
        equalizer->left[band][index] = 0;
        equalizer->right[band][index] = 0;
    }
}

static void design(Equalizer *equalizer, bool keep_history)
{
    uint32_t previous = keep_history ? active_bands(equalizer) : 0;
    const EqualizerCurve *curve = &equalizer->curve;
    uint32_t rate = equalizer->rate;
    equalizer->stage_count = 0;
    if (rate == 0 || rate > EQUALIZER_MAX_RATE) {
        return;
    }
    equalizer->preamp = (int32_t)power_of_ten(2 * equalizer_preamp_db(curve, rate));
    for (int band = 0; band < curve->band_count; band++) {
        if (!equalizer_band_audible(curve->bands[band], rate)) {
            continue;
        }
        if ((previous & (1U << band)) == 0) {
            clear_history(equalizer, band);
        }
        EqualizerStage *stage = &equalizer->stages[equalizer->stage_count++];
        *stage = design_stage(curve->bands[band], rate);
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

void equalizer_init(Equalizer *equalizer)
{
    *equalizer = (Equalizer){0};
}

void equalizer_set_curve(Equalizer *equalizer, const EqualizerCurve *curve)
{
    bool keep_history = same_layout(&equalizer->curve, curve);
    equalizer->curve = *curve;
    if (equalizer->curve.band_count > EQUALIZER_MAX_BANDS) {
        equalizer->curve.band_count = EQUALIZER_MAX_BANDS;
    }
    design(equalizer, keep_history);
}

void equalizer_set_rate(Equalizer *equalizer, uint32_t rate)
{
    if (equalizer->rate == rate) {
        return;
    }
    equalizer->rate = rate;
    design(equalizer, false);
}
