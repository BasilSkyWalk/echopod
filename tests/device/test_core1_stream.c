#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/core1_aac.h"
#include "device/core1_alac.h"
#include "device/core1_control.h"
#include "device/core1_mp3.h"
#include "device/core1_opus.h"
#include "device/core1_stream.h"
#include "device/mp3_probe.h"

enum {
    BIG_RING_BYTES = 64 * 1024,
    FULL_RING_BYTES = CORE1_STREAM_RING_BYTES,
    SMALL_RING_BYTES = 24 * 1024,
    FILL_BUDGET = 4096,
    MAX_ROUNDS = 100000
};

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
} Fixture;

static StreamControl control;
static int32_t samples[STREAM_SLOTS][FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK];
static uint8_t ring[FULL_RING_BYTES];
static FlacDecoder core0_decoder;
static FlacDecoder core1_decoder;
static Core1Flac core1_flac;
static Mp3Decoder mp3_decoder;
static Mp3Decoder reference_decoder;
static Core1Mp3 core1_mp3;
static AlacDecoder alac_decoder;
static Core1Alac core1_alac;
static Core1Aac core1_aac;
static Core1Opus core1_opus;
static uint8_t opus_state[32 * 1024] __attribute__((aligned(8)));
static uint8_t opus_packet[CORE1_OPUS_PACKET_BYTES];
static int16_t opus_output[CORE1_OPUS_MAX_FRAME * FLAC_MAX_CHANNELS];
static Core1Source source;
static Core1Stream stream;
static AudioSource interface;
static Fixture opened;
static int close_count;

static Fixture load(const char *name, const char *extension)
{
    char path[512];
    if (strncmp(name, "opus/", 5) == 0) {
        const char *suffix = strcmp(extension, "flac") == 0 ? "opus" : extension;
        snprintf(path, sizeof path, "%s/%s.%s", ECHO_FIXTURE_DIR, name, suffix);
    } else if (strncmp(name, "mp3/", 4) == 0) {
        snprintf(path, sizeof path, "%s/%s.mp3", ECHO_FIXTURE_DIR, name);
    } else if (strncmp(name, "alac/", 5) == 0 || strncmp(name, "tags/", 5) == 0 ||
               strncmp(name, "aac/", 4) == 0) {
        const char *suffix = strcmp(extension, "flac") == 0 ? "m4a" : extension;
        snprintf(path, sizeof path, "%s/%s.%s", ECHO_FIXTURE_DIR, name, suffix);
    } else {
        snprintf(path, sizeof path, "%s/flac/%s.%s", ECHO_FIXTURE_DIR, name, extension);
    }
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
    close_count++;
}

static void start_with(uint32_t ring_bytes, bool mp3)
{
    memset(&control, 0, sizeof control);
    memset(ring, 0, sizeof ring);
    close_count = 0;
    StreamRegion region = {
        .control = &control, .samples = samples, .ring = ring, .ring_bytes = ring_bytes};
    core1_source_init(&source, region,
                      (FlacFileOpener){.open = open_file, .close = close_file, .context = NULL},
                      &core0_decoder, FILL_BUDGET);
    if (mp3) {
        core1_stream_init(&stream, region, core1_mp3_decoder(&core1_mp3, &mp3_decoder));
        interface = core1_source_mp3_interface(&source);
    } else {
        core1_stream_init(&stream, region, core1_flac_decoder(&core1_flac, &core1_decoder));
        interface = core1_source_interface(&source);
    }
}

static void start(uint32_t ring_bytes)
{
    start_with(ring_bytes, false);
}

static void start_mp4(uint32_t ring_bytes)
{
    start_with(ring_bytes, false);
    core1_stream_set_decoder(&stream, STREAM_CODEC_ALAC,
                             core1_alac_decoder(&core1_alac, &alac_decoder));
    interface = core1_source_mp4_interface(&source);
}

static void start_aac(uint32_t ring_bytes)
{
    start_mp4(ring_bytes);
    core1_aac_close(&core1_aac);
    core1_stream_set_decoder(&stream, STREAM_CODEC_AAC, core1_aac_decoder(&core1_aac, NULL));
}

static AudioFrameResult next_frame(AudioFrame *frame)
{
    for (int round = 0; round < MAX_ROUNDS; round++) {
        AudioFrameResult result = interface.next(interface.context, frame);
        if (result != AUDIO_FRAME_WAIT) {
            return result;
        }
        core1_stream_step(&stream);
    }
    return AUDIO_FRAME_WAIT;
}

static int32_t expected_at(const Fixture *pcm, uint32_t index)
{
    int32_t value;
    memcpy(&value, pcm->bytes + 4 * index, sizeof value);
    return value;
}

static bool frame_matches(const Fixture *pcm, const AudioFrame *frame, int channels)
{
    for (uint32_t sample = 0; sample < frame->samples; sample++) {
        uint32_t index = (frame->first_sample + sample) * (uint32_t)channels;
        if (4 * (index + (uint32_t)channels) > pcm->size) {
            return false;
        }
        if (frame->left[sample] != expected_at(pcm, index)) {
            return false;
        }
        int32_t right = channels > 1 ? expected_at(pcm, index + 1) : expected_at(pcm, index);
        if (frame->right[sample] != right) {
            return false;
        }
    }
    return true;
}

static bool plays_exactly(const char *name, int channels)
{
    Fixture pcm = load(name, "pcm");
    AudioTrackInfo info;
    bool ok = pcm.size > 0 && interface.open(interface.context, name, 0, &info);
    uint32_t next_sample = 0;
    AudioFrame frame;
    AudioFrameResult result = AUDIO_FRAME_READY;
    while (ok && (result = next_frame(&frame)) == AUDIO_FRAME_READY) {
        ok = frame.first_sample == next_sample && frame_matches(&pcm, &frame, channels) &&
             frame.bits == info.stream.bits_per_sample;
        next_sample += frame.samples;
        interface.release(interface.context);
    }
    ok = ok && result == AUDIO_FRAME_END && next_sample == info.stream.total_samples;
    interface.close(interface.context);
    free(pcm.bytes);
    return ok;
}

static Equalizer core1_equalizer;
static Core1EqualizerShare share;

static EqualizerCurve two_bands(int8_t low_gain, int8_t high_gain)
{
    EqualizerCurve curve = {.band_count = 2};
    curve.bands[0] = (EqualizerBand){.frequency_hz = 160, .gain_db = low_gain, .q_tenths = 10};
    curve.bands[1] = (EqualizerBand){.frequency_hz = 2500, .gain_db = high_gain, .q_tenths = 10};
    return curve;
}

static void share_curve(EqualizerCurve curve, bool enabled)
{
    share.curve = curve;
    share.enabled = enabled ? 1u : 0u;
    share.version++;
}

static bool equalized_frames_match(Fixture *pcm, Equalizer *reference, const AudioFrame *frame)
{
    bool ok = frame->equalized && frame->bits == 24;
    for (uint32_t sample = 0; ok && sample < frame->samples; sample++) {
        uint32_t index = 2 * (frame->first_sample + sample);
        int32_t pair[2] = {expected_at(pcm, index) * 256, expected_at(pcm, index + 1) * 256};
        equalizer_process(reference, pair, 1);
        ok = frame->left[sample] == pair[0] && frame->right[sample] == pair[1];
    }
    return ok;
}

static void core_1_equalizes_exactly_like_the_main_core(void)
{
    start(BIG_RING_BYTES);
    core1_stream_set_equalizer(&stream, &core1_equalizer, &share);
    share_curve(two_bands(6, -4), true);
    Equalizer reference;
    equalizer_init(&reference);
    equalizer_set_rate(&reference, 44100);
    EqualizerCurve curve = two_bands(6, -4);
    equalizer_set_curve(&reference, &curve);
    Fixture pcm = load("lpc_16_44k_stereo", "pcm");
    AudioTrackInfo info;
    bool ok = pcm.size > 0 && interface.open(interface.context, "lpc_16_44k_stereo", 0, &info);
    AudioFrame frame;
    int frames = 0;
    while (ok && next_frame(&frame) == AUDIO_FRAME_READY) {
        ok = equalized_frames_match(&pcm, &reference, &frame);
        interface.release(interface.context);
        frames++;
    }
    interface.close(interface.context);
    free(pcm.bytes);
    CHECK(ok);
    CHECK(frames > 1);
}

static void core_1_leaves_the_audio_alone_unless_asked(void)
{
    start(BIG_RING_BYTES);
    core1_stream_set_equalizer(&stream, &core1_equalizer, &share);
    share_curve(two_bands(6, -4), false);
    CHECK(plays_exactly("lpc_16_44k_stereo", 2));
    start(BIG_RING_BYTES);
    core1_stream_set_equalizer(&stream, &core1_equalizer, &share);
    share_curve(two_bands(0, 0), true);
    CHECK(plays_exactly("lpc_16_44k_stereo", 2));
}

static bool streams_exactly(const char *name, int channels, uint32_t ring_bytes)
{
    start(ring_bytes);
    return plays_exactly(name, channels);
}

static bool alac_streams_exactly(const char *name, int channels, uint32_t ring_bytes)
{
    start_mp4(ring_bytes);
    return plays_exactly(name, channels);
}

static void a_24_bit_96k_track_streams_through_core_1_exactly(void)
{
    CHECK(streams_exactly("lpc_24_96k_stereo", 2, BIG_RING_BYTES));
}

static void a_16_bit_track_streams_through_core_1_exactly(void)
{
    CHECK(streams_exactly("lpc_16_44k_stereo", 2, BIG_RING_BYTES));
}

static void a_mono_track_plays_its_channel_on_both_sides(void)
{
    CHECK(streams_exactly("fixed_16_mono", 1, BIG_RING_BYTES));
}

static void a_ring_smaller_than_the_track_wraps_and_stays_exact(void)
{
    CHECK(streams_exactly("lpc_24_96k_stereo", 2, SMALL_RING_BYTES));
    CHECK(control.released > SMALL_RING_BYTES);
}

static void core_1_waits_while_its_slots_are_full(void)
{
    start(BIG_RING_BYTES);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "fixed_16_mono", 0, &info));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    for (int round = 0; round < 20; round++) {
        interface.next(interface.context, &frame);
        core1_stream_step(&stream);
    }
    CHECK_EQUAL(STREAM_SLOT_FULL, control.slots[0].state);
    CHECK_EQUAL(STREAM_SLOT_FULL, control.slots[1].state);
    CHECK_EQUAL(1152u, frame.samples);
    interface.release(interface.context);
    CHECK_EQUAL(STREAM_SLOT_EMPTY, control.slots[0].state);
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(1152u, frame.first_sample);
    interface.close(interface.context);
}

static void reopening_part_way_drops_the_old_frames(void)
{
    start(BIG_RING_BYTES);
    Fixture pcm = load("fixed_16_mono", "pcm");
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "fixed_16_mono", 0, &info));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    core1_stream_step(&stream);
    core1_stream_step(&stream);
    CHECK(interface.open(interface.context, "fixed_16_mono", 100, &info));
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    uint32_t target = 100u * 44100u / 1000u;
    CHECK(frame.first_sample <= target && target < frame.first_sample + frame.samples);
    CHECK(frame_matches(&pcm, &frame, 1));
    interface.release(interface.context);
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK(frame_matches(&pcm, &frame, 1));
    interface.close(interface.context);
    free(pcm.bytes);
}

static void closing_idles_core_1_and_closes_the_file(void)
{
    start(BIG_RING_BYTES);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "fixed_16_mono", 0, &info));
    interface.close(interface.context);
    CHECK_EQUAL(0u, control.generation);
    CHECK_EQUAL(1, close_count);
    CHECK(!core1_stream_step(&stream));
}

static void a_missing_track_does_not_start_core_1(void)
{
    start(BIG_RING_BYTES);
    AudioTrackInfo info;
    CHECK(!interface.open(interface.context, "missing", 0, &info));
    CHECK_EQUAL(0u, control.generation);
}

static uint32_t reference_samples(const char *name, uint32_t start_offset, int32_t (*out)[2],
                                  uint32_t capacity, Mp3Stream *probed)
{
    Fixture mp3 = load(name, "mp3");
    FlacInput whole = {.read = read_fixture, .context = &mp3, .size = mp3.size};
    Mp3Stream probe;
    if (!mp3_probe(whole, &probe)) {
        free(mp3.bytes);
        return 0;
    }
    Fixture audio = {mp3.bytes + start_offset, probe.audio_end - start_offset};
    mp3_decoder_start(&reference_decoder,
                      (FlacInput){.read = read_fixture, .context = &audio, .size = audio.size});
    uint32_t count = 0;
    while (mp3_decode(&reference_decoder) == MP3_DECODED) {
        int channels = mp3_decoder_channels(&reference_decoder);
        for (uint32_t index = 0; index < mp3_decoder_samples(&reference_decoder); index++) {
            if (count < capacity) {
                out[count][0] = mp3_decoder_channel(&reference_decoder, 0)[index];
                out[count][1] =
                    mp3_decoder_channel(&reference_decoder, channels > 1 ? 1 : 0)[index];
            }
            count++;
        }
    }
    free(mp3.bytes);
    *probed = probe;
    return count;
}

static int32_t mp3_reference[120000][2];

static bool mp3_streams_like_the_decoder(const char *name, uint32_t ring_bytes)
{
    start_with(ring_bytes, true);
    AudioTrackInfo info;
    if (!interface.open(interface.context, name, 0, &info)) {
        return false;
    }
    Mp3Stream probe;
    uint32_t decoded = reference_samples(name, control.start_offset, mp3_reference, 120000, &probe);
    uint32_t lead = probe.lead_trim;
    uint32_t expected = decoded - lead - probe.tail_trim;
    bool ok = info.codec == AUDIO_CODEC_MP3 && decoded > 0 && info.stream.total_samples == expected;
    uint32_t next_sample = 0;
    AudioFrame frame;
    AudioFrameResult result = AUDIO_FRAME_READY;
    while (ok && (result = next_frame(&frame)) == AUDIO_FRAME_READY) {
        ok = frame.first_sample == next_sample && frame.bits == 24 && frame.samples > 0;
        for (uint32_t index = 0; ok && index < frame.samples; index++) {
            ok = frame.left[index] == mp3_reference[lead + next_sample + index][0] &&
                 frame.right[index] == mp3_reference[lead + next_sample + index][1];
        }
        next_sample += frame.samples;
        interface.release(interface.context);
    }
    ok = ok && result == AUDIO_FRAME_END && next_sample == expected;
    interface.close(interface.context);
    return ok;
}

static void an_mp3_streams_through_core_1_like_its_decoder(void)
{
    CHECK(mp3_streams_like_the_decoder("mp3/cbr_128_44k_stereo_tagged", BIG_RING_BYTES));
    CHECK(mp3_streams_like_the_decoder("mp3/vbr_48k_mono", SMALL_RING_BYTES));
    CHECK(mp3_streams_like_the_decoder("mp3/cbr_192_48k_stereo_bare", SMALL_RING_BYTES));
    CHECK(mp3_streams_like_the_decoder("mp3/mpeg2_22k_stereo", SMALL_RING_BYTES));
}

static uint32_t stream_left(const char *name, uint32_t *rate)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    if (!interface.open(interface.context, name, 0, &info)) {
        return 0;
    }
    *rate = info.stream.sample_rate;
    uint32_t count = 0;
    AudioFrame frame;
    while (next_frame(&frame) == AUDIO_FRAME_READY) {
        for (uint32_t index = 0; index < frame.samples && count < 120000; index++) {
            mp3_reference[count++][0] = frame.left[index];
        }
        interface.release(interface.context);
    }
    interface.close(interface.context);
    return count;
}

static int best_sine_lag(uint32_t count, uint32_t rate)
{
    static const double TONE_HZ = 440.0;
    static const double TWO_PI = 6.283185307179586;
    int reach = (int)(rate / TONE_HZ / 2.0) - 2;
    int best = 0;
    double best_score = -1.0e300;
    for (int lag = -reach; lag <= reach; lag++) {
        double score = 0.0;
        for (uint32_t index = count / 10; index < count - count / 10; index++) {
            double phase = TWO_PI * TONE_HZ * ((double)index + lag) / rate;
            score += mp3_reference[index][0] * sin(phase);
        }
        if (score > best_score) {
            best_score = score;
            best = lag;
        }
    }
    return best;
}

static bool lines_up_with_its_source(const char *name, uint32_t expected)
{
    uint32_t rate = 0;
    uint32_t count = stream_left(name, &rate);
    int lag = best_sine_lag(count, rate);
    return count == expected && lag >= -1 && lag <= 1;
}

static void a_trimmed_mp3_lines_up_with_the_sound_it_was_made_from(void)
{
    CHECK(lines_up_with_its_source("mp3/cbr_128_44k_stereo_tagged", 66150));
    CHECK(lines_up_with_its_source("mp3/vbr_48k_mono", 96000));
    CHECK(lines_up_with_its_source("mp3/mpeg2_22k_stereo", 22050));
}

static void a_trimmed_mp3_starts_at_its_first_real_sample(void)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "mp3/cbr_128_44k_stereo_tagged", 0, &info));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(0u, frame.first_sample);
    CHECK_EQUAL(1152u - 576u - MP3_DECODER_DELAY, frame.samples);
    interface.release(interface.context);
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(1152u - 576u - MP3_DECODER_DELAY, frame.first_sample);
    CHECK_EQUAL(1152u, frame.samples);
    interface.close(interface.context);
}

static void a_trimmed_mp3_opened_part_way_counts_real_samples(void)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "mp3/cbr_128_44k_stereo_tagged", 1000, &info));
    CHECK(control.start_offset > 481u);
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(44100u, frame.first_sample);
    interface.close(interface.context);
}

static void an_mp3_describes_its_stream(void)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "mp3/vbr_48k_mono", 0, &info));
    CHECK_EQUAL(48000u, info.stream.sample_rate);
    CHECK_EQUAL(1, info.stream.channels);
    CHECK_EQUAL(0, info.stream.bits_per_sample);
    CHECK_EQUAL(96000u, info.stream.total_samples);
    CHECK_EQUAL(9504u - 192u, info.audio_bytes);
    interface.close(interface.context);
    CHECK_EQUAL(1, close_count);
}

static void an_mp3_opened_part_way_starts_counting_from_there(void)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "mp3/cbr_192_48k_stereo_bare", 500, &info));
    CHECK_EQUAL(12000u, control.start_offset);
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(24000u, frame.first_sample);
    interface.close(interface.context);
}

static void a_file_that_is_not_mp3_is_refused(void)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    CHECK(!interface.open(interface.context, "fixed_16_mono", 0, &info));
    CHECK_EQUAL(0u, control.generation);
    CHECK_EQUAL(1, close_count);
}

static void an_alac_track_streams_through_core_1_exactly(void)
{
    CHECK(alac_streams_exactly("alac/stereo_24_96k", 2, BIG_RING_BYTES));
    CHECK(alac_streams_exactly("alac/stereo_16_44k", 2, SMALL_RING_BYTES));
    CHECK(control.released > SMALL_RING_BYTES);
    CHECK(alac_streams_exactly("alac/noise_16", 2, SMALL_RING_BYTES));
    CHECK(alac_streams_exactly("alac/mono_16_22k", 1, SMALL_RING_BYTES));
}

static void an_alac_track_describes_its_stream(void)
{
    start_mp4(BIG_RING_BYTES);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "alac/stereo_24_96k", 0, &info));
    CHECK_EQUAL(AUDIO_CODEC_ALAC, info.codec);
    CHECK_EQUAL(96000u, info.stream.sample_rate);
    CHECK_EQUAL(24, info.stream.bits_per_sample);
    CHECK_EQUAL(2, info.stream.channels);
    CHECK_EQUAL(9000u, info.stream.total_samples);
    CHECK(info.audio_bytes > 20000 && info.audio_bytes < opened.size);
    CHECK_EQUAL(STREAM_CODEC_ALAC, control.codec);
    interface.close(interface.context);
}

static void an_alac_track_opened_part_way_starts_on_its_frame(void)
{
    start_mp4(SMALL_RING_BYTES);
    Fixture pcm = load("alac/stereo_16_44k", "pcm");
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "alac/stereo_16_44k", 150, &info));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(4096u, frame.first_sample);
    CHECK(frame_matches(&pcm, &frame, 2));
    interface.release(interface.context);
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    CHECK_EQUAL(8192u, frame.first_sample);
    CHECK_EQUAL(10000u - 8192u, frame.samples);
    CHECK(frame_matches(&pcm, &frame, 2));
    interface.release(interface.context);
    CHECK_EQUAL(AUDIO_FRAME_END, next_frame(&frame));
    interface.close(interface.context);
    free(pcm.bytes);
}

static void flac_and_alac_tracks_take_turns_on_one_program(void)
{
    start_mp4(BIG_RING_BYTES);
    AudioSource mp4 = interface;
    interface = core1_source_interface(&source);
    AudioTrackInfo info;
    AudioFrame frame;
    CHECK(interface.open(interface.context, "lpc_16_44k_stereo", 0, &info));
    CHECK_EQUAL(AUDIO_FRAME_READY, next_frame(&frame));
    interface = mp4;
    CHECK(plays_exactly("alac/mono_24_48k", 1));
    interface = core1_source_interface(&source);
    CHECK(plays_exactly("lpc_16_44k_stereo", 2));
}

static void a_program_without_the_codec_reports_an_error(void)
{
    start_mp4(BIG_RING_BYTES);
    AudioTrackInfo info;
    AudioFrame frame;
    CHECK(interface.open(interface.context, "tags/aac", 0, &info));
    CHECK_EQUAL(AUDIO_CODEC_AAC, info.codec);
    CHECK_EQUAL(STREAM_CODEC_AAC, control.codec);
    CHECK_EQUAL(AUDIO_FRAME_ERROR, next_frame(&frame));
    interface.close(interface.context);
}

static void start_opus(uint32_t ring_bytes)
{
    start_with(ring_bytes, false);
    Core1OpusMemory memory = {opus_state, sizeof opus_state, opus_packet, opus_output};
    core1_stream_set_decoder(&stream, STREAM_CODEC_OPUS, core1_opus_decoder(&core1_opus, memory));
    interface = core1_source_ogg_interface(&source);
}

static bool opus_close(const Fixture *pcm, const AudioFrame *frame, int channels, int tolerance)
{
    for (uint32_t sample = 0; sample < frame->samples; sample++) {
        uint32_t index = (frame->first_sample + sample) * (uint32_t)channels;
        if (2 * (index + (uint32_t)channels) > pcm->size) {
            return false;
        }
        int16_t left = 0;
        int16_t right = 0;
        memcpy(&left, pcm->bytes + 2 * index, sizeof left);
        memcpy(&right, pcm->bytes + 2 * (index + (uint32_t)(channels - 1)), sizeof right);
        if (abs(frame->left[sample] - left) > tolerance ||
            abs(frame->right[sample] - right) > tolerance) {
            return false;
        }
    }
    return true;
}

static bool opus_plays_closely(const char *name, int channels, uint32_t ring_bytes,
                               uint32_t start_ms, uint32_t *first)
{
    start_opus(ring_bytes);
    Fixture pcm = load(name, "pcm");
    AudioTrackInfo info;
    bool ok = pcm.size > 0 && interface.open(interface.context, name, start_ms, &info) &&
              info.codec == AUDIO_CODEC_OPUS && control.codec == STREAM_CODEC_OPUS;
    uint32_t next_sample = UINT32_MAX;
    AudioFrame frame;
    AudioFrameResult result = AUDIO_FRAME_READY;
    while (ok && (result = next_frame(&frame)) == AUDIO_FRAME_READY) {
        if (next_sample == UINT32_MAX) {
            *first = frame.first_sample;
            next_sample = frame.first_sample;
        }
        ok = frame.first_sample == next_sample && frame.bits == 16 &&
             opus_close(&pcm, &frame, channels, start_ms == 0 ? 16 : 96);
        next_sample += frame.samples;
        interface.release(interface.context);
    }
    ok = ok && result == AUDIO_FRAME_END && next_sample == info.stream.total_samples &&
         info.stream.total_samples == pcm.size / 2 / (uint32_t)channels;
    interface.close(interface.context);
    free(pcm.bytes);
    return ok;
}

static void an_opus_track_streams_through_core_1_like_libopus_decodes_it(void)
{
    uint32_t first = 0;
    CHECK(opus_plays_closely("opus/stereo_small_pages", 2, BIG_RING_BYTES, 0, &first));
    CHECK_EQUAL(0u, first);
    CHECK(opus_plays_closely("opus/mono_60ms_speech", 1, BIG_RING_BYTES, 0, &first));
    CHECK(opus_plays_closely("opus/stereo_low_rate", 2, BIG_RING_BYTES, 0, &first));
    CHECK(opus_plays_closely("opus/stereo_high_rate", 2, BIG_RING_BYTES, 0, &first));
    CHECK(opus_plays_closely("opus/stereo_small_pages", 2, 4096, 0, &first));
    CHECK(control.released > 4096);
}

static void an_opus_track_opened_part_way_starts_on_the_sample(void)
{
    uint32_t first = 0;
    CHECK(opus_plays_closely("opus/stereo_small_pages", 2, BIG_RING_BYTES, 250, &first));
    CHECK_EQUAL(12000u, first);
    CHECK(opus_plays_closely("opus/mono_60ms_speech", 1, SMALL_RING_BYTES, 100, &first));
    CHECK_EQUAL(4800u, first);
}

static void opus_leaves_the_top_of_the_ring_to_its_decoder(void)
{
    uint32_t first = 0;
    CHECK(opus_plays_closely("opus/stereo_high_rate", 2, FULL_RING_BYTES, 0, &first));
    CHECK_EQUAL(CORE1_OPUS_RING_BYTES, control.ring_bytes);
    CHECK_EQUAL(CORE1_OPUS_RING_BYTES, stream.region.ring_bytes);
    interface = core1_source_interface(&source);
    CHECK(plays_exactly("lpc_16_44k_stereo", 2));
    CHECK_EQUAL(FULL_RING_BYTES, control.ring_bytes);
    CHECK_EQUAL(FULL_RING_BYTES, stream.region.ring_bytes);
}

static void an_opus_track_describes_its_stream(void)
{
    start_opus(BIG_RING_BYTES);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "opus/stereo_small_pages", 0, &info));
    CHECK_EQUAL(AUDIO_CODEC_OPUS, info.codec);
    CHECK_EQUAL(48000u, info.stream.sample_rate);
    CHECK_EQUAL(0, info.stream.bits_per_sample);
    CHECK_EQUAL(2, info.stream.channels);
    CHECK_EQUAL(19200u, info.stream.total_samples);
    CHECK(info.audio_bytes > 5000 && info.audio_bytes < opened.size);
    interface.close(interface.context);
    CHECK(core1_opus_decoder_bytes() <= sizeof opus_state);
}

static bool close_to_reference(const Fixture *pcm, const AudioFrame *frame, int channels)
{
    for (uint32_t sample = 0; sample < frame->samples; sample++) {
        uint32_t index = (frame->first_sample + sample) * (uint32_t)channels;
        if (2 * (index + (uint32_t)channels) > pcm->size) {
            return false;
        }
        int16_t left = 0;
        int16_t right = 0;
        memcpy(&left, pcm->bytes + 2 * index, sizeof left);
        memcpy(&right, pcm->bytes + 2 * (index + (uint32_t)(channels - 1)), sizeof right);
        if (abs(frame->left[sample] / 256 - left) > 2 ||
            abs(frame->right[sample] / 256 - right) > 2) {
            return false;
        }
    }
    return true;
}

static bool aac_plays_closely(const char *name, int channels, uint32_t ring_bytes,
                              uint32_t start_ms, uint32_t *first)
{
    start_aac(ring_bytes);
    Fixture pcm = load(name, "pcm");
    AudioTrackInfo info;
    bool ok = pcm.size > 0 && interface.open(interface.context, name, start_ms, &info) &&
              info.codec == AUDIO_CODEC_AAC && control.codec == STREAM_CODEC_AAC;
    uint32_t target = (uint32_t)((uint64_t)start_ms * info.stream.sample_rate / 1000);
    uint32_t next_sample = UINT32_MAX;
    AudioFrame frame;
    AudioFrameResult result = AUDIO_FRAME_READY;
    while (ok && (result = next_frame(&frame)) == AUDIO_FRAME_READY) {
        if (next_sample == UINT32_MAX) {
            *first = frame.first_sample;
            ok = frame.first_sample <= target && target < frame.first_sample + frame.samples;
            next_sample = frame.first_sample;
        }
        ok = ok && frame.first_sample == next_sample && frame.bits == 24 &&
             close_to_reference(&pcm, &frame, channels);
        next_sample += frame.samples;
        interface.release(interface.context);
    }
    ok = ok && result == AUDIO_FRAME_END && next_sample == info.stream.total_samples;
    interface.close(interface.context);
    free(pcm.bytes);
    return ok;
}

static void an_aac_track_streams_through_core_1_like_ffmpeg_decodes_it(void)
{
    uint32_t first = 0;
    CHECK(aac_plays_closely("aac/stereo_44k", 2, BIG_RING_BYTES, 0, &first));
    CHECK_EQUAL(0u, first);
    CHECK(aac_plays_closely("aac/mono_48k", 1, BIG_RING_BYTES, 0, &first));
    CHECK(aac_plays_closely("aac/stereo_96k_faststart", 2, 1024, 0, &first));
    CHECK(control.released > 1024);
}

static void an_aac_track_opened_part_way_starts_on_the_frame_with_the_target(void)
{
    uint32_t first = 0;
    CHECK(aac_plays_closely("aac/stereo_44k", 2, BIG_RING_BYTES, 100, &first));
    CHECK_EQUAL(4096u, first);
}

static void an_aac_track_describes_its_stream(void)
{
    start_aac(BIG_RING_BYTES);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "aac/stereo_44k", 0, &info));
    CHECK_EQUAL(AUDIO_CODEC_AAC, info.codec);
    CHECK_EQUAL(44100u, info.stream.sample_rate);
    CHECK_EQUAL(0, info.stream.bits_per_sample);
    CHECK_EQUAL(12000u, info.stream.total_samples);
    CHECK_EQUAL(1024, info.stream.max_block);
    CHECK(info.audio_bytes > 3000 && info.audio_bytes < opened.size);
    interface.close(interface.context);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_24_bit_96k_track_streams_through_core_1_exactly),
        TEST_CASE(a_16_bit_track_streams_through_core_1_exactly),
        TEST_CASE(core_1_equalizes_exactly_like_the_main_core),
        TEST_CASE(core_1_leaves_the_audio_alone_unless_asked),
        TEST_CASE(a_mono_track_plays_its_channel_on_both_sides),
        TEST_CASE(a_ring_smaller_than_the_track_wraps_and_stays_exact),
        TEST_CASE(core_1_waits_while_its_slots_are_full),
        TEST_CASE(reopening_part_way_drops_the_old_frames),
        TEST_CASE(closing_idles_core_1_and_closes_the_file),
        TEST_CASE(a_missing_track_does_not_start_core_1),
        TEST_CASE(an_mp3_streams_through_core_1_like_its_decoder),
        TEST_CASE(a_trimmed_mp3_lines_up_with_the_sound_it_was_made_from),
        TEST_CASE(a_trimmed_mp3_starts_at_its_first_real_sample),
        TEST_CASE(a_trimmed_mp3_opened_part_way_counts_real_samples),
        TEST_CASE(an_mp3_describes_its_stream),
        TEST_CASE(an_mp3_opened_part_way_starts_counting_from_there),
        TEST_CASE(a_file_that_is_not_mp3_is_refused),
        TEST_CASE(an_alac_track_streams_through_core_1_exactly),
        TEST_CASE(an_alac_track_describes_its_stream),
        TEST_CASE(an_alac_track_opened_part_way_starts_on_its_frame),
        TEST_CASE(flac_and_alac_tracks_take_turns_on_one_program),
        TEST_CASE(a_program_without_the_codec_reports_an_error),
        TEST_CASE(an_aac_track_streams_through_core_1_like_ffmpeg_decodes_it),
        TEST_CASE(an_aac_track_opened_part_way_starts_on_the_frame_with_the_target),
        TEST_CASE(an_aac_track_describes_its_stream),
        TEST_CASE(an_opus_track_streams_through_core_1_like_libopus_decodes_it),
        TEST_CASE(an_opus_track_opened_part_way_starts_on_the_sample),
        TEST_CASE(an_opus_track_describes_its_stream),
        TEST_CASE(opus_leaves_the_top_of_the_ring_to_its_decoder),
    };
    int failures = test_run_all(cases, TEST_CASE_COUNT(cases));
    core1_aac_close(&core1_aac);
    free(opened.bytes);
    return failures;
}
