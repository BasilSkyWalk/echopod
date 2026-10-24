#include <string.h>

#include "check.h"
#include "device/wav_source.h"

enum {
    FILE_CAPACITY = 65536,
    PCM_TAG = 1,
    FLOAT_TAG = 3,
    EXTENSIBLE_TAG = 0xFFFE
};

typedef struct Builder {
    uint8_t bytes[FILE_CAPACITY];
    uint32_t size;
} Builder;

static Builder file;
static WavBuffers buffers;
static WavSource source;
static int closes;

static void put_bytes(const void *data, uint32_t length)
{
    memcpy(file.bytes + file.size, data, length);
    file.size += length;
}

static void put_u16(uint32_t value)
{
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    put_bytes(bytes, 2);
}

static void put_u32(uint32_t value)
{
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16),
                        (uint8_t)(value >> 24)};
    put_bytes(bytes, 4);
}

static void put_sample(int32_t value, int bits)
{
    if (bits == 8) {
        uint8_t byte = (uint8_t)(value + 128);
        put_bytes(&byte, 1);
        return;
    }
    for (int shift = 0; shift < bits; shift += 8) {
        uint8_t byte = (uint8_t)((uint32_t)value >> shift);
        put_bytes(&byte, 1);
    }
}

static void put_fmt(uint32_t tag, int channels, uint32_t rate, int bits)
{
    uint32_t align = (uint32_t)(channels * bits / 8);
    put_bytes("fmt ", 4);
    put_u32(tag == EXTENSIBLE_TAG ? 40 : 16);
    put_u16(tag);
    put_u16((uint32_t)channels);
    put_u32(rate);
    put_u32(rate * align);
    put_u16(align);
    put_u16((uint32_t)bits);
    if (tag == EXTENSIBLE_TAG) {
        static const uint8_t PCM_GUID_TAIL[14] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
                                                  0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
        put_u16(22);
        put_u16((uint32_t)bits);
        put_u32(channels == 1 ? 4u : 3u);
        put_u16(PCM_TAG);
        put_bytes(PCM_GUID_TAIL, sizeof PCM_GUID_TAIL);
    }
}

static int32_t sample_at(uint32_t frame, int channel, int bits)
{
    uint32_t span = bits == 8 ? 128u : bits == 16 ? 32768u : 8388608u;
    uint32_t mix = frame * 2654435761u + (uint32_t)channel * 40503u;
    return (int32_t)(mix % (2u * span)) - (int32_t)span;
}

static void build(uint32_t tag, int channels, uint32_t rate, int bits, uint32_t frames,
                  bool extra_chunk)
{
    file.size = 0;
    put_bytes("RIFF", 4);
    put_u32(0);
    put_bytes("WAVE", 4);
    if (extra_chunk) {
        put_bytes("LIST", 4);
        put_u32(3);
        put_bytes("abc", 3);
        put_bytes("\0", 1);
    }
    put_fmt(tag, channels, rate, bits);
    put_bytes("data", 4);
    put_u32(frames * (uint32_t)(channels * bits / 8));
    for (uint32_t frame = 0; frame < frames; frame++) {
        for (int channel = 0; channel < channels; channel++) {
            put_sample(sample_at(frame, channel, bits), bits);
        }
    }
    uint32_t riff = file.size - 8;
    memcpy(file.bytes + 4, &riff, 4);
}

static bool read_file(void *context, uint32_t offset, void *destination, uint32_t length)
{
    (void)context;
    if (offset > file.size || length > file.size - offset) {
        return false;
    }
    memcpy(destination, file.bytes + offset, length);
    return true;
}

static bool open_file(void *context, const char *path, FlacInput *input)
{
    (void)context;
    (void)path;
    *input = (FlacInput){.read = read_file, .context = NULL, .size = file.size};
    return file.size > 0;
}

static void close_file(void *context)
{
    (void)context;
    closes++;
}

static AudioSource fresh_source(void)
{
    closes = 0;
    wav_source_init(&source,
                    (FlacFileOpener){.open = open_file, .close = close_file, .context = NULL},
                    &buffers);
    return wav_source_interface(&source);
}

static bool frames_match(AudioFrame frame, int channels, int bits)
{
    for (uint32_t index = 0; index < frame.samples; index++) {
        uint32_t at = frame.first_sample + index;
        int32_t left = sample_at(at, 0, bits);
        int32_t right = channels == 1 ? left : sample_at(at, 1, bits);
        if (frame.left[index] != left || frame.right[index] != right) {
            return false;
        }
    }
    return true;
}

static bool plays_through(AudioSource audio, int channels, int bits, uint32_t frames)
{
    AudioFrame frame;
    uint32_t expected = 0;
    while (audio.next(audio.context, &frame) == AUDIO_FRAME_READY) {
        if (frame.first_sample != expected || frame.bits != bits ||
            !frames_match(frame, channels, bits)) {
            return false;
        }
        expected += frame.samples;
        audio.release(audio.context);
    }
    return expected == frames;
}

static void sixteen_bit_stereo_plays_every_frame(void)
{
    build(PCM_TAG, 2, 44100, 16, 5000, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    CHECK_EQUAL(AUDIO_CODEC_WAV, track.codec);
    CHECK_EQUAL(44100u, track.stream.sample_rate);
    CHECK_EQUAL(16, track.stream.bits_per_sample);
    CHECK_EQUAL(2, track.stream.channels);
    CHECK_EQUAL(5000u, track.stream.total_samples);
    CHECK_EQUAL(20000u, track.audio_bytes);
    CHECK(plays_through(audio, 2, 16, 5000));
}

static void blocks_are_as_long_as_the_buffers_allow(void)
{
    build(PCM_TAG, 2, 48000, 16, 5000, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, audio.next(audio.context, &frame));
    CHECK_EQUAL((uint32_t)WAV_SOURCE_BLOCK_FRAMES, frame.samples);
}

static void twenty_four_bit_samples_keep_their_sign(void)
{
    build(PCM_TAG, 2, 96000, 24, 3000, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    CHECK_EQUAL(24, track.stream.bits_per_sample);
    CHECK(plays_through(audio, 2, 24, 3000));
}

static void mono_plays_on_both_sides(void)
{
    build(PCM_TAG, 1, 48000, 16, 1000, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    CHECK_EQUAL(1, track.stream.channels);
    CHECK(plays_through(audio, 1, 16, 1000));
}

static void eight_bit_samples_are_centred_on_zero(void)
{
    build(PCM_TAG, 2, 44100, 8, 600, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    CHECK(plays_through(audio, 2, 8, 600));
}

static void the_extensible_header_and_other_chunks_are_understood(void)
{
    build(EXTENSIBLE_TAG, 2, 88200, 24, 700, true);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    CHECK_EQUAL(88200u, track.stream.sample_rate);
    CHECK(plays_through(audio, 2, 24, 700));
}

static void starting_part_way_begins_at_that_frame(void)
{
    build(PCM_TAG, 2, 44100, 16, 9000, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 100, &track));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_READY, audio.next(audio.context, &frame));
    CHECK_EQUAL(4410u, frame.first_sample);
    CHECK(frames_match(frame, 2, 16));
}

static void a_start_past_the_end_ends_at_once(void)
{
    build(PCM_TAG, 2, 44100, 16, 100, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 60000, &track));
    AudioFrame frame;
    CHECK_EQUAL(AUDIO_FRAME_END, audio.next(audio.context, &frame));
}

static void a_data_chunk_longer_than_the_file_stops_at_the_file(void)
{
    build(PCM_TAG, 2, 44100, 16, 1000, false);
    file.size -= 400;
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    CHECK_EQUAL(900u, track.stream.total_samples);
    CHECK(plays_through(audio, 2, 16, 900));
}

static void files_it_cannot_play_are_refused(void)
{
    AudioTrackInfo track;
    build(FLOAT_TAG, 2, 44100, 32, 100, false);
    AudioSource audio = fresh_source();
    CHECK(!audio.open(audio.context, "a.wav", 0, &track));
    CHECK_EQUAL(1, closes);
    build(PCM_TAG, 6, 48000, 16, 100, false);
    CHECK(!audio.open(audio.context, "a.wav", 0, &track));
    build(PCM_TAG, 2, 44100, 16, 100, false);
    memcpy(file.bytes, "fLaC", 4);
    CHECK(!audio.open(audio.context, "a.wav", 0, &track));
}

static void closing_closes_the_file(void)
{
    build(PCM_TAG, 2, 44100, 16, 100, false);
    AudioSource audio = fresh_source();
    AudioTrackInfo track;
    CHECK(audio.open(audio.context, "a.wav", 0, &track));
    audio.close(audio.context);
    CHECK_EQUAL(1, closes);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(sixteen_bit_stereo_plays_every_frame),
        TEST_CASE(blocks_are_as_long_as_the_buffers_allow),
        TEST_CASE(twenty_four_bit_samples_keep_their_sign),
        TEST_CASE(mono_plays_on_both_sides),
        TEST_CASE(eight_bit_samples_are_centred_on_zero),
        TEST_CASE(the_extensible_header_and_other_chunks_are_understood),
        TEST_CASE(starting_part_way_begins_at_that_frame),
        TEST_CASE(a_start_past_the_end_ends_at_once),
        TEST_CASE(a_data_chunk_longer_than_the_file_stops_at_the_file),
        TEST_CASE(files_it_cannot_play_are_refused),
        TEST_CASE(closing_closes_the_file),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
