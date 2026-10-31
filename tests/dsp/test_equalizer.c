#include <math.h>

#include "check.h"
#include "dsp/equalizer.h"

enum {
    SETTLE_FRAMES = 9600,
    MEASURE_FRAMES = 9600,
    FULL_SCALE = 8388607
};

static const double PI = 3.14159265358979323846;
static const double HALF_SCALE = 4194304.0;

static EqualizerCurve one_band(uint16_t frequency, int8_t gain, uint8_t q_tenths)
{
    EqualizerCurve curve = {.band_count = 1};
    curve.bands[0] =
        (EqualizerBand){.frequency_hz = frequency, .gain_db = gain, .q_tenths = q_tenths};
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
    CHECK_EQUAL(-6, equalizer_preamp_db(&curve, 44100));
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
    CHECK_EQUAL(0, equalizer_preamp_db(&curve, 32000));
}

static void the_largest_audible_boost_sets_the_preamp(void)
{
    EqualizerCurve curve = {.band_count = 3};
    curve.bands[0] = (EqualizerBand){100, 4, 14};
    curve.bands[1] = (EqualizerBand){1000, -6, 14};
    curve.bands[2] = (EqualizerBand){10000, 7, 14};
    CHECK_EQUAL(-7, equalizer_preamp_db(&curve, 44100));
    curve.bands[2].gain_db = 40;
    CHECK_EQUAL(-12, equalizer_preamp_db(&curve, 44100));
}

static void a_full_scale_boost_saturates_instead_of_wrapping(void)
{
    EqualizerCurve curve = {.band_count = 2};
    curve.bands[0] = (EqualizerBand){100, 12, 5};
    curve.bands[1] = (EqualizerBand){200, -12, 30};
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
    int32_t state = equalizer.left[0][0];
    CHECK(state != 0);
    EqualizerCurve softer = one_band(1000, -3, 14);
    equalizer_set_curve(&equalizer, &softer);
    CHECK_EQUAL(state, equalizer.left[0][0]);
    EqualizerCurve moved = one_band(2000, -3, 14);
    equalizer_set_curve(&equalizer, &moved);
    CHECK_EQUAL(0, equalizer.left[0][0]);
}

static void silence_after_loud_bass_comes_out_as_silence(void)
{
    EqualizerCurve curve = {.band_count = 7};
    static const uint16_t CENTRES[7] = {63, 160, 400, 1000, 2500, 6300, 16000};
    for (int band = 0; band < 7; band++) {
        curve.bands[band] = (EqualizerBand){CENTRES[band], 12, 10};
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
    curve.bands[0] = (EqualizerBand){63, 6, 10};
    curve.bands[1] = (EqualizerBand){1000, -4, 10};
    curve.bands[2] = (EqualizerBand){10000, 3, 14};
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
        TEST_CASE(silence_after_loud_bass_comes_out_as_silence),
        TEST_CASE(a_batch_sounds_the_same_as_one_frame_at_a_time),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
