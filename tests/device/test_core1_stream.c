#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/core1_mp3.h"
#include "device/core1_stream.h"
#include "device/mp3_probe.h"

enum {
    BIG_RING_BYTES = 64 * 1024,
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
static uint8_t ring[BIG_RING_BYTES];
static FlacDecoder core0_decoder;
static FlacDecoder core1_decoder;
static Core1Flac core1_flac;
static Mp3Decoder mp3_decoder;
static Mp3Decoder reference_decoder;
static Core1Mp3 core1_mp3;
static Core1Source source;
static Core1Stream stream;
static AudioSource interface;
static Fixture opened;
static int close_count;

static Fixture load(const char *name, const char *extension)
{
    char path[512];
    if (strncmp(name, "mp3/", 4) == 0) {
        snprintf(path, sizeof path, "%s/%s.mp3", ECHO_FIXTURE_DIR, name);
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

static bool streams_exactly(const char *name, int channels, uint32_t ring_bytes)
{
    start(ring_bytes);
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
                                  uint32_t capacity)
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
    uint32_t expected = reference_samples(name, control.start_offset, mp3_reference, 120000);
    bool ok = info.codec == AUDIO_CODEC_MP3 && expected > 0;
    uint32_t next_sample = 0;
    AudioFrame frame;
    AudioFrameResult result = AUDIO_FRAME_READY;
    while (ok && (result = next_frame(&frame)) == AUDIO_FRAME_READY) {
        ok = frame.first_sample == next_sample && frame.bits == 24;
        for (uint32_t index = 0; ok && index < frame.samples; index++) {
            ok = frame.left[index] == mp3_reference[next_sample + index][0] &&
                 frame.right[index] == mp3_reference[next_sample + index][1];
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
}

static void an_mp3_describes_its_stream(void)
{
    start_with(BIG_RING_BYTES, true);
    AudioTrackInfo info;
    CHECK(interface.open(interface.context, "mp3/vbr_48k_mono", 0, &info));
    CHECK_EQUAL(48000u, info.stream.sample_rate);
    CHECK_EQUAL(1, info.stream.channels);
    CHECK_EQUAL(0, info.stream.bits_per_sample);
    CHECK_EQUAL(85u * 1152u, info.stream.total_samples);
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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_24_bit_96k_track_streams_through_core_1_exactly),
        TEST_CASE(a_16_bit_track_streams_through_core_1_exactly),
        TEST_CASE(a_mono_track_plays_its_channel_on_both_sides),
        TEST_CASE(a_ring_smaller_than_the_track_wraps_and_stays_exact),
        TEST_CASE(core_1_waits_while_its_slots_are_full),
        TEST_CASE(reopening_part_way_drops_the_old_frames),
        TEST_CASE(closing_idles_core_1_and_closes_the_file),
        TEST_CASE(a_missing_track_does_not_start_core_1),
        TEST_CASE(an_mp3_streams_through_core_1_like_its_decoder),
        TEST_CASE(an_mp3_describes_its_stream),
        TEST_CASE(an_mp3_opened_part_way_starts_counting_from_there),
        TEST_CASE(a_file_that_is_not_mp3_is_refused),
    };
    int failures = test_run_all(cases, TEST_CASE_COUNT(cases));
    free(opened.bytes);
    return failures;
}
