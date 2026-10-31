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
static FlacSource source;
static uint32_t ring[RING_WORDS];
static AudioEngine engine;
static Fixture opened;

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

static void start_engine(void)
{
    flac_source_init(&source,
                     (FlacFileOpener){.open = open_file, .close = close_file, .context = NULL},
                     &decoder);
    audio_engine_init(&engine, (AudioEngineConfig){.source = flac_source_interface(&source),
                                                   .ring = ring,
                                                   .ring_words = RING_WORDS});
    audio_engine_set_volume(&engine, FULL_VOLUME);
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
        return AUDIO_FRAME_END;
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

static void start_scripted_engine(int waits, int frames)
{
    scripted = (ScriptedSource){.waits_before_frame = waits, .frames_left = frames};
    for (int index = 0; index < 16; index++) {
        scripted.left[index] = index + 1;
        scripted.right[index] = -(index + 1);
    }
    AudioSource interface = {scripted_open, scripted_next, scripted_release, scripted_close,
                             &scripted};
    audio_engine_init(
        &engine, (AudioEngineConfig){.source = interface, .ring = ring, .ring_words = RING_WORDS});
    audio_engine_set_volume(&engine, FULL_VOLUME);
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
    curve.bands[0] = (EqualizerBand){.frequency_hz = 1000, .gain_db = -6, .q_tenths = 10};
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

static void an_unsupported_rate_is_refused(void)
{
    start_engine();
    CHECK(!audio_engine_play(&engine, "independent_16", 0, 1));
    CHECK_EQUAL(AUDIO_ENGINE_STOPPED, audio_engine_status(&engine).state);
    AudioRefusal refusal = audio_engine_refusal(&engine);
    CHECK_EQUAL(AUDIO_REFUSAL_RATE, refusal.problem);
    CHECK_EQUAL(22050u, refusal.sample_rate);
}

static void a_queued_track_at_an_unsupported_rate_is_refused_with_its_rate(void)
{
    start_engine();
    CHECK(audio_engine_play(&engine, "odd_blocks_16", 0, 3));
    audio_engine_queue_next(&engine, "independent_16", 4);
    pump_until_full(0);
    audio_engine_pump(&engine, 3001);
    CHECK(audio_engine_status(&engine).failed);
    CHECK_EQUAL(AUDIO_REFUSAL_RATE, audio_engine_refusal(&engine).problem);
}

static bool scripted_rate_plays(uint32_t rate)
{
    start_scripted_engine(0, 1);
    scripted.rate = rate;
    return audio_engine_play(&engine, "scripted", 0, 1);
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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_current_path_is_the_track_being_played),
        TEST_CASE(a_source_that_is_not_ready_leaves_the_ring_alone),
        TEST_CASE(a_frame_is_released_once_its_samples_are_in_the_ring),
        TEST_CASE(observing_the_dma_position_never_decodes),
        TEST_CASE(the_dma_overtaking_the_decoder_counts_an_underrun),
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
        TEST_CASE(starting_part_way_skips_to_that_sample),
        TEST_CASE(seeking_restarts_the_ring_at_the_new_place),
        TEST_CASE(volume_scales_the_samples),
        TEST_CASE(mono_plays_on_both_channels),
        TEST_CASE(an_unsupported_rate_is_refused),
        TEST_CASE(rates_up_to_192_khz_play_and_higher_ones_are_refused),
        TEST_CASE(a_queued_track_at_an_unsupported_rate_is_refused_with_its_rate),
        TEST_CASE(a_missing_file_is_refused),
        TEST_CASE(the_ring_never_runs_ahead_of_the_dma_by_more_than_its_size),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
