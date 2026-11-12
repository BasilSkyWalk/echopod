#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "check.h"
#include "dsp/speed.h"

enum {
    SOURCE_MAX = 44100,
    RESULT_MAX = 65536,
    PULL_FRAMES = 32,
    PUSH_FRAMES = 32,
    RATE_44K = 44100,
    RATE_22K = 22050,
    SEQUENCE_44K = 1760,
    LEAD_OUT_44K = 1320,
    WINDOW_44K = 2416,
    LEAD_OUT_22K = 660,
    DC_LEVEL = 1000000
};

static const double TONE_HZ = 441.0;
static const double PI = 3.14159265358979323846;
static const double HALF_SCALE = 4194304.0;
static const uint8_t SPEEDS[] = {3, 5, 6, 7, 8};

static SpeedStage stage;
static int32_t source[SPEED_CHANNELS * SOURCE_MAX];
static int32_t result[SPEED_CHANNELS * RESULT_MAX];

static void configure(uint8_t quarters, uint32_t rate)
{
    speed_configure(&stage, (SpeedSetting){quarters}, rate);
}

static void fill_tone(double hz, double rate, uint32_t frames)
{
    for (uint32_t frame = 0; frame < frames; frame++) {
        int32_t sample = (int32_t)(HALF_SCALE * sin(2.0 * PI * hz * frame / rate));
        source[SPEED_CHANNELS * frame] = sample;
        source[SPEED_CHANNELS * frame + 1] = sample;
    }
}

static void fill_chord(uint32_t frames)
{
    const double tones[] = {441.0, 1003.7, 2347.3};
    for (uint32_t frame = 0; frame < frames; frame++) {
        double sum = 0.0;
        for (unsigned tone = 0; tone < 3; tone++) {
            sum += sin(2.0 * PI * tones[tone] * frame / (double)RATE_44K) / 3.0;
        }
        source[SPEED_CHANNELS * frame] = (int32_t)(HALF_SCALE * sum);
        source[SPEED_CHANNELS * frame + 1] = (int32_t)(HALF_SCALE * sum);
    }
}

static void fill_constant(int32_t level, uint32_t frames)
{
    for (uint32_t index = 0; index < SPEED_CHANNELS * frames; index++) {
        source[index] = level;
    }
}

static uint32_t drain(uint32_t made)
{
    uint32_t got;
    do {
        got = speed_pull(&stage, result + SPEED_CHANNELS * made, PULL_FRAMES);
        made += got;
    } while (got > 0 && made + PULL_FRAMES <= RESULT_MAX);
    return made;
}

static uint32_t play(uint32_t frames)
{
    uint32_t pushed = 0;
    uint32_t made = 0;
    while (pushed < frames) {
        uint32_t chunk = frames - pushed < PUSH_FRAMES ? frames - pushed : PUSH_FRAMES;
        uint32_t taken = speed_push(&stage, source + SPEED_CHANNELS * pushed, chunk);
        uint32_t before = made;
        made = drain(made);
        pushed += taken;
        if (taken == 0 && made == before) {
            CHECK(false);
            return made;
        }
    }
    speed_finish(&stage);
    made = drain(made);
    CHECK(speed_drained(&stage));
    return made;
}

static uint32_t ceil_quarters(uint32_t frames, uint32_t quarters)
{
    return (4 * frames + quarters - 1) / quarters;
}

static double tone_hz(uint32_t from, uint32_t to, double rate)
{
    uint32_t first = 0;
    uint32_t last = 0;
    uint32_t count = 0;
    for (uint32_t frame = from + 1; frame < to; frame++) {
        if (result[SPEED_CHANNELS * (frame - 1)] < 0 && result[SPEED_CHANNELS * frame] >= 0) {
            first = count == 0 ? frame : first;
            last = frame;
            count++;
        }
    }
    return count > 1 ? (count - 1) * rate / (last - first) : 0.0;
}

static bool near(double expected, double actual, double tolerance)
{
    return fabs(expected - actual) <= tolerance;
}

static int32_t jump_at(const int32_t *frames, uint32_t frame)
{
    int32_t gap = frames[SPEED_CHANNELS * (frame + 1)] - frames[SPEED_CHANNELS * frame];
    return gap < 0 ? -gap : gap;
}

static int32_t largest_jump(const int32_t *frames, uint32_t count)
{
    int32_t largest = 0;
    for (uint32_t frame = 0; frame + 1 < count; frame++) {
        largest = jump_at(frames, frame) > largest ? jump_at(frames, frame) : largest;
    }
    return largest;
}

static void unity_and_out_of_range_speeds_bypass_the_stage(void)
{
    const uint8_t bypassed[] = {0, 2, 4, 9};
    for (unsigned index = 0; index < sizeof bypassed; index++) {
        CHECK_EQUAL(SPEED_BYPASS, speed_method_for((SpeedSetting){bypassed[index]}, RATE_44K));
    }
    configure(4, RATE_44K);
    CHECK_EQUAL(SPEED_BYPASS, speed_method(&stage));
    CHECK_EQUAL(SPEED_UNITY_QUARTERS, speed_quarters(&stage));
    CHECK_EQUAL(0, speed_space(&stage));
}

static void the_stage_stretches_up_to_96_khz_and_bypasses_above(void)
{
    SpeedSetting setting = {6};
    CHECK_EQUAL(SPEED_STRETCH, speed_method_for(setting, RATE_22K));
    CHECK_EQUAL(SPEED_STRETCH, speed_method_for(setting, 48000));
    CHECK_EQUAL(SPEED_STRETCH, speed_method_for(setting, 64000));
    CHECK_EQUAL(SPEED_STRETCH, speed_method_for(setting, 96000));
    CHECK_EQUAL(SPEED_BYPASS, speed_method_for(setting, 96001));
    CHECK_EQUAL(SPEED_BYPASS, speed_method_for(setting, 176400));
    CHECK_EQUAL(SPEED_BYPASS, speed_method_for((SpeedSetting){3}, 192000));
}

static void a_fast_speed_is_capped_where_decoding_would_pass_144_khz(void)
{
    CHECK_EQUAL(8, speed_played_quarters((SpeedSetting){8}, 48000));
    CHECK_EQUAL(8, speed_played_quarters((SpeedSetting){8}, 64000));
    CHECK_EQUAL(8, speed_played_quarters((SpeedSetting){8}, 72000));
    CHECK_EQUAL(6, speed_played_quarters((SpeedSetting){8}, 88200));
    CHECK_EQUAL(6, speed_played_quarters((SpeedSetting){8}, 96000));
    CHECK_EQUAL(6, speed_played_quarters((SpeedSetting){7}, 96000));
    CHECK_EQUAL(5, speed_played_quarters((SpeedSetting){5}, 96000));
    CHECK_EQUAL(3, speed_played_quarters((SpeedSetting){3}, 96000));
    CHECK_EQUAL(4, speed_played_quarters((SpeedSetting){8}, 176400));
    configure(8, 96000);
    CHECK_EQUAL(SPEED_STRETCH, speed_method(&stage));
    CHECK_EQUAL(6, speed_quarters(&stage));
}

static void the_equalizer_sees_the_decoded_rate_once_that_passes_96_khz(void)
{
    CHECK_EQUAL(RATE_44K, speed_equalizer_rate(RATE_44K, 4));
    CHECK_EQUAL(48000, speed_equalizer_rate(48000, 8));
    CHECK_EQUAL(64000, speed_equalizer_rate(64000, 6));
    CHECK_EQUAL(128000, speed_equalizer_rate(64000, 8));
    CHECK_EQUAL(144000, speed_equalizer_rate(96000, 6));
    CHECK_EQUAL(96000, speed_equalizer_rate(96000, 3));
    CHECK_EQUAL(192000, speed_equalizer_rate(192000, 4));
}

static void the_stage_bypasses_below_the_lowest_stretch_rate(void)
{
    SpeedSetting setting = {6};
    CHECK_EQUAL(SPEED_BYPASS, speed_method_for(setting, 0));
    CHECK_EQUAL(SPEED_BYPASS, speed_method_for(setting, 24));
    CHECK_EQUAL(SPEED_BYPASS, speed_method_for(setting, 3999));
    CHECK_EQUAL(SPEED_STRETCH, speed_method_for(setting, 4000));
}

static void a_rate_outside_the_stretch_range_plays_at_normal_speed(void)
{
    const uint32_t rates[] = {24, 176400};
    for (unsigned index = 0; index < 2; index++) {
        configure(6, rates[index]);
        CHECK_EQUAL(SPEED_BYPASS, speed_method(&stage));
        CHECK_EQUAL(SPEED_UNITY_QUARTERS, speed_quarters(&stage));
        CHECK_EQUAL(0, speed_space(&stage));
        CHECK(speed_drained(&stage));
    }
}

static void the_stretch_starts_with_the_first_input_unchanged(void)
{
    fill_tone(1234.5, RATE_44K, 10000);
    for (uint32_t frame = 0; frame < 10000; frame++) {
        source[SPEED_CHANNELS * frame + 1] = source[SPEED_CHANNELS * frame] / 3 + (int32_t)frame;
    }
    for (unsigned index = 0; index < sizeof SPEEDS; index++) {
        configure(SPEEDS[index], RATE_44K);
        CHECK(play(10000) > LEAD_OUT_44K);
        for (uint32_t frame = 0; frame < LEAD_OUT_44K; frame++) {
            CHECK_EQUAL(source[SPEED_CHANNELS * frame], result[SPEED_CHANNELS * frame]);
            CHECK_EQUAL(source[SPEED_CHANNELS * frame + 1], result[SPEED_CHANNELS * frame + 1]);
        }
    }
}

static void the_stretch_passes_a_constant_unchanged_at_every_speed(void)
{
    fill_constant(DC_LEVEL, SOURCE_MAX);
    for (unsigned index = 0; index < sizeof SPEEDS; index++) {
        configure(SPEEDS[index], RATE_44K);
        uint32_t made = play(SOURCE_MAX);
        uint32_t exact = 4 * (SOURCE_MAX - 2 * WINDOW_44K) / SPEEDS[index];
        CHECK(made > exact);
        for (uint32_t frame = 0; frame < exact; frame++) {
            CHECK_EQUAL(DC_LEVEL, result[SPEED_CHANNELS * frame]);
            CHECK_EQUAL(DC_LEVEL, result[SPEED_CHANNELS * frame + 1]);
        }
    }
}

static void the_stretch_output_length_follows_the_speed(void)
{
    const uint32_t rates[] = {RATE_44K, RATE_22K};
    const uint32_t lead_outs[] = {LEAD_OUT_44K, LEAD_OUT_22K};
    for (unsigned rate = 0; rate < 2; rate++) {
        fill_tone(TONE_HZ, rates[rate], rates[rate]);
        for (unsigned index = 0; index < sizeof SPEEDS; index++) {
            configure(SPEEDS[index], rates[rate]);
            uint32_t made = play(rates[rate]);
            CHECK(made >= 4 * rates[rate] / SPEEDS[index]);
            CHECK(made <= ceil_quarters(rates[rate], SPEEDS[index]) + lead_outs[rate]);
        }
    }
}

static void the_stretch_keeps_a_sines_pitch_at_every_speed(void)
{
    const uint8_t speeds[] = {3, 6, 8};
    fill_tone(TONE_HZ, RATE_44K, SOURCE_MAX);
    for (unsigned index = 0; index < sizeof speeds; index++) {
        configure(speeds[index], RATE_44K);
        uint32_t made = play(SOURCE_MAX);
        double hz = tone_hz(SEQUENCE_44K, made - SEQUENCE_44K, RATE_44K);
        CHECK(near(TONE_HZ, hz, TONE_HZ * 0.01));
    }
}

static void above_48_khz_the_stretch_keeps_the_pitch_in_the_same_memory(void)
{
    const uint32_t rates[] = {64000, 88200, 96000};
    for (unsigned index = 0; index < 3; index++) {
        fill_tone(TONE_HZ, rates[index], SOURCE_MAX);
        configure(6, rates[index]);
        CHECK(speed_space(&stage) <= SPEED_FIFO_FRAMES);
        uint32_t made = play(SOURCE_MAX);
        CHECK(made >= 4 * SOURCE_MAX / 6);
        CHECK(made <= ceil_quarters(SOURCE_MAX, 6) + STRETCH_MAX_SEQUENCE);
        double hz = tone_hz(STRETCH_MAX_SEQUENCE, made - STRETCH_MAX_SEQUENCE, rates[index]);
        CHECK(near(TONE_HZ, hz, TONE_HZ * 0.01));
    }
}

static void the_stretch_splices_without_jumps(void)
{
    const uint8_t speeds[] = {3, 6, 8};
    fill_chord(SOURCE_MAX);
    int32_t allowed = largest_jump(source, SOURCE_MAX);
    allowed = allowed + allowed / 20 + 2;
    for (unsigned index = 0; index < sizeof speeds; index++) {
        configure(speeds[index], RATE_44K);
        uint32_t made = play(SOURCE_MAX);
        CHECK(largest_jump(result, made - SEQUENCE_44K) <= allowed);
    }
}

static void the_stretch_moves_both_channels_together(void)
{
    const uint8_t speeds[] = {3, 6, 8};
    fill_tone(TONE_HZ, RATE_44K, SOURCE_MAX);
    for (uint32_t frame = 0; frame < SOURCE_MAX; frame++) {
        source[SPEED_CHANNELS * frame + 1] = -source[SPEED_CHANNELS * frame];
    }
    for (unsigned index = 0; index < sizeof speeds; index++) {
        configure(speeds[index], RATE_44K);
        uint32_t made = play(SOURCE_MAX);
        CHECK(made > SEQUENCE_44K);
        for (uint32_t frame = 0; frame < made; frame++) {
            CHECK_EQUAL(-result[SPEED_CHANNELS * frame], result[SPEED_CHANNELS * frame + 1]);
        }
    }
}

static void a_finished_stretch_plays_out_its_tail_then_reports_drained(void)
{
    fill_tone(TONE_HZ, RATE_44K, 1000);
    configure(6, RATE_44K);
    uint32_t made = play(1000);
    CHECK(made >= 4 * 1000 / 6);
    CHECK(made <= ceil_quarters(1000, 6) + LEAD_OUT_44K);
    CHECK(speed_drained(&stage));
    CHECK_EQUAL(0, speed_pull(&stage, result, PULL_FRAMES));
}

static void configuring_again_forgets_the_previous_input(void)
{
    fill_tone(300.0, RATE_44K, 5000);
    configure(6, RATE_44K);
    CHECK_EQUAL(speed_space(&stage), speed_push(&stage, source, 5000));
    speed_pull(&stage, result, PULL_FRAMES);
    configure(6, RATE_44K);
    CHECK(!speed_drained(&stage));
    fill_tone(777.0, RATE_44K, 6000);
    uint32_t made = play(6000);
    CHECK(made > LEAD_OUT_44K);
    for (uint32_t frame = 0; frame < LEAD_OUT_44K; frame++) {
        CHECK_EQUAL(source[SPEED_CHANNELS * frame], result[SPEED_CHANNELS * frame]);
    }
}

static void check_progress(uint8_t quarters)
{
    configure(quarters, RATE_44K);
    uint32_t pushed = 0;
    for (int round = 0; round < 4000 && pushed < SOURCE_MAX; round++) {
        uint32_t space = speed_space(&stage);
        uint32_t chunk = SOURCE_MAX - pushed < PUSH_FRAMES ? SOURCE_MAX - pushed : PUSH_FRAMES;
        pushed += speed_push(&stage, source + SPEED_CHANNELS * pushed, chunk);
        CHECK(space > 0 || speed_pull(&stage, result, PULL_FRAMES) > 0);
        speed_pull(&stage, result, PULL_FRAMES);
    }
    CHECK_EQUAL(SOURCE_MAX, pushed);
}

static void the_stage_always_takes_input_or_gives_output(void)
{
    fill_tone(TONE_HZ, RATE_44K, SOURCE_MAX);
    check_progress(3);
    check_progress(8);
}

static void push_never_takes_more_than_the_space(void)
{
    fill_tone(TONE_HZ, RATE_44K, SOURCE_MAX);
    configure(8, RATE_44K);
    uint32_t space = speed_space(&stage);
    CHECK(space > 0 && space < SOURCE_MAX);
    CHECK_EQUAL(space, speed_push(&stage, source, SOURCE_MAX));
    CHECK_EQUAL(0, speed_space(&stage));
    CHECK_EQUAL(0, speed_push(&stage, source, PUSH_FRAMES));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(unity_and_out_of_range_speeds_bypass_the_stage),
        TEST_CASE(the_stage_stretches_up_to_96_khz_and_bypasses_above),
        TEST_CASE(a_fast_speed_is_capped_where_decoding_would_pass_144_khz),
        TEST_CASE(the_equalizer_sees_the_decoded_rate_once_that_passes_96_khz),
        TEST_CASE(the_stage_bypasses_below_the_lowest_stretch_rate),
        TEST_CASE(a_rate_outside_the_stretch_range_plays_at_normal_speed),
        TEST_CASE(the_stretch_starts_with_the_first_input_unchanged),
        TEST_CASE(the_stretch_passes_a_constant_unchanged_at_every_speed),
        TEST_CASE(the_stretch_output_length_follows_the_speed),
        TEST_CASE(the_stretch_keeps_a_sines_pitch_at_every_speed),
        TEST_CASE(above_48_khz_the_stretch_keeps_the_pitch_in_the_same_memory),
        TEST_CASE(the_stretch_splices_without_jumps),
        TEST_CASE(the_stretch_moves_both_channels_together),
        TEST_CASE(a_finished_stretch_plays_out_its_tail_then_reports_drained),
        TEST_CASE(configuring_again_forgets_the_previous_input),
        TEST_CASE(the_stage_always_takes_input_or_gives_output),
        TEST_CASE(push_never_takes_more_than_the_space),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
