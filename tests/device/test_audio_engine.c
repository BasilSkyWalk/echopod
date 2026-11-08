#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/audio_engine.h"
#include "device/flac_source.h"

enum {
    RING_FRAMES = 8192,
    RING_WORDS = 2 * RING_FRAMES,
    FULL_VOLUME = 50
};

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
} Fixture;

static FlacDecoder decoder;
static FlacBlockBuffer block;
static FlacSource source;
static uint32_t ring[RING_WORDS];
static AudioEngine engine;
static Fixture opened;
static SpeedStage stage;

static Fixture load(const char *name, const char *extension)
{
    char path[512];
    snprintf(path, sizeof path, "%s/flac/%s.%s", ECHO_FIXTURE_DIR, name, extension);
    Fixture fixture = {NULL, 0};
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return fixture;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fixture.bytes = malloc((size_t)size);
    if (fixture.bytes != NULL && fread(fixture.bytes, 1, (size_t)size, file) == (size_t)size) {
        fixture.size = (uint32_t)size;
    }
    fclose(file);
    return fixture;
}

static bool read_fixture(void *context, uint32_t offset, void *destination, uint32_t length)
{
    const Fixture *fixture = context;
    if (offset > fixture->size || length > fixture->size - offset) {
        return false;
    }
    memcpy(destination, fixture->bytes + offset, length);
    return true;
}

static bool open_file(void *context, const char *path, FlacInput *input)
{
    (void)context;
    free(opened.bytes);
    opened = load(path, "flac");
    *input = (FlacInput){.read = read_fixture, .context = &opened, .size = opened.size};
    return opened.size > 0;
}

static void close_file(void *context)
{
    (void)context;
}

static void start_engine_with(SpeedStage *speed_stage, SpeedSetting speed)
{
    flac_use_block_buffer(&decoder, &block);
    flac_source_init(&source,
                     (FlacFileOpener){.open = open_file, .close = close_file, .context = NULL},
                     &decoder);
    audio_engine_init(&engine, (AudioEngineConfig){.source = flac_source_interface(&source),
                                                   .ring = ring,
                                                   .ring_words = RING_WORDS,
                                                   .speed_stage = speed_stage});
    audio_engine_set_volume(&engine, FULL_VOLUME);
    audio_engine_set_speed(&engine, speed);
}

static void start_engine(void)
{
    start_engine_with(NULL, (SpeedSetting){0});
}

static void start_engine_at(SpeedSetting speed)
{
    start_engine_with(&stage, speed);
}

typedef struct ScriptedSource {
    int32_t left[16];
    int32_t right[16];
    int waits_before_frame;
    int frames_left;
    int releases;
    int closes;
    uint32_t rate;
    bool equalized;
    bool fail_at_end;
    int ends;
} ScriptedSource;

static ScriptedSource scripted;

static bool scripted_open(void *context, const char *path, uint32_t start_ms, AudioTrackInfo *track)
{
    (void)path;
    (void)start_ms;
    const ScriptedSource *source_state = context;
    uint32_t rate = source_state->rate == 0 ? 44100 : source_state->rate;
    *track = (AudioTrackInfo){
        .stream = {.sample_rate = rate, .channels = 2, .bits_per_sample = 16, .max_block = 16}};
    return true;
}

static AudioFrameResult scripted_next(void *context, AudioFrame *frame)
{
    ScriptedSource *source_state = context;
    if (source_state->waits_before_frame > 0) {
        source_state->waits_before_frame--;
        return AUDIO_FRAME_WAIT;
    }
    if (source_state->frames_left == 0) {
        source_state->ends++;
        return source_state->fail_at_end ? AUDIO_FRAME_ERROR : AUDIO_FRAME_END;
    }
    source_state->frames_left--;
    *frame = (AudioFrame){.left = source_state->left,
                          .right = source_state->right,
                          .first_sample = 0,
                          .samples = 16,
                          .bits = 16,
                          .equalized = source_state->equalized};
    return AUDIO_FRAME_READY;
}

static void scripted_release(void *context)
{
    ((ScriptedSource *)context)->releases++;
}

static void scripted_close(void *context)
{
    ((ScriptedSource *)context)->closes++;
}

static void start_scripted_engine_with(int waits, int frames, SpeedStage *speed_stage,
                                       SpeedSetting speed)
{
    scripted = (ScriptedSource){.waits_before_frame = waits, .frames_left = frames};
    for (int index = 0; index < 16; index++) {
        scripted.left[index] = index + 1;
        scripted.right[index] = -(index + 1);
    }
    AudioSource interface = {scripted_open, scripted_next, scripted_release, scripted_close,
                             &scripted};
    audio_engine_init(&engine, (AudioEngineConfig){.source = interface,
                                                   .ring = ring,
                                                   .ring_words = RING_WORDS,
                                                   .speed_stage = speed_stage});
    audio_engine_set_volume(&engine, FULL_VOLUME);
    audio_engine_set_speed(&engine, speed);
}

static void start_scripted_engine(int waits, int frames)
{
    start_scripted_engine_with(waits, frames, NULL, (SpeedSetting){0});
}

static void start_scripted_engine_at(int waits, int frames, SpeedSetting speed)
{
    start_scripted_engine_with(waits, frames, &stage, speed);
}

static int32_t expected_at(const Fixture *pcm, uint32_t index)
{
    int32_t value;
    memcpy(&value, pcm->bytes + 4 * index, sizeof value);
    return value;
}

static uint32_t word_of(int32_t sample, int bits)
{
    return (uint32_t)sample << (32 - bits);
}

static bool frame_is(uint32_t frame, int32_t left, int32_t right, int bits)
{
    uint32_t slot = frame % RING_FRAMES;
    return ring[2 * slot] == word_of(left, bits) && ring[2 * slot + 1] == word_of(right, bits);
}

static int32_t sixteen_bit_left(uint32_t frame)
{
    return (int32_t)ring[2 * (frame % RING_FRAMES)] / 65536;
}

static bool ring_holds(const char *name, uint32_t first_frame, uint32_t ring_start, uint32_t frames,
                       int bits)
{
    Fixture pcm = load(name, "pcm");
    bool ok = pcm.size > 0;
    for (uint32_t frame = 0; ok && frame < frames; frame++) {
        uint32_t index = 2 * (first_frame + frame);
        ok = frame_is(ring_start + frame, expected_at(&pcm, index), expected_at(&pcm, index + 1),
                      bits);
    }
    free(pcm.bytes);
    return ok;
}

static void pump_until_full(uint32_t consumed)
{
    for (int round = 0; round < 64; round++) {
        audio_engine_pump(&engine, consumed);
    }
}

static void pump_rounds(uint32_t consumed, int rounds)
{
    for (int round = 0; round < rounds; round++) {
        audio_engine_pump(&engine, consumed);
    }
}

static void playing_fills_the_ring_with_the_track(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    CHECK_EQUAL(44100u, audio_engine_rate(&engine));
    pump_until_full(0);
    CHECK(ring_holds("lpc_16_44k_stereo", 0, 0, 6000, 16));
    AudioEngineStatus status = audio_engine_status(&engine);
    CHECK_EQUAL(AUDIO_ENGINE_PLAYING, status.state);
    CHECK_EQUAL(7u, status.token);
    CHECK_EQUAL(0u, status.position_ms);
}

static void use_a_cut_at_one_kilohertz(void)
{
    EqualizerCurve curve = {.band_count = 1};
    curve.bands[0] = (EqualizerBand){.frequency_hz = 1000, .gain_tenths = -60, .q_hundredths = 100};
    audio_engine_set_equalizer(&engine, &curve);
}

static void the_equalizer_shapes_what_the_main_core_decodes(void)
{
    start_scripted_engine(0, 1);
    use_a_cut_at_one_kilohertz();
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    pump_until_full(0);
    bool changed = false;
    for (uint32_t frame = 0; frame < 16; frame++) {
        changed = changed || !frame_is(frame, scripted.left[frame], scripted.right[frame], 16);
    }
    CHECK(changed);
}

static void frames_core_1_already_equalized_pass_straight_through(void)
{
    start_scripted_engine(0, 1);
    scripted.equalized = true;
    use_a_cut_at_one_kilohertz();
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    pump_until_full(0);
    for (uint32_t frame = 0; frame < 16; frame++) {
        CHECK(frame_is(frame, scripted.left[frame], scripted.right[frame], 16));
    }
}

static void the_position_follows_what_the_dma_consumed(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    pump_until_full(0);
    audio_engine_pump(&engine, 4410);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
}

static void twenty_four_bit_audio_reaches_the_ring_exactly(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_24_96k_stereo", 0, 1));
    CHECK_EQUAL(96000u, audio_engine_rate(&engine));
    pump_until_full(0);
    CHECK(ring_holds("lpc_24_96k_stereo", 0, 0, 5000, 24));
}

static void the_end_of_a_track_without_a_next_one_stops_and_goes_silent(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    pump_until_full(0);
    CHECK(frame_is(3001 + 10, 0, 0, 16));
    audio_engine_pump(&engine, 3000);
    CHECK_EQUAL(AUDIO_ENGINE_PLAYING, audio_engine_status(&engine).state);
    audio_engine_pump(&engine, 3001);
    AudioEngineStatus status = audio_engine_status(&engine);
    CHECK_EQUAL(AUDIO_ENGINE_STOPPED, status.state);
    CHECK_EQUAL(3u, status.token);
    CHECK(!status.failed);
}

static void a_queued_track_follows_without_a_gap(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_16_44k_stereo", 4);
    pump_until_full(0);
    CHECK(ring_holds("odd_blocks_16", 0, 0, 3001, 16));
    CHECK(ring_holds("lpc_16_44k_stereo", 0, 3001, 4000, 16));
    audio_engine_pump(&engine, 3000);
    CHECK_EQUAL(3u, audio_engine_status(&engine).token);
    audio_engine_pump(&engine, 3001 + 441);
    AudioEngineStatus status = audio_engine_status(&engine);
    CHECK_EQUAL(4u, status.token);
    CHECK_EQUAL(10u, status.position_ms);
}

static void the_status_describes_the_playing_tracks_stream(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_24_96k_stereo", 0, 1));
    AudioTrackInfo track = audio_engine_status(&engine).track;
    CHECK_EQUAL(96000u, track.stream.sample_rate);
    CHECK_EQUAL(24, track.stream.bits_per_sample);
    CHECK_EQUAL(2, track.stream.channels);
    CHECK_EQUAL(5000u, track.stream.total_samples);
    CHECK_EQUAL(25502u, track.audio_bytes);
}

static void the_stream_description_follows_the_audio_into_the_next_track(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_16_44k_stereo", 4);
    pump_until_full(0);
    CHECK_EQUAL(9451u, audio_engine_status(&engine).track.audio_bytes);
    audio_engine_pump(&engine, 3001 + 441);
    CHECK_EQUAL(18719u, audio_engine_status(&engine).track.audio_bytes);
}

static void a_next_track_at_another_rate_waits_for_the_ring_to_drain(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_24_96k_stereo", 4);
    pump_until_full(0);
    uint32_t rate = 0;
    CHECK(!audio_engine_wants_rate(&engine, &rate));
    audio_engine_pump(&engine, 3001);
    CHECK(audio_engine_wants_rate(&engine, &rate));
    CHECK_EQUAL(96000u, rate);
    audio_engine_restart(&engine);
    pump_until_full(0);
    CHECK(ring_holds("lpc_24_96k_stereo", 0, 0, 5000, 24));
    CHECK_EQUAL(4u, audio_engine_status(&engine).token);
    CHECK_EQUAL(96000u, audio_engine_rate(&engine));
}

static void waiting_for_a_new_rate_plays_silence_not_old_audio(void)
{
    start_engine();
    memset(ring, 0x55, sizeof ring);
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_24_96k_stereo", 4);
    pump_until_full(0);
    for (uint32_t frame = 3001; frame < RING_FRAMES; frame++) {
        CHECK(frame_is(frame, 0, 0, 16));
    }
    audio_engine_pump(&engine, 6000);
    CHECK_EQUAL(0u, audio_engine_stats(&engine).underruns);
    for (uint32_t frame = 0; frame < 6000; frame++) {
        CHECK(frame_is(RING_FRAMES + frame, 0, 0, 16));
    }
    uint32_t rate = 0;
    CHECK(audio_engine_wants_rate(&engine, &rate));
}

static void a_lead_in_plays_silence_before_the_track(void)
{
    start_engine();
    memset(ring, 0x55, sizeof ring);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    audio_engine_lead_in(&engine, 100, 0);
    pump_until_full(0);
    for (uint32_t frame = 0; frame < 100; frame++) {
        CHECK(frame_is(frame, 0, 0, 16));
    }
    CHECK(ring_holds("lpc_16_44k_stereo", 0, 100, 4000, 16));
    audio_engine_pump(&engine, 50);
    CHECK_EQUAL(0u, audio_engine_status(&engine).position_ms);
    audio_engine_pump(&engine, 100 + 4410);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
    CHECK_EQUAL(7u, audio_engine_status(&engine).token);
}

static void a_lead_in_ramps_the_first_frames_up(void)
{
    start_scripted_engine(0, 4);
    for (int index = 0; index < 16; index++) {
        scripted.left[index] = 2000;
        scripted.right[index] = -2000;
    }
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    audio_engine_lead_in(&engine, 10, 8);
    pump_until_full(0);
    for (uint32_t step = 0; step < 8; step++) {
        CHECK(frame_is(10 + step, (int32_t)(250 * step), -(int32_t)(250 * step), 16));
    }
    for (uint32_t frame = 18; frame < 10 + 64; frame++) {
        CHECK(frame_is(frame, 2000, -2000, 16));
    }
}

static void a_lead_in_only_applies_before_anything_was_written(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    pump_until_full(0);
    audio_engine_lead_in(&engine, 100, 0);
    CHECK(ring_holds("lpc_16_44k_stereo", 0, 0, 1000, 16));
    audio_engine_pump(&engine, 4410);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
}

static void starting_part_way_skips_to_that_sample(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 100, 5));
    pump_until_full(0);
    CHECK(ring_holds("lpc_16_44k_stereo", 4410, 0, 1000, 16));
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
}

static void seeking_restarts_the_ring_at_the_new_place(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 5));
    pump_until_full(0);
    audio_engine_pump(&engine, 1000);
    CHECK(audio_engine_seek(&engine, 50));
    pump_until_full(0);
    CHECK(ring_holds("lpc_16_44k_stereo", 2205, 0, 1000, 16));
    CHECK_EQUAL(50u, audio_engine_status(&engine).position_ms);
    CHECK_EQUAL(5u, audio_engine_status(&engine).token);
}

static void seeking_after_the_next_track_opened_queues_it_again(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_16_44k_stereo", 4);
    pump_until_full(0);
    CHECK(audio_engine_seek(&engine, 20));
    pump_until_full(0);
    CHECK_EQUAL(3u, audio_engine_status(&engine).token);
    audio_engine_pump(&engine, 3001 - 882 + 441);
    CHECK_EQUAL(4u, audio_engine_status(&engine).token);
    CHECK_EQUAL(10u, audio_engine_status(&engine).position_ms);
}

static void a_seek_while_a_new_rate_waits_keeps_the_next_track(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_24_96k_stereo", 4);
    pump_until_full(0);
    CHECK(audio_engine_seek(&engine, 0));
    pump_until_full(0);
    audio_engine_pump(&engine, 3001);
    uint32_t rate = 0;
    CHECK(audio_engine_wants_rate(&engine, &rate));
    CHECK_EQUAL(96000u, rate);
}

static void closing_the_source_keeps_the_pause_and_the_position(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 5));
    pump_until_full(0);
    audio_engine_pump(&engine, 2205);
    audio_engine_pause(&engine);
    audio_engine_close_source(&engine);
    AudioEngineStatus status = audio_engine_status(&engine);
    CHECK_EQUAL(AUDIO_ENGINE_PAUSED, status.state);
    CHECK_EQUAL(50u, status.position_ms);
    CHECK_EQUAL(5u, status.token);
    CHECK(audio_engine_seek(&engine, status.position_ms));
    CHECK_EQUAL(AUDIO_ENGINE_PAUSED, audio_engine_status(&engine).state);
    pump_until_full(0);
    CHECK(ring_holds("lpc_16_44k_stereo", 2205, 0, 1000, 16));
}

static void volume_scales_the_samples(void)
{
    start_engine();
    audio_engine_set_volume(&engine, 0);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 1));
    pump_until_full(0);
    CHECK(frame_is(1000, 0, 0, 16));
    start_engine();
    audio_engine_set_volume(&engine, FULL_VOLUME - 6);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 1));
    pump_until_full(0);
    Fixture pcm = load("lpc_16_44k_stereo", "pcm");
    int32_t full = expected_at(&pcm, 2 * 1000);
    int32_t scaled = sixteen_bit_left(1000);
    int32_t error = scaled * 2 - full;
    int32_t allowed = (full < 0 ? -full : full) / 100 + 2;
    CHECK(error >= -allowed && error <= allowed);
    free(pcm.bytes);
}

static void mono_plays_on_both_channels(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "fixed_16_mono", 0, 1));
    pump_until_full(0);
    Fixture pcm = load("fixed_16_mono", "pcm");
    int32_t sample = expected_at(&pcm, 500);
    CHECK(frame_is(500, sample, sample, 16));
    free(pcm.bytes);
}

static bool scripted_rate_plays(uint32_t rate)
{
    start_scripted_engine(0, 1);
    scripted.rate = rate;
    return audio_engine_play(&engine, "scripted", 0, 1);
}

static void an_unsupported_rate_is_refused(void)
{
    CHECK(!scripted_rate_plays(7000));
    CHECK_EQUAL(AUDIO_ENGINE_STOPPED, audio_engine_status(&engine).state);
    AudioRefusal refusal = audio_engine_refusal(&engine);
    CHECK_EQUAL(AUDIO_REFUSAL_RATE, refusal.problem);
    CHECK_EQUAL(7000u, refusal.sample_rate);
}

static void a_queued_track_at_an_unsupported_rate_is_refused_with_its_rate(void)
{
    start_scripted_engine(0, 1);
    CHECK(audio_engine_play(&engine, "scripted", 0, 3));
    scripted.rate = 7000;
    scripted.frames_left = 1;
    audio_engine_queue_next(&engine, "scripted", 4);
    pump_until_full(0);
    CHECK(audio_engine_status(&engine).failed);
    CHECK_EQUAL(AUDIO_REFUSAL_RATE, audio_engine_refusal(&engine).problem);
    CHECK_EQUAL(7000u, audio_engine_refusal(&engine).sample_rate);
}

static void low_rates_play_at_a_rate_they_divide(void)
{
    static const uint32_t RATES[][2] = {{8000, 48000},  {11025, 44100}, {12000, 48000},
                                        {16000, 48000}, {22050, 44100}, {24000, 48000},
                                        {32000, 96000}};
    for (size_t index = 0; index < sizeof RATES / sizeof RATES[0]; index++) {
        CHECK(scripted_rate_plays(RATES[index][0]));
        CHECK_EQUAL(RATES[index][1], audio_engine_rate(&engine));
    }
}

static int32_t sample_24(const Fixture *pcm, uint32_t index)
{
    return index < 2 ? 0 : expected_at(pcm, index - 2) * 256;
}

static void a_22_khz_track_fills_in_every_other_sample(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "independent_16", 0, 1));
    CHECK_EQUAL(44100u, audio_engine_rate(&engine));
    pump_until_full(0);
    Fixture pcm = load("independent_16", "pcm");
    bool ok = pcm.size > 0;
    for (uint32_t frame = 0; ok && frame < 2000; frame++) {
        for (uint32_t channel = 0; channel < 2; channel++) {
            uint32_t index = 2 * frame + channel;
            int32_t before = sample_24(&pcm, index);
            int32_t now = expected_at(&pcm, index) * 256;
            int32_t between = before + (now - before) / 2;
            ok = ok && ring[2 * (2 * frame) + channel] == (uint32_t)between << 8 &&
                 ring[2 * (2 * frame + 1) + channel] == (uint32_t)now << 8;
        }
    }
    CHECK(ok);
    free(pcm.bytes);
    audio_engine_observe(&engine, 44100);
    CHECK_EQUAL(1000u, audio_engine_status(&engine).position_ms);
    CHECK_EQUAL(22050u, audio_engine_status(&engine).track.stream.sample_rate);
}

static void rates_up_to_192_khz_play_and_higher_ones_are_refused(void)
{
    CHECK(scripted_rate_plays(192000));
    CHECK_EQUAL(192000u, audio_engine_rate(&engine));
    CHECK(scripted_rate_plays(176400));
    CHECK_EQUAL(176400u, audio_engine_rate(&engine));
    CHECK(scripted_rate_plays(96000));
    CHECK(!scripted_rate_plays(384000));
    CHECK_EQUAL(AUDIO_REFUSAL_RATE, audio_engine_refusal(&engine).problem);
    CHECK_EQUAL(384000u, audio_engine_refusal(&engine).sample_rate);
}

static void a_missing_file_is_refused(void)
{
    start_engine();
    CHECK(!audio_engine_play(&engine, "no_such_file", 0, 1));
    CHECK_EQUAL(AUDIO_REFUSAL_UNREADABLE, audio_engine_refusal(&engine).problem);
}

static void the_ring_never_runs_ahead_of_the_dma_by_more_than_its_size(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "constant_and_noise", 0, 1));
    pump_until_full(0);
    CHECK(audio_engine_buffered(&engine, 0) <= RING_FRAMES);
    CHECK(audio_engine_buffered(&engine, 0) >= RING_FRAMES - 4608);
}

static void observing_the_dma_position_never_decodes(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    pump_until_full(0);
    uint32_t produced = audio_engine_buffered(&engine, 0);
    audio_engine_observe(&engine, 4410);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
    CHECK_EQUAL(produced - 4410, audio_engine_buffered(&engine, 4410));
    audio_engine_pump(&engine, 4410);
    CHECK(audio_engine_buffered(&engine, 4410) > produced - 4410);
}

static void the_dma_overtaking_the_decoder_counts_an_underrun(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "constant_and_noise", 0, 7));
    pump_until_full(0);
    CHECK_EQUAL(0u, audio_engine_stats(&engine).underruns);
    audio_engine_pump(&engine, RING_FRAMES + 100);
    CHECK_EQUAL(1u, audio_engine_stats(&engine).underruns);
    CHECK(audio_engine_buffered(&engine, RING_FRAMES + 100) <= RING_FRAMES);
    audio_engine_pump(&engine, RING_FRAMES + 200);
    CHECK_EQUAL(1u, audio_engine_stats(&engine).underruns);
}

static void what_the_dma_played_is_silenced_so_a_late_lap_plays_nothing_old(void)
{
    start_scripted_engine(0, 1);
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    audio_engine_pump(&engine, 0);
    CHECK(frame_is(0, 1, -1, 16));
    audio_engine_observe(&engine, 8);
    CHECK(frame_is(0, 0, 0, 16));
    CHECK(frame_is(7, 0, 0, 16));
    CHECK(frame_is(8, 9, -9, 16));
    audio_engine_observe(&engine, 3 * RING_FRAMES);
    CHECK(frame_is(8, 0, 0, 16));
    CHECK(frame_is(RING_FRAMES - 1, 0, 0, 16));
}

static void every_decoded_frame_is_counted(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    CHECK_EQUAL(0u, audio_engine_stats(&engine).frames);
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(1u, audio_engine_stats(&engine).frames);
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(2u, audio_engine_stats(&engine).frames);
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(2u, audio_engine_stats(&engine).frames);
}

static void the_current_path_is_the_track_being_played(void)
{
    start_engine();
    CHECK(audio_engine_current_path(&engine) == NULL);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 1));
    CHECK(strcmp("lpc_16_44k_stereo", audio_engine_current_path(&engine)) == 0);
}

static void a_source_that_is_not_ready_leaves_the_ring_alone(void)
{
    start_scripted_engine(2, 1);
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(0u, audio_engine_buffered(&engine, 0));
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(0u, audio_engine_buffered(&engine, 0));
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(16u, audio_engine_buffered(&engine, 0));
    CHECK(frame_is(0, 1, -1, 16));
    CHECK(frame_is(15, 16, -16, 16));
}

static void a_frame_is_released_once_its_samples_are_in_the_ring(void)
{
    start_scripted_engine(0, 2);
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(1, scripted.releases);
    audio_engine_pump(&engine, 0);
    CHECK_EQUAL(2, scripted.releases);
    audio_engine_stop(&engine);
    CHECK_EQUAL(1, scripted.closes);
}

enum {
    STRETCH_LEAD_OUT = 1320,
    PUMP_ROUNDS = 700
};

static const SpeedSetting UNITY_KEEP = {4, true};
static const SpeedSetting UNITY_FOLLOW = {4, false};
static const SpeedSetting DOUBLE_KEEP = {8, true};
static const SpeedSetting DOUBLE_FOLLOW = {8, false};
static const SpeedSetting THREE_FOLLOW = {3, false};
static const SpeedSetting SIX_KEEP = {6, true};

static void a_stage_at_unity_speed_leaves_the_ring_bit_identical(void)
{
    const SpeedSetting settings[] = {UNITY_KEEP, UNITY_FOLLOW};
    for (int index = 0; index < 2; index++) {
        start_engine_at(settings[index]);
        CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
        pump_until_full(0);
        CHECK(ring_holds("lpc_16_44k_stereo", 0, 0, 6000, 16));
        audio_engine_observe(&engine, 4410);
        CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
    }
}

static void the_position_is_track_time_at_double_speed(void)
{
    const SpeedSetting settings[] = {DOUBLE_KEEP, DOUBLE_FOLLOW};
    for (int index = 0; index < 2; index++) {
        start_engine_at(settings[index]);
        CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
        pump_rounds(0, PUMP_ROUNDS);
        audio_engine_observe(&engine, 4410);
        CHECK_EQUAL(200u, audio_engine_status(&engine).position_ms);
    }
}

static void the_position_is_track_time_at_three_quarter_speed(void)
{
    start_engine_at(THREE_FOLLOW);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_observe(&engine, 4410);
    CHECK_EQUAL(75u, audio_engine_status(&engine).position_ms);
}

static void a_lead_in_does_not_count_as_track_time_at_speed(void)
{
    start_engine_at(SIX_KEEP);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    audio_engine_lead_in(&engine, 100, 0);
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_observe(&engine, 100 + 4410);
    CHECK_EQUAL(150u, audio_engine_status(&engine).position_ms);
}

static void starting_part_way_at_speed_counts_from_the_start_position(void)
{
    start_engine_at(DOUBLE_FOLLOW);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 100, 5));
    pump_rounds(0, PUMP_ROUNDS);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
    audio_engine_observe(&engine, 441);
    CHECK_EQUAL(120u, audio_engine_status(&engine).position_ms);
}

static void a_queued_track_at_the_same_speed_follows_without_a_gap(void)
{
    start_engine_at(DOUBLE_FOLLOW);
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_16_44k_stereo", 4);
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_observe(&engine, 1500);
    CHECK_EQUAL(3u, audio_engine_status(&engine).token);
    audio_engine_observe(&engine, 1501 + 441);
    AudioEngineStatus status = audio_engine_status(&engine);
    CHECK_EQUAL(4u, status.token);
    CHECK_EQUAL(20u, status.position_ms);
}

static uint32_t first_frame_of_token(uint32_t token)
{
    for (uint32_t frame = 0; frame < RING_FRAMES; frame++) {
        audio_engine_observe(&engine, frame);
        if (audio_engine_status(&engine).token == token) {
            return frame;
        }
    }
    return 0;
}

static void a_stretched_track_hands_over_after_its_tail(void)
{
    start_engine_at(SIX_KEEP);
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_16_44k_stereo", 4);
    pump_rounds(0, PUMP_ROUNDS);
    uint32_t boundary = first_frame_of_token(4);
    CHECK(boundary >= 3001 * 4 / 6);
    CHECK(boundary <= 3001 * 4 / 6 + 1 + STRETCH_LEAD_OUT);
    audio_engine_observe(&engine, boundary + 441);
    CHECK_EQUAL(15u, audio_engine_status(&engine).position_ms);
}

static void the_end_of_a_stretched_track_plays_its_tail_before_stopping(void)
{
    start_engine_at(SIX_KEEP);
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    pump_rounds(0, PUMP_ROUNDS);
    CHECK(engine.ended);
    CHECK(engine.end_at >= 3001 * 4 / 6);
    CHECK(engine.end_at <= 3001 * 4 / 6 + 1 + STRETCH_LEAD_OUT);
    audio_engine_observe(&engine, engine.end_at - 1);
    CHECK_EQUAL(AUDIO_ENGINE_PLAYING, audio_engine_status(&engine).state);
    audio_engine_observe(&engine, engine.end_at);
    CHECK_EQUAL(AUDIO_ENGINE_STOPPED, audio_engine_status(&engine).state);
}

static void seeking_at_speed_resets_the_stage(void)
{
    start_engine_at(SIX_KEEP);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 5));
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_pump(&engine, 1000);
    CHECK(audio_engine_seek(&engine, 50));
    pump_rounds(0, PUMP_ROUNDS);
    CHECK(ring_holds("lpc_16_44k_stereo", 2205, 0, STRETCH_LEAD_OUT, 16));
    CHECK_EQUAL(50u, audio_engine_status(&engine).position_ms);
}

static void a_speed_set_while_playing_waits_for_the_next_start(void)
{
    start_engine_at(UNITY_KEEP);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 5));
    pump_until_full(0);
    audio_engine_set_speed(&engine, DOUBLE_FOLLOW);
    audio_engine_observe(&engine, 4410);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
    CHECK(audio_engine_seek(&engine, 0));
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_observe(&engine, 4410);
    CHECK_EQUAL(200u, audio_engine_status(&engine).position_ms);
}

static void a_next_track_at_another_rate_waits_for_the_stretched_tail(void)
{
    start_engine_at(SIX_KEEP);
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_24_96k_stereo", 4);
    pump_rounds(0, PUMP_ROUNDS);
    uint32_t rate = 0;
    audio_engine_observe(&engine, 3001 * 4 / 6 - 1);
    CHECK(!audio_engine_wants_rate(&engine, &rate));
    CHECK(engine.rate_end <= 3001 * 4 / 6 + 1 + STRETCH_LEAD_OUT);
    audio_engine_observe(&engine, engine.rate_end);
    CHECK(audio_engine_wants_rate(&engine, &rate));
    CHECK_EQUAL(96000u, rate);
    audio_engine_restart(&engine);
    CHECK_EQUAL(SPEED_RESAMPLE, speed_method(&stage));
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_observe(&engine, 9600);
    CHECK_EQUAL(150u, audio_engine_status(&engine).position_ms);
    CHECK_EQUAL(4u, audio_engine_status(&engine).token);
}

static void a_22_khz_track_at_speed_is_sped_up_before_it_is_doubled(void)
{
    start_engine_at(DOUBLE_FOLLOW);
    CHECK(audio_engine_play(&engine, "independent_16", 0, 1));
    CHECK_EQUAL(44100u, audio_engine_rate(&engine));
    pump_rounds(0, PUMP_ROUNDS);
    audio_engine_observe(&engine, 44100);
    CHECK_EQUAL(2000u, audio_engine_status(&engine).position_ms);
}

static void without_a_stage_the_speed_is_ignored(void)
{
    start_engine_with(NULL, DOUBLE_FOLLOW);
    CHECK(audio_engine_play(&engine, "lpc_16_44k_stereo", 0, 7));
    pump_until_full(0);
    CHECK(ring_holds("lpc_16_44k_stereo", 0, 0, 6000, 16));
    audio_engine_observe(&engine, 4410);
    CHECK_EQUAL(100u, audio_engine_status(&engine).position_ms);
}

static void double_speed_keeps_the_ring_fed(void)
{
    start_scripted_engine_at(0, 4000, DOUBLE_KEEP);
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    pump_rounds(0, 400);
    uint32_t consumed = 0;
    for (int round = 0; round < 20; round++) {
        consumed += 1024;
        pump_rounds(consumed, 140);
    }
    CHECK_EQUAL(0u, audio_engine_stats(&engine).underruns);
    CHECK(audio_engine_buffered(&engine, consumed) > 0);
    uint32_t position = audio_engine_status(&engine).position_ms;
    CHECK(position >= 2 * consumed * 1000 / 44100 - 1 &&
          position <= 2 * consumed * 1000 / 44100 + 1);
}

static void an_error_while_speeding_leaves_the_stage_unused(void)
{
    start_scripted_engine_at(0, 300, SIX_KEEP);
    scripted.fail_at_end = true;
    memset(ring, 0x55, sizeof ring);
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    pump_rounds(0, PUMP_ROUNDS);
    CHECK(audio_engine_status(&engine).failed);
    CHECK(!engine.speeding);
    CHECK(!engine.draining);
    CHECK(engine.ended);
    uint32_t end = engine.end_at;
    CHECK(end > 0);
    pump_rounds(0, 50);
    CHECK_EQUAL(end, engine.end_at);
    for (uint32_t frame = end; frame < end + 600; frame++) {
        CHECK(frame_is(frame, 0, 0, 16));
    }
}

static void a_stage_never_decodes_past_the_end_of_the_source(void)
{
    start_scripted_engine_at(0, 160, SIX_KEEP);
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    pump_rounds(0, PUMP_ROUNDS);
    CHECK_EQUAL(1, scripted.closes);
    CHECK_EQUAL(160u, audio_engine_stats(&engine).frames);
    CHECK_EQUAL(1, scripted.ends);
}

enum {
    EQUALIZED_SOURCE_FRAMES = 40,
    EQUALIZED_CHECK_FRAMES = 320
};

static int32_t reference_ring_frames[2 * EQUALIZED_CHECK_FRAMES];

static void reference_through_stage(bool equalize)
{
    static SpeedStage reference_stage;
    static int32_t input[2 * 16 * EQUALIZED_SOURCE_FRAMES];
    EqualizerCurve curve = {.band_count = 1};
    curve.bands[0] = (EqualizerBand){.frequency_hz = 1000, .gain_tenths = -60, .q_hundredths = 100};
    Equalizer reference_eq;
    equalizer_init(&reference_eq);
    equalizer_set_rate(&reference_eq, 44100);
    equalizer_set_curve(&reference_eq, &curve);
    for (uint32_t frame = 0; frame < 16 * EQUALIZED_SOURCE_FRAMES; frame++) {
        input[2 * frame] = (int32_t)(frame % 16 + 1) * 256 * 100;
        input[2 * frame + 1] = -(int32_t)(frame % 16 + 1) * 256 * 100;
    }
    speed_configure(&reference_stage, DOUBLE_FOLLOW, 44100);
    uint32_t made = 0;
    uint32_t pushed = 0;
    while (made < EQUALIZED_CHECK_FRAMES) {
        if (pushed < 16 * EQUALIZED_SOURCE_FRAMES) {
            pushed += speed_push(&reference_stage, input + 2 * pushed, 32);
        } else {
            speed_finish(&reference_stage);
        }
        uint32_t got = speed_pull(&reference_stage, reference_ring_frames + 2 * made, 32);
        if (equalize) {
            equalizer_process(&reference_eq, reference_ring_frames + 2 * made, got);
        }
        made += got;
    }
}

static uint32_t equalizer_test_ring_differs_from(bool equalize)
{
    uint32_t differing = 0;
    reference_through_stage(equalize);
    for (uint32_t frame = 0; frame < EQUALIZED_CHECK_FRAMES; frame++) {
        uint32_t slot = 2 * (frame % RING_FRAMES);
        differing += ring[slot] != (uint32_t)reference_ring_frames[2 * frame] << 8;
        differing += ring[slot + 1] != (uint32_t)reference_ring_frames[2 * frame + 1] << 8;
    }
    return differing;
}

static void run_unequalized_scripted_at_double_speed(bool already_equalized)
{
    start_scripted_engine_at(0, EQUALIZED_SOURCE_FRAMES, DOUBLE_FOLLOW);
    scripted.equalized = already_equalized;
    for (int index = 0; index < 16; index++) {
        scripted.left[index] = (index + 1) * 100;
        scripted.right[index] = -(index + 1) * 100;
    }
    use_a_cut_at_one_kilohertz();
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    pump_rounds(0, PUMP_ROUNDS);
}

static void a_ramp_at_speed_rises_over_output_frames(void)
{
    start_scripted_engine_at(0, EQUALIZED_SOURCE_FRAMES, DOUBLE_FOLLOW);
    for (int index = 0; index < 16; index++) {
        scripted.left[index] = (index + 1) * 100;
        scripted.right[index] = -(index + 1) * 100;
    }
    CHECK(audio_engine_play(&engine, "scripted", 0, 1));
    audio_engine_lead_in(&engine, 10, 8);
    pump_rounds(0, PUMP_ROUNDS);
    reference_through_stage(false);
    for (uint32_t frame = 0; frame < 40; frame++) {
        for (uint32_t channel = 0; channel < 2; channel++) {
            int32_t plain = reference_ring_frames[2 * frame + channel];
            int32_t expected =
                frame < 8 ? (int32_t)(((int64_t)plain * (int64_t)frame) >> 3) : plain;
            CHECK_EQUAL((uint32_t)expected << 8, ring[2 * (10 + frame) + channel]);
        }
    }
}

static void the_equalizer_shapes_the_output_of_the_speed_stage(void)
{
    run_unequalized_scripted_at_double_speed(false);
    CHECK_EQUAL(0u, equalizer_test_ring_differs_from(true));
    CHECK(equalizer_test_ring_differs_from(false) > 0);
}

static void frames_core_1_equalized_are_not_equalized_again_after_the_stage(void)
{
    run_unequalized_scripted_at_double_speed(true);
    CHECK_EQUAL(0u, equalizer_test_ring_differs_from(false));
    CHECK(equalizer_test_ring_differs_from(true) > 0);
}

static void queue_after_opening_the_next_track(const char *path, uint32_t token)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "lpc_16_44k_stereo", 4);
    pump_until_full(0);
    audio_engine_queue_next(&engine, path, token);
}

static void a_cancelled_next_track_stays_cancelled_after_a_seek(void)
{
    queue_after_opening_the_next_track(NULL, 0);
    CHECK(audio_engine_seek(&engine, 20));
    CHECK(!audio_engine_next_pending(&engine));
    pump_until_full(0);
    audio_engine_observe(&engine, 3001 - 882 + 441);
    CHECK_EQUAL(3u, audio_engine_status(&engine).token);
    audio_engine_observe(&engine, 3001 - 882);
    CHECK_EQUAL(AUDIO_ENGINE_STOPPED, audio_engine_status(&engine).state);
}

static void a_newer_next_track_wins_over_the_opened_one_after_a_seek(void)
{
    queue_after_opening_the_next_track("lpc_24_96k_stereo", 9);
    CHECK(audio_engine_seek(&engine, 20));
    CHECK(audio_engine_next_pending(&engine));
    pump_until_full(0);
    uint32_t rate = 0;
    audio_engine_observe(&engine, 3001 - 882);
    CHECK(audio_engine_wants_rate(&engine, &rate));
    audio_engine_restart(&engine);
    CHECK_EQUAL(9u, audio_engine_status(&engine).token);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_current_path_is_the_track_being_played),
        TEST_CASE(a_source_that_is_not_ready_leaves_the_ring_alone),
        TEST_CASE(a_frame_is_released_once_its_samples_are_in_the_ring),
        TEST_CASE(observing_the_dma_position_never_decodes),
        TEST_CASE(the_dma_overtaking_the_decoder_counts_an_underrun),
        TEST_CASE(what_the_dma_played_is_silenced_so_a_late_lap_plays_nothing_old),
        TEST_CASE(every_decoded_frame_is_counted),
        TEST_CASE(playing_fills_the_ring_with_the_track),
        TEST_CASE(the_equalizer_shapes_what_the_main_core_decodes),
        TEST_CASE(frames_core_1_already_equalized_pass_straight_through),
        TEST_CASE(the_position_follows_what_the_dma_consumed),
        TEST_CASE(twenty_four_bit_audio_reaches_the_ring_exactly),
        TEST_CASE(the_end_of_a_track_without_a_next_one_stops_and_goes_silent),
        TEST_CASE(a_queued_track_follows_without_a_gap),
        TEST_CASE(the_status_describes_the_playing_tracks_stream),
        TEST_CASE(the_stream_description_follows_the_audio_into_the_next_track),
        TEST_CASE(a_next_track_at_another_rate_waits_for_the_ring_to_drain),
        TEST_CASE(waiting_for_a_new_rate_plays_silence_not_old_audio),
        TEST_CASE(a_lead_in_plays_silence_before_the_track),
        TEST_CASE(a_lead_in_ramps_the_first_frames_up),
        TEST_CASE(a_lead_in_only_applies_before_anything_was_written),
        TEST_CASE(starting_part_way_skips_to_that_sample),
        TEST_CASE(seeking_restarts_the_ring_at_the_new_place),
        TEST_CASE(seeking_after_the_next_track_opened_queues_it_again),
        TEST_CASE(a_seek_while_a_new_rate_waits_keeps_the_next_track),
        TEST_CASE(closing_the_source_keeps_the_pause_and_the_position),
        TEST_CASE(volume_scales_the_samples),
        TEST_CASE(mono_plays_on_both_channels),
        TEST_CASE(low_rates_play_at_a_rate_they_divide),
        TEST_CASE(a_22_khz_track_fills_in_every_other_sample),
        TEST_CASE(an_unsupported_rate_is_refused),
        TEST_CASE(rates_up_to_192_khz_play_and_higher_ones_are_refused),
        TEST_CASE(a_queued_track_at_an_unsupported_rate_is_refused_with_its_rate),
        TEST_CASE(a_missing_file_is_refused),
        TEST_CASE(the_ring_never_runs_ahead_of_the_dma_by_more_than_its_size),
        TEST_CASE(a_cancelled_next_track_stays_cancelled_after_a_seek),
        TEST_CASE(a_newer_next_track_wins_over_the_opened_one_after_a_seek),
        TEST_CASE(a_stage_at_unity_speed_leaves_the_ring_bit_identical),
        TEST_CASE(the_position_is_track_time_at_double_speed),
        TEST_CASE(the_position_is_track_time_at_three_quarter_speed),
        TEST_CASE(a_lead_in_does_not_count_as_track_time_at_speed),
        TEST_CASE(starting_part_way_at_speed_counts_from_the_start_position),
        TEST_CASE(a_queued_track_at_the_same_speed_follows_without_a_gap),
        TEST_CASE(a_stretched_track_hands_over_after_its_tail),
        TEST_CASE(the_end_of_a_stretched_track_plays_its_tail_before_stopping),
        TEST_CASE(seeking_at_speed_resets_the_stage),
        TEST_CASE(a_speed_set_while_playing_waits_for_the_next_start),
        TEST_CASE(a_ramp_at_speed_rises_over_output_frames),
        TEST_CASE(a_next_track_at_another_rate_waits_for_the_stretched_tail),
        TEST_CASE(a_22_khz_track_at_speed_is_sped_up_before_it_is_doubled),
        TEST_CASE(without_a_stage_the_speed_is_ignored),
        TEST_CASE(double_speed_keeps_the_ring_fed),
        TEST_CASE(an_error_while_speeding_leaves_the_stage_unused),
        TEST_CASE(a_stage_never_decodes_past_the_end_of_the_source),
        TEST_CASE(the_equalizer_shapes_the_output_of_the_speed_stage),
        TEST_CASE(frames_core_1_equalized_are_not_equalized_again_after_the_stage),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
