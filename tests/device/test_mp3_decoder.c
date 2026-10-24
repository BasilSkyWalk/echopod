#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/mp3_decoder.h"
#include "device/mp3_probe.h"

enum {
    MAX_REFERENCE_SAMPLES = 200000,
    FULL_SCALE = 8388608
};

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
} Fixture;

typedef struct Limited {
    const Fixture *fixture;
    uint32_t base;
    uint32_t available;
} Limited;

static Fixture fixture;
static Mp3Decoder decoder;
static Mp3Decoder reference_decoder;
static int32_t reference[2][MAX_REFERENCE_SAMPLES];
static uint32_t reference_samples;
static int reference_channels;

static void load(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/mp3/%s.mp3", ECHO_FIXTURE_DIR, name);
    free(fixture.bytes);
    fixture = (Fixture){NULL, 0};
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fixture.bytes = malloc((size_t)size + MAD_BUFFER_GUARD);
    if (fixture.bytes != NULL && fread(fixture.bytes, 1, (size_t)size, file) == (size_t)size) {
        fixture.size = (uint32_t)size;
        memset(fixture.bytes + size, 0, MAD_BUFFER_GUARD);
    }
    fclose(file);
}

static int32_t to_24_bits(mad_fixed_t sample)
{
    int64_t value = (int64_t)sample + (1 << 4);
    if (value >= (1 << 28)) {
        value = (1 << 28) - 1;
    }
    if (value < -(1 << 28)) {
        value = -(1 << 28);
    }
    return (int32_t)(value >> 5);
}

static void decode_reference(uint32_t start, uint32_t end)
{
    struct mad_stream stream;
    mad_stream_init(&stream);
    stream.main_data = &reference_decoder.main_data;
    mad_frame_init(&reference_decoder.frame);
    reference_decoder.frame.overlap = &reference_decoder.overlap;
    memset(reference_decoder.overlap, 0, sizeof reference_decoder.overlap);
    mad_synth_init(&reference_decoder.synth);
    uint8_t *copy = malloc(end - start + MAD_BUFFER_GUARD);
    memcpy(copy, fixture.bytes + start, end - start);
    memset(copy + end - start, 0, MAD_BUFFER_GUARD);
    mad_stream_buffer(&stream, copy, end - start + MAD_BUFFER_GUARD);
    reference_samples = 0;
    for (;;) {
        if (mad_frame_decode(&reference_decoder.frame, &stream) == -1) {
            if (MAD_RECOVERABLE(stream.error)) {
                continue;
            }
            break;
        }
        mad_synth_frame(&reference_decoder.synth, &reference_decoder.frame);
        struct mad_pcm *pcm = &reference_decoder.synth.pcm;
        reference_channels = pcm->channels;
        for (unsigned index = 0; index < pcm->length; index++) {
            for (int channel = 0; channel < pcm->channels; channel++) {
                reference[channel][reference_samples] = to_24_bits(pcm->samples[channel][index]);
            }
            reference_samples++;
        }
    }
    free(copy);
}

static bool read_limited(void *context, uint32_t offset, void *destination, uint32_t length)
{
    Limited *limited = context;
    if (offset + length > limited->available) {
        return false;
    }
    memcpy(destination, limited->fixture->bytes + limited->base + offset, length);
    return true;
}

static bool decodes_like_the_reference(const char *name, uint32_t growth)
{
    load(name);
    Mp3Stream stream;
    FlacInput whole = {.read = read_limited, .context = NULL, .size = fixture.size};
    Limited everything = {&fixture, 0, fixture.size};
    whole.context = &everything;
    if (!mp3_probe(whole, &stream)) {
        return false;
    }
    decode_reference(stream.audio_start, stream.audio_end);
    uint32_t length = stream.audio_end - stream.audio_start;
    Limited limited = {&fixture, stream.audio_start, growth == 0 ? length : 0};
    mp3_decoder_start(&decoder, (FlacInput){read_limited, &limited, length});
    uint32_t produced = 0;
    for (int round = 0; round < 100000; round++) {
        Mp3Result result = mp3_decode(&decoder);
        if (result == MP3_FINISHED) {
            return produced == reference_samples && reference_samples > 0;
        }
        if (result == MP3_FAILED) {
            return false;
        }
        if (result == MP3_STARVED) {
            limited.available =
                limited.available + growth > length ? length : limited.available + growth;
            continue;
        }
        if (mp3_decoder_channels(&decoder) != reference_channels) {
            return false;
        }
        for (uint32_t index = 0; index < mp3_decoder_samples(&decoder); index++) {
            for (int channel = 0; channel < reference_channels; channel++) {
                if (mp3_decoder_channel(&decoder, channel)[index] !=
                    reference[channel][produced + index]) {
                    return false;
                }
            }
        }
        produced += mp3_decoder_samples(&decoder);
        if (mp3_decoder_position(&decoder) > length) {
            return false;
        }
    }
    return false;
}

static void a_stereo_file_decodes_like_libmad_itself(void)
{
    CHECK(decodes_like_the_reference("cbr_128_44k_stereo_tagged", 0));
}

static void a_mono_variable_rate_file_decodes_like_libmad_itself(void)
{
    CHECK(decodes_like_the_reference("vbr_48k_mono", 0));
}

static void input_arriving_in_small_pieces_gives_the_same_samples(void)
{
    CHECK(decodes_like_the_reference("cbr_192_48k_stereo_bare", 700));
    CHECK(decodes_like_the_reference("mpeg2_22k_stereo", 333));
}

static void the_sine_comes_out_at_its_level(void)
{
    CHECK(decodes_like_the_reference("cbr_192_48k_stereo_bare", 0));
    int32_t peak = 0;
    for (uint32_t index = reference_samples / 4; index < reference_samples / 2; index++) {
        int32_t value = reference[0][index] < 0 ? -reference[0][index] : reference[0][index];
        peak = value > peak ? value : peak;
    }
    CHECK(peak > FULL_SCALE / 16);
    CHECK(peak < FULL_SCALE / 9);
}

static void the_position_only_moves_forward(void)
{
    load("cbr_128_44k_stereo_tagged");
    Limited limited = {&fixture, 481, 25140 - 481};
    mp3_decoder_start(&decoder, (FlacInput){read_limited, &limited, 25140 - 481});
    uint32_t previous = 0;
    while (mp3_decode(&decoder) == MP3_DECODED) {
        CHECK(mp3_decoder_position(&decoder) >= previous);
        previous = mp3_decoder_position(&decoder);
    }
    CHECK(previous > 0);
}

static void starting_in_the_middle_of_a_frame_finds_the_next_one(void)
{
    load("cbr_192_48k_stereo_bare");
    Limited limited = {&fixture, 1000, fixture.size - 1000};
    mp3_decoder_start(&decoder, (FlacInput){read_limited, &limited, fixture.size - 1000});
    int decoded = 0;
    Mp3Result result;
    while ((result = mp3_decode(&decoder)) == MP3_DECODED) {
        decoded++;
    }
    CHECK_EQUAL(MP3_FINISHED, result);
    CHECK(decoded >= 38);
}

static void garbage_never_hangs(void)
{
    load("cbr_192_48k_stereo_bare");
    for (uint32_t index = 0; index < fixture.size; index++) {
        fixture.bytes[index] = (uint8_t)(index * 7u + 3u);
    }
    Limited limited = {&fixture, 0, fixture.size};
    mp3_decoder_start(&decoder, (FlacInput){read_limited, &limited, fixture.size});
    Mp3Result result = MP3_DECODED;
    for (int round = 0; round < 1000 && result == MP3_DECODED; round++) {
        result = mp3_decode(&decoder);
    }
    CHECK(result == MP3_FINISHED || result == MP3_FAILED);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_stereo_file_decodes_like_libmad_itself),
        TEST_CASE(a_mono_variable_rate_file_decodes_like_libmad_itself),
        TEST_CASE(input_arriving_in_small_pieces_gives_the_same_samples),
        TEST_CASE(the_sine_comes_out_at_its_level),
        TEST_CASE(the_position_only_moves_forward),
        TEST_CASE(starting_in_the_middle_of_a_frame_finds_the_next_one),
        TEST_CASE(garbage_never_hangs),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    free(fixture.bytes);
    return result;
}
