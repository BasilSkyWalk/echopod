#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "check.h"
#include "dsp/equalizer.h"

enum {
    SETTLE_FRAMES = 9600,
    MEASURE_FRAMES = 9600,
    FULL_SCALE = 8388607
};

static const double PI = 3.14159265358979323846;
static const double HALF_SCALE = 4194304.0;
static const double TOP = 8388607.0;

static EqualizerCurve one_band(uint16_t frequency, int gain, uint8_t q_tenths)
{
    EqualizerCurve curve = {.band_count = 1};
    curve.bands[0] = (EqualizerBand){.frequency_hz = frequency,
                                     .gain_tenths = (int16_t)(gain * 10),
                                     .q_hundredths = (uint16_t)(q_tenths * 10),
                                     .filter = EQUALIZER_PEAK};
    return curve;
}

static void process_pair(Equalizer *equalizer, int32_t *left, int32_t *right)
{
    int32_t frame[2] = {*left, *right};
    equalizer_process(equalizer, frame, 1);
    *left = frame[0];
    *right = frame[1];
}

static Equalizer equalizer_for(EqualizerCurve curve, uint32_t rate)
{
    Equalizer equalizer;
    equalizer_init(&equalizer);
    equalizer_set_rate(&equalizer, rate);
    equalizer_set_curve(&equalizer, &curve);
    return equalizer;
}

static double gain_at(Equalizer *equalizer, double frequency, uint32_t rate)
{
    double peak = 0.0;
    for (int frame = 0; frame < SETTLE_FRAMES + MEASURE_FRAMES; frame++) {
        int32_t sample = (int32_t)(HALF_SCALE * sin(2.0 * PI * frequency * frame / rate));
        int32_t left = sample;
        int32_t right = sample;
        process_pair(equalizer, &left, &right);
        if (frame >= SETTLE_FRAMES && fabs((double)left) > peak) {
            peak = fabs((double)left);
        }
    }
    return 20.0 * log10(peak / HALF_SCALE);
}

static bool near(double expected, double actual, double tolerance)
{
    return fabs(expected - actual) <= tolerance;
}

static void a_flat_curve_leaves_the_equalizer_off(void)
{
    Equalizer equalizer = equalizer_for(one_band(1000, 0, 14), 44100);
    CHECK(!equalizer_active(&equalizer));
}

static void a_cut_lowers_its_band_and_leaves_far_tones_alone(void)
{
    Equalizer equalizer = equalizer_for(one_band(1000, -6, 14), 48000);
    CHECK(equalizer_active(&equalizer));
    CHECK(near(-6.0, gain_at(&equalizer, 1000.0, 48000), 0.2));
    CHECK(near(0.0, gain_at(&equalizer, 100.0, 48000), 0.3));
    CHECK(near(0.0, gain_at(&equalizer, 15000.0, 48000), 0.3));
}

static void a_boost_is_paid_for_with_headroom(void)
{
    EqualizerCurve curve = one_band(1000, 6, 14);
    Equalizer equalizer = equalizer_for(curve, 44100);
    CHECK_EQUAL(-60, equalizer_preamp_tenths(&curve, 44100));
    CHECK(near(0.0, gain_at(&equalizer, 1000.0, 44100), 0.2));
    CHECK(near(-6.0, gain_at(&equalizer, 12000.0, 44100), 0.3));
}

static void a_low_band_stays_accurate_at_ninety_six_kilohertz(void)
{
    Equalizer equalizer = equalizer_for(one_band(63, -9, 10), 96000);
    CHECK(near(-9.0, gain_at(&equalizer, 63.0, 96000), 0.3));
    CHECK(near(0.0, gain_at(&equalizer, 5000.0, 96000), 0.3));
}

static void tracks_above_ninety_six_kilohertz_play_without_it(void)
{
    Equalizer equalizer = equalizer_for(one_band(1000, -6, 14), 192000);
    CHECK(!equalizer_active(&equalizer));
    equalizer_set_rate(&equalizer, 44100);
    CHECK(equalizer_active(&equalizer));
}

static void bands_near_the_top_of_a_low_rate_are_skipped(void)
{
    EqualizerCurve curve = one_band(16000, 6, 10);
    Equalizer equalizer = equalizer_for(curve, 32000);
    CHECK(!equalizer_active(&equalizer));
    CHECK_EQUAL(0, equalizer_preamp_tenths(&curve, 32000));
}

static void the_largest_audible_boost_sets_the_preamp(void)
{
    EqualizerCurve curve = {.band_count = 3};
    curve.bands[0] = (EqualizerBand){100, 40, 140, EQUALIZER_PEAK};
    curve.bands[1] = (EqualizerBand){1000, -60, 140, EQUALIZER_PEAK};
    curve.bands[2] = (EqualizerBand){10000, 70, 140, EQUALIZER_PEAK};
    CHECK_EQUAL(-70, equalizer_preamp_tenths(&curve, 44100));
    curve.bands[2].gain_tenths = 400;
    CHECK_EQUAL(-120, equalizer_preamp_tenths(&curve, 44100));
}

static void a_full_scale_boost_saturates_instead_of_wrapping(void)
{
    EqualizerCurve curve = {.band_count = 2};
    curve.bands[0] = (EqualizerBand){100, 120, 50, EQUALIZER_PEAK};
    curve.bands[1] = (EqualizerBand){200, -120, 300, EQUALIZER_PEAK};
    Equalizer equalizer = equalizer_for(curve, 44100);
    for (int frame = 0; frame < 20000; frame++) {
        int32_t sample = (frame / 100) % 2 == 0 ? FULL_SCALE : -FULL_SCALE;
        int32_t left = sample;
        int32_t right = -sample;
        process_pair(&equalizer, &left, &right);
        CHECK(left <= FULL_SCALE && left >= -FULL_SCALE - 1);
        CHECK(right <= FULL_SCALE && right >= -FULL_SCALE - 1);
    }
}

static void both_channels_are_filtered_on_their_own(void)
{
    Equalizer equalizer = equalizer_for(one_band(1000, -6, 14), 48000);
    int32_t left = 0;
    int32_t right = 0;
    for (int frame = 0; frame < 2000; frame++) {
        left = (int32_t)(HALF_SCALE * sin(2.0 * PI * 1000.0 * frame / 48000));
        right = 0;
        process_pair(&equalizer, &left, &right);
        CHECK_EQUAL(0, right);
    }
}

static void changing_a_gain_keeps_the_filter_history(void)
{
    Equalizer equalizer = equalizer_for(one_band(1000, -6, 14), 48000);
    int32_t left = (int32_t)HALF_SCALE;
    int32_t right = (int32_t)HALF_SCALE;
    process_pair(&equalizer, &left, &right);
    int32_t state = equalizer.current.left[0][0];
    CHECK(state != 0);
    EqualizerCurve softer = one_band(1000, -3, 14);
    equalizer_set_curve(&equalizer, &softer);
    CHECK_EQUAL(state, equalizer.current.left[0][0]);
    EqualizerCurve moved = one_band(2000, -3, 14);
    equalizer_set_curve(&equalizer, &moved);
    CHECK(equalizer.has_pending);
    CHECK_EQUAL(2000, equalizer.pending_curve.bands[0].frequency_hz);
    for (int frame = 0; frame < 2 * EQUALIZER_FADE_FRAMES; frame++) {
        int32_t faded_left = (int32_t)HALF_SCALE;
        int32_t faded_right = (int32_t)HALF_SCALE;
        process_pair(&equalizer, &faded_left, &faded_right);
    }
    CHECK(!equalizer.has_pending);
    CHECK_EQUAL(2000, equalizer.current.curve.bands[0].frequency_hz);
}

static void a_fade_can_finish_inside_a_block(void)
{
    Equalizer equalizer = equalizer_for(one_band(1000, -6, 14), 48000);
    EqualizerCurve moved = one_band(2000, -3, 14);
    equalizer_set_curve(&equalizer, &moved);
    int32_t frames[2 * 502] = {0};
    equalizer_process(&equalizer, frames, 502);
    equalizer_process(&equalizer, frames, 32);
    CHECK(!equalizer.has_pending);
    CHECK_EQUAL(2000, equalizer.current.curve.bands[0].frequency_hz);
}

static int32_t largest_step(Equalizer *equalizer, int frames, int32_t *previous)
{
    int32_t largest = 0;
    for (int frame = 0; frame < frames; frame++) {
        int32_t left = (int32_t)HALF_SCALE;
        int32_t right = (int32_t)HALF_SCALE;
        process_pair(equalizer, &left, &right);
        int32_t step = abs(left - *previous);
        largest = step > largest ? step : largest;
        *previous = left;
    }
    return largest;
}

static void a_change_during_a_fade_carries_on_from_the_current_level(void)
{
    EqualizerCurve first = one_band(1000, 0, 14);
    first.bands[0].gain_tenths = -1;
    Equalizer equalizer = equalizer_for(first, 48000);
    int32_t previous = (int32_t)HALF_SCALE;
    int32_t smooth = (int32_t)(HALF_SCALE / 64);
    CHECK(largest_step(&equalizer, 64, &previous) < smooth);
    EqualizerCurve second = first;
    second.bands[0].frequency_hz = 2000;
    equalizer_set_curve(&equalizer, &second);
    CHECK(largest_step(&equalizer, EQUALIZER_FADE_FRAMES + 96, &previous) < smooth);
    CHECK(equalizer.has_pending && !equalizer.fading_out);
    EqualizerCurve third = first;
    third.bands[0].frequency_hz = 3000;
    equalizer_set_curve(&equalizer, &third);
    CHECK(largest_step(&equalizer, 40, &previous) < smooth);
    equalizer_set_curve(&equalizer, &second);
    CHECK(largest_step(&equalizer, 3 * EQUALIZER_FADE_FRAMES, &previous) < smooth);
    CHECK(!equalizer.has_pending);
    CHECK_EQUAL(2000, equalizer.current.curve.bands[0].frequency_hz);
}

static void silence_after_loud_bass_comes_out_as_silence(void)
{
    EqualizerCurve curve = {.band_count = 7};
    static const uint16_t CENTRES[7] = {63, 160, 400, 1000, 2500, 6300, 16000};
    for (int band = 0; band < 7; band++) {
        curve.bands[band] = (EqualizerBand){CENTRES[band], 120, 100, EQUALIZER_PEAK};
    }
    Equalizer equalizer = equalizer_for(curve, 96000);
    for (int frame = 0; frame < 96000; frame++) {
        int32_t sample = (int32_t)(HALF_SCALE * sin(2.0 * PI * 50.0 * frame / 96000));
        int32_t left = sample;
        int32_t right = sample;
        process_pair(&equalizer, &left, &right);
    }
    int32_t left = 0;
    int32_t right = 0;
    for (int frame = 0; frame < 96000; frame++) {
        left = 0;
        right = 0;
        process_pair(&equalizer, &left, &right);
    }
    CHECK_EQUAL(0, left);
    CHECK_EQUAL(0, right);
}

static void a_batch_sounds_the_same_as_one_frame_at_a_time(void)
{
    EqualizerCurve curve = {.band_count = 3};
    curve.bands[0] = (EqualizerBand){63, 60, 100, EQUALIZER_PEAK};
    curve.bands[1] = (EqualizerBand){1000, -40, 100, EQUALIZER_PEAK};
    curve.bands[2] = (EqualizerBand){10000, 30, 140, EQUALIZER_PEAK};
    Equalizer single = equalizer_for(curve, 96000);
    Equalizer batched = equalizer_for(curve, 96000);
    int32_t frames[2 * 64];
    for (int frame = 0; frame < 64; frame++) {
        frames[2 * frame] = (int32_t)(HALF_SCALE * sin(2.0 * PI * 440.0 * frame / 96000));
        frames[2 * frame + 1] = (int32_t)(HALF_SCALE * sin(2.0 * PI * 3000.0 * frame / 96000));
    }
    int32_t expected[2 * 64];
    for (int frame = 0; frame < 64; frame++) {
        expected[2 * frame] = frames[2 * frame];
        expected[2 * frame + 1] = frames[2 * frame + 1];
        process_pair(&single, &expected[2 * frame], &expected[2 * frame + 1]);
    }
    equalizer_process(&batched, frames, 40);
    equalizer_process(&batched, frames + 80, 24);
    for (int index = 0; index < 2 * 64; index++) {
        CHECK_EQUAL(expected[index], frames[index]);
    }
}

static void a_tenth_decibel_cut_is_honoured(void)
{
    EqualizerCurve curve = one_band(1000, -6, 14);
    curve.bands[0].gain_tenths = -6;
    Equalizer equalizer = equalizer_for(curve, 48000);
    CHECK(near(-0.6, gain_at(&equalizer, 1000.0, 48000), 0.15));
}

static void a_very_low_q_is_honoured(void)
{
    EqualizerCurve curve = one_band(100, -6, 1);
    Equalizer equalizer = equalizer_for(curve, 48000);
    CHECK(equalizer_active(&equalizer));
    CHECK(near(-6.0, gain_at(&equalizer, 100.0, 48000), 0.4));
}

static void a_preamp_only_curve_is_active(void)
{
    EqualizerCurve curve = {.band_count = 0, .preamp_set = 1, .preamp_tenths = -55};
    Equalizer equalizer = equalizer_for(curve, 48000);
    CHECK(equalizer_active(&equalizer));
    CHECK(near(-5.5, gain_at(&equalizer, 1000.0, 48000), 0.15));
}

static void a_file_preamp_never_removes_automatic_headroom(void)
{
    EqualizerCurve curve = one_band(1000, 6, 14);
    curve.preamp_set = 1;
    curve.preamp_tenths = -10;
    CHECK_EQUAL(-60, equalizer_preamp_tenths(&curve, 48000));
    curve.preamp_tenths = -100;
    CHECK_EQUAL(-100, equalizer_preamp_tenths(&curve, 48000));
}

static void a_band_too_high_for_the_rate_is_skipped_alone(void)
{
    EqualizerCurve curve = {.band_count = 2};
    curve.bands[0] = (EqualizerBand){1000, -60, 140, EQUALIZER_PEAK};
    curve.bands[1] = (EqualizerBand){30000, 60, 140, EQUALIZER_PEAK};
    Equalizer equalizer = equalizer_for(curve, 44100);
    CHECK(equalizer_valid(&equalizer));
    CHECK_EQUAL(1, equalizer.current.stage_count);
    CHECK_EQUAL(0, equalizer_preamp_tenths(&curve, 44100));
    CHECK(near(-6.0, gain_at(&equalizer, 1000.0, 44100), 0.2));
    equalizer_set_rate(&equalizer, 96000);
    CHECK_EQUAL(2, equalizer.current.stage_count);
    CHECK_EQUAL(-60, equalizer_preamp_tenths(&curve, 96000));
}

static int32_t saturated(double value)
{
    double rounded = floor(value + 0.5);
    return rounded > TOP ? FULL_SCALE : rounded < -TOP - 1.0 ? -FULL_SCALE - 1 : (int32_t)rounded;
}

static int worst_error_against_a_double_model(EqualizerCurve curve, uint32_t rate, double frequency,
                                              int frames)
{
    Equalizer equalizer = equalizer_for(curve, rate);
    const EqualizerChain *chain = &equalizer.current;
    double preamp = chain->preamp / 1073741824.0;
    double band[EQUALIZER_MAX_BANDS] = {0};
    double low[EQUALIZER_MAX_BANDS] = {0};
    int worst = 0;
    for (int frame = 0; frame < frames; frame++) {
        int32_t sample = (int32_t)(TOP * sin(2.0 * PI * frequency * frame / rate));
        double x = sample * 16.0 * preamp;
        for (int index = 0; index < chain->stage_count; index++) {
            EqualizerStage stage = chain->stages[index];
            double g = stage.g / 16384.0;
            double v1 = stage.a1 / 16384.0 * (band[index] + g * (x - low[index]));
            double v2 = low[index] + g * v1;
            band[index] = 2.0 * v1 - band[index];
            low[index] = 2.0 * v2 - low[index];
            x += stage.m1 / 8192.0 * v1 + stage.m2 / 16384.0 * v2;
        }
        int32_t expected = saturated(x / 16.0);
        int32_t left = sample;
        int32_t right = sample;
        process_pair(&equalizer, &left, &right);
        int error = abs(left - expected);
        worst = error > worst ? error : worst;
    }
    return worst;
}

static void a_band_at_the_q_cap_holds_a_full_scale_tone_at_its_centre(void)
{
    static const uint32_t RATES[] = {44100, 48000, 96000};
    static const uint16_t CENTRES[] = {20, 100, 1000, 5000, 10000, 16000, 19000, 40000};
    static const int16_t GAINS[] = {1, 20, 40, 120, -120};
    for (size_t rate = 0; rate < sizeof RATES / sizeof RATES[0]; rate++) {
        for (size_t centre = 0; centre < sizeof CENTRES / sizeof CENTRES[0]; centre++) {
            if (CENTRES[centre] * 20u >= RATES[rate] * 9u) {
                continue;
            }
            for (size_t gain = 0; gain < sizeof GAINS / sizeof GAINS[0]; gain++) {
                EqualizerCurve curve = {.band_count = 1};
                curve.bands[0] = (EqualizerBand){CENTRES[centre], GAINS[gain],
                                                 EQUALIZER_Q_MAX_HUNDREDTHS, EQUALIZER_PEAK};
                int frames = (int)RATES[rate] / 2 + (int)(RATES[rate] * 4u / CENTRES[centre]);
                CHECK(worst_error_against_a_double_model(curve, RATES[rate], CENTRES[centre],
                                                         frames) <= 16);
            }
        }
    }
}

static void stacked_boosts_at_the_q_cap_never_wrap(void)
{
    EqualizerCurve curve = {.band_count = 3};
    for (int band = 0; band < 3; band++) {
        curve.bands[band] = (EqualizerBand){2000, 120, EQUALIZER_Q_MAX_HUNDREDTHS, EQUALIZER_PEAK};
    }
    CHECK(worst_error_against_a_double_model(curve, 48000, 2000.0, 48000) <= 16);
}

static void ordinary_curves_keep_their_preamp(void)
{
    EqualizerCurve curve = {.band_count = 7};
    static const uint16_t CENTRES[7] = {63, 160, 400, 1000, 2500, 6300, 16000};
    for (int band = 0; band < 7; band++) {
        curve.bands[band] = (EqualizerBand){CENTRES[band], 120, 100, EQUALIZER_PEAK};
    }
    static const uint32_t RATES[] = {44100, 48000, 96000};
    for (size_t rate = 0; rate < sizeof RATES / sizeof RATES[0]; rate++) {
        Equalizer equalizer = equalizer_for(curve, RATES[rate]);
        CHECK(near(-12.0, 20.0 * log10(equalizer.current.preamp / 1073741824.0), 0.05));
    }
}

static double cookbook_gain_db(EqualizerBand band, uint32_t rate, double frequency)
{
    double a = pow(10.0, band.gain_tenths / 400.0);
    double w0 = 2.0 * PI * band.frequency_hz / rate;
    double alpha = sin(w0) / (2.0 * band.q_hundredths / 100.0);
    double c = cos(w0);
    double root = 2.0 * sqrt(a) * alpha;
    double b[3];
    double d[3];
    if (band.filter == EQUALIZER_PEAK) {
        b[0] = 1.0 + alpha * a;
        b[1] = -2.0 * c;
        b[2] = 1.0 - alpha * a;
        d[0] = 1.0 + alpha / a;
        d[1] = -2.0 * c;
        d[2] = 1.0 - alpha / a;
    } else if (band.filter == EQUALIZER_LOW_SHELF) {
        b[0] = a * ((a + 1.0) - (a - 1.0) * c + root);
        b[1] = 2.0 * a * ((a - 1.0) - (a + 1.0) * c);
        b[2] = a * ((a + 1.0) - (a - 1.0) * c - root);
        d[0] = (a + 1.0) + (a - 1.0) * c + root;
        d[1] = -2.0 * ((a - 1.0) + (a + 1.0) * c);
        d[2] = (a + 1.0) + (a - 1.0) * c - root;
    } else {
        b[0] = a * ((a + 1.0) + (a - 1.0) * c + root);
        b[1] = -2.0 * a * ((a - 1.0) + (a + 1.0) * c);
        b[2] = a * ((a + 1.0) + (a - 1.0) * c - root);
        d[0] = (a + 1.0) - (a - 1.0) * c + root;
        d[1] = 2.0 * ((a - 1.0) - (a + 1.0) * c);
        d[2] = (a + 1.0) - (a - 1.0) * c - root;
    }
    double w = 2.0 * PI * frequency / rate;
    double top_real = b[0] + b[1] * cos(w) + b[2] * cos(2.0 * w);
    double top_imaginary = -b[1] * sin(w) - b[2] * sin(2.0 * w);
    double bottom_real = d[0] + d[1] * cos(w) + d[2] * cos(2.0 * w);
    double bottom_imaginary = -d[1] * sin(w) - d[2] * sin(2.0 * w);
    return 10.0 * log10((top_real * top_real + top_imaginary * top_imaginary) /
                        (bottom_real * bottom_real + bottom_imaginary * bottom_imaginary));
}

static double curve_gain_db(const EqualizerCurve *curve, uint32_t rate, double frequency)
{
    double total = 0.0;
    for (int index = 0; index < curve->band_count; index++) {
        if (equalizer_band_audible(curve->bands[index], rate)) {
            total += cookbook_gain_db(curve->bands[index], rate, frequency);
        }
    }
    return total;
}

static double preamp_db(const Equalizer *equalizer)
{
    return 20.0 * log10(equalizer->current.preamp / 1073741824.0);
}

static bool follows_the_cookbook(EqualizerCurve curve, uint32_t rate, const double *frequencies,
                                 int count, double tolerance)
{
    Equalizer equalizer = equalizer_for(curve, rate);
    double level = equalizer_preamp_tenths(&curve, rate) / 10.0;
    bool all = true;
    for (int index = 0; index < count; index++) {
        double expected = level + curve_gain_db(&curve, rate, frequencies[index]);
        double actual = gain_at(&equalizer, frequencies[index], rate);
        if (!near(expected, actual, tolerance)) {
            printf("  %.0f Hz: expected %.2f dB, got %.2f dB\n", frequencies[index], expected,
                   actual);
            all = false;
        }
    }
    return all;
}

static void a_low_shelf_follows_the_cookbook_curve(void)
{
    static const double PROBES[] = {30.0, 60.0, 105.0, 210.0, 1000.0, 10000.0};
    EqualizerCurve curve = {.band_count = 1};
    curve.bands[0] = (EqualizerBand){105, 58, 70, EQUALIZER_LOW_SHELF};
    CHECK(follows_the_cookbook(curve, 44100, PROBES, 6, 0.2));
    curve.bands[0] = (EqualizerBand){300, -80, 200, EQUALIZER_LOW_SHELF};
    CHECK(follows_the_cookbook(curve, 96000, PROBES, 6, 0.2));
}

static void a_high_shelf_follows_the_cookbook_curve(void)
{
    static const double PROBES[] = {100.0, 2000.0, 5000.0, 10000.0, 14000.0, 18000.0};
    EqualizerCurve curve = {.band_count = 1};
    curve.bands[0] = (EqualizerBand){10000, -17, 70, EQUALIZER_HIGH_SHELF};
    CHECK(follows_the_cookbook(curve, 44100, PROBES, 6, 0.2));
    curve.bands[0] = (EqualizerBand){4000, 60, 70, EQUALIZER_HIGH_SHELF};
    CHECK(follows_the_cookbook(curve, 48000, PROBES, 6, 0.2));
    Equalizer equalizer = equalizer_for(curve, 48000);
    CHECK(near(0.0, preamp_db(&equalizer), 0.05));
}

static const EqualizerBand HD650[] = {
    {105, 58, 70, EQUALIZER_LOW_SHELF}, {180, -31, 53, EQUALIZER_PEAK},
    {1350, 19, 142, EQUALIZER_PEAK},    {3120, -24, 267, EQUALIZER_PEAK},
    {5480, 36, 385, EQUALIZER_PEAK},    {10000, -17, 70, EQUALIZER_HIGH_SHELF},
    {61, 12, 118, EQUALIZER_PEAK},      {7650, -29, 441, EQUALIZER_PEAK},
    {11200, 14, 206, EQUALIZER_PEAK},   {19400, -11, 82, EQUALIZER_PEAK}};

static EqualizerCurve hd650_curve(void)
{
    EqualizerCurve curve = {
        .band_count = EQUALIZER_MAX_BANDS, .preamp_set = 1, .preamp_tenths = -62};
    for (int index = 0; index < EQUALIZER_MAX_BANDS; index++) {
        curve.bands[index] = HD650[index];
    }
    return curve;
}

static void ten_autoeq_bands_follow_the_cookbook_curve(void)
{
    static const double PROBES[] = {30.0,   61.0,   105.0,  180.0,   500.0,   1350.0,
                                    3120.0, 5480.0, 7650.0, 10000.0, 11200.0, 14000.0};
    EqualizerCurve curve = hd650_curve();
    CHECK(follows_the_cookbook(curve, 48000, PROBES, 12, 0.3));
    CHECK(follows_the_cookbook(curve, 96000, PROBES, 12, 0.3));
    Equalizer equalizer = equalizer_for(curve, 44100);
    CHECK_EQUAL(EQUALIZER_MAX_BANDS, equalizer.current.stage_count);
    CHECK(near(-6.2 - 1.7, preamp_db(&equalizer), 0.05));
}

static void shelves_at_the_q_cap_hold_a_full_scale_tone(void)
{
    static const uint16_t CENTRES[] = {30, 1000, 12000};
    static const int16_t GAINS[] = {120, -120, 30};
    for (size_t centre = 0; centre < sizeof CENTRES / sizeof CENTRES[0]; centre++) {
        for (size_t gain = 0; gain < sizeof GAINS / sizeof GAINS[0]; gain++) {
            for (int filter = EQUALIZER_LOW_SHELF; filter <= EQUALIZER_HIGH_SHELF; filter++) {
                EqualizerCurve curve = {.band_count = 1};
                curve.bands[0] = (EqualizerBand){CENTRES[centre], GAINS[gain],
                                                 EQUALIZER_Q_MAX_HUNDREDTHS, (uint8_t)filter};
                CHECK(worst_error_against_a_double_model(curve, 48000, CENTRES[centre], 48000) <=
                      256);
            }
        }
    }
}

static void a_deep_file_preamp_is_honoured(void)
{
    EqualizerCurve curve = {.band_count = 0, .preamp_set = 1, .preamp_tenths = -240};
    Equalizer equalizer = equalizer_for(curve, 48000);
    CHECK(near(-24.0, gain_at(&equalizer, 1000.0, 48000), 0.2));
}

static void a_reset_clears_the_filter_state(void)
{
    EqualizerCurve curve = one_band(100, 60, 14);
    Equalizer used = equalizer_for(curve, 44100);
    for (int frame = 0; frame < SETTLE_FRAMES; frame++) {
        int32_t sample = (int32_t)(HALF_SCALE * sin(2.0 * PI * 100.0 * frame / 44100));
        int32_t pair[2] = {sample, sample};
        equalizer_process(&used, pair, 1);
    }
    equalizer_reset(&used);
    Equalizer fresh = equalizer_for(curve, 44100);
    int32_t a[2] = {FULL_SCALE, FULL_SCALE};
    int32_t b[2] = {FULL_SCALE, FULL_SCALE};
    equalizer_process(&used, a, 1);
    equalizer_process(&fresh, b, 1);
    CHECK_EQUAL(b[0], a[0]);
    CHECK_EQUAL(b[1], a[1]);
}

static void a_reset_applies_a_pending_curve(void)
{
    Equalizer used = equalizer_for(one_band(1000, 60, 14), 44100);
    EqualizerCurve next = one_band(2000, -60, 14);
    equalizer_set_curve(&used, &next);
    equalizer_reset(&used);
    Equalizer fresh = equalizer_for(next, 44100);
    int32_t a[2] = {FULL_SCALE, FULL_SCALE};
    int32_t b[2] = {FULL_SCALE, FULL_SCALE};
    equalizer_process(&used, a, 1);
    equalizer_process(&fresh, b, 1);
    CHECK_EQUAL(b[0], a[0]);
    CHECK_EQUAL(b[1], a[1]);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_flat_curve_leaves_the_equalizer_off),
        TEST_CASE(a_cut_lowers_its_band_and_leaves_far_tones_alone),
        TEST_CASE(a_boost_is_paid_for_with_headroom),
        TEST_CASE(a_low_band_stays_accurate_at_ninety_six_kilohertz),
        TEST_CASE(tracks_above_ninety_six_kilohertz_play_without_it),
        TEST_CASE(bands_near_the_top_of_a_low_rate_are_skipped),
        TEST_CASE(the_largest_audible_boost_sets_the_preamp),
        TEST_CASE(a_full_scale_boost_saturates_instead_of_wrapping),
        TEST_CASE(both_channels_are_filtered_on_their_own),
        TEST_CASE(changing_a_gain_keeps_the_filter_history),
        TEST_CASE(a_fade_can_finish_inside_a_block),
        TEST_CASE(a_change_during_a_fade_carries_on_from_the_current_level),
        TEST_CASE(silence_after_loud_bass_comes_out_as_silence),
        TEST_CASE(a_batch_sounds_the_same_as_one_frame_at_a_time),
        TEST_CASE(a_tenth_decibel_cut_is_honoured),
        TEST_CASE(a_very_low_q_is_honoured),
        TEST_CASE(a_preamp_only_curve_is_active),
        TEST_CASE(a_file_preamp_never_removes_automatic_headroom),
        TEST_CASE(a_band_too_high_for_the_rate_is_skipped_alone),
        TEST_CASE(a_deep_file_preamp_is_honoured),
        TEST_CASE(a_reset_clears_the_filter_state),
        TEST_CASE(a_reset_applies_a_pending_curve),
        TEST_CASE(a_band_at_the_q_cap_holds_a_full_scale_tone_at_its_centre),
        TEST_CASE(stacked_boosts_at_the_q_cap_never_wrap),
        TEST_CASE(ordinary_curves_keep_their_preamp),
        TEST_CASE(a_low_shelf_follows_the_cookbook_curve),
        TEST_CASE(a_high_shelf_follows_the_cookbook_curve),
        TEST_CASE(ten_autoeq_bands_follow_the_cookbook_curve),
        TEST_CASE(shelves_at_the_q_cap_hold_a_full_scale_tone),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
