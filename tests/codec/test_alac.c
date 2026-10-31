#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "codec/alac.h"
#include "codec/mp4.h"

enum {
    PIECE_BYTES = 777,
    STEREO_FRAMES = 10000,
    FRAME_LENGTH = 4096
};

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t size;
} Buffer;

static AlacDecoder decoder;

static Buffer load(const char *folder, const char *name, const char *extension)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s/%s.%s", ECHO_FIXTURE_DIR, folder, name, extension);
    Buffer buffer = {NULL, 0};
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return buffer;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer.bytes = malloc((size_t)size);
    if (buffer.bytes != NULL && fread(buffer.bytes, 1, (size_t)size, file) == (size_t)size) {
        buffer.size = (uint32_t)size;
    }
    fclose(file);
    return buffer;
}

static bool read_memory(void *context, uint32_t offset, void *destination, uint32_t length)
{
    const Buffer *buffer = context;
    if (offset > buffer->size || length > buffer->size - offset) {
        return false;
    }
    memcpy(destination, buffer->bytes + offset, length);
    return true;
}

static FlacInput input_of(Buffer *buffer)
{
    return (FlacInput){.read = read_memory, .context = buffer, .size = buffer->size};
}

static int32_t expected_at(const Buffer *pcm, uint32_t index)
{
    int32_t value;
    memcpy(&value, pcm->bytes + 4 * index, sizeof value);
    return value;
}

static Buffer drain(FlacInput input)
{
    Buffer out = {malloc(input.size), input.size};
    uint32_t done = 0;
    while (out.bytes != NULL && done < input.size) {
        uint32_t piece = input.size - done < PIECE_BYTES ? input.size - done : PIECE_BYTES;
        if (!input.read(input.context, done, out.bytes + done, piece)) {
            free(out.bytes);
            return (Buffer){NULL, 0};
        }
        done += piece;
    }
    return out;
}

typedef struct Decoded {
    bool ok;
    uint32_t first;
    uint32_t end;
    uint32_t frames;
} Decoded;

static Decoded decode_from(Buffer *file, const Buffer *pcm, uint32_t start)
{
    Decoded decoded = {false, 0, 0, 0};
    Mp4Track track;
    Mp4Stream stream;
    if (!mp4_probe(input_of(file), &track) ||
        !mp4_stream_open(&stream, input_of(file), &track, start, &decoded.first)) {
        return decoded;
    }
    Buffer records = drain(mp4_stream_input(&stream));
    AlacConfig config = {0};
    uint32_t config_bytes = records.size >= 4 ? mp4_record_length(records.bytes) : 0;
    bool ok = records.bytes != NULL && config_bytes + 4 <= records.size &&
              alac_parse_config(records.bytes + 4, config_bytes, &config);
    alac_start(&decoder, config);
    uint32_t at = 4 + config_bytes;
    uint32_t sample = decoded.first;
    FlacInput input = input_of(&records);
    while (ok && at + 4 <= records.size) {
        uint32_t length = mp4_record_length(records.bytes + at);
        ok = alac_decode_frame(&decoder, input, at + 4, length) == ALAC_OK;
        for (uint32_t index = 0; ok && index < decoder.block_samples; index++) {
            for (uint32_t channel = 0; ok && channel < config.channels; channel++) {
                uint32_t place = (sample + index) * config.channels + channel;
                ok = 4 * place < pcm->size &&
                     alac_channel(&decoder, (int)channel)[index] == expected_at(pcm, place);
            }
        }
        sample += decoder.block_samples;
        at += 4 + length;
        decoded.frames++;
    }
    decoded.ok = ok && at == records.size;
    decoded.end = sample;
    free(records.bytes);
    return decoded;
}

static bool decodes_exactly(const char *name, uint32_t rate, uint8_t channels, uint8_t bits)
{
    Buffer file = load("alac", name, "m4a");
    Buffer pcm = load("alac", name, "pcm");
    Mp4Track track;
    bool ok = file.size > 0 && pcm.size > 0 && mp4_probe(input_of(&file), &track) &&
              track.codec == MP4_CODEC_ALAC && track.sample_rate == rate &&
              track.channels == channels && track.bits_per_sample == bits && track.lead_trim == 0 &&
              track.total_samples == pcm.size / 4 / channels;
    Decoded decoded = decode_from(&file, &pcm, 0);
    ok = ok && decoded.ok && decoded.first == 0 && decoded.end == track.total_samples;
    free(file.bytes);
    free(pcm.bytes);
    return ok;
}

static void sixteen_bit_stereo_decodes_exactly(void)
{
    CHECK(decodes_exactly("stereo_16_44k", 44100, 2, 16));
}

static void twenty_four_bit_stereo_decodes_exactly(void)
{
    CHECK(decodes_exactly("stereo_24_96k", 96000, 2, 24));
}

static void mono_decodes_exactly(void)
{
    CHECK(decodes_exactly("mono_16_22k", 22050, 1, 16));
    CHECK(decodes_exactly("mono_24_48k", 48000, 1, 24));
}

static void noise_falls_back_to_verbatim_frames_and_decodes_exactly(void)
{
    CHECK(decodes_exactly("noise_16", 44100, 2, 16));
    CHECK(decodes_exactly("noise_24", 48000, 2, 24));
}

static void silence_runs_decode_exactly(void)
{
    CHECK(decodes_exactly("gaps_16", 44100, 2, 16));
}

static void the_fastest_setting_decodes_exactly(void)
{
    CHECK(decodes_exactly("fast_16", 44100, 2, 16));
}

static void a_file_with_the_index_first_decodes_exactly(void)
{
    CHECK(decodes_exactly("faststart_16", 44100, 2, 16));
}

static void seeking_starts_at_the_frame_holding_the_sample(void)
{
    Buffer file = load("alac", "stereo_16_44k", "m4a");
    Buffer pcm = load("alac", "stereo_16_44k", "pcm");
    Decoded decoded = decode_from(&file, &pcm, 5000);
    CHECK(decoded.ok);
    CHECK_EQUAL(FRAME_LENGTH, decoded.first);
    CHECK_EQUAL(STEREO_FRAMES, decoded.end);
    CHECK_EQUAL(2, decoded.frames);
    decoded = decode_from(&file, &pcm, 2 * FRAME_LENGTH);
    CHECK(decoded.ok);
    CHECK_EQUAL(2 * FRAME_LENGTH, decoded.first);
    CHECK_EQUAL(1, decoded.frames);
    free(file.bytes);
    free(pcm.bytes);
}

static void seeking_past_the_end_leaves_only_the_config(void)
{
    Buffer file = load("alac", "stereo_16_44k", "m4a");
    Buffer pcm = load("alac", "stereo_16_44k", "pcm");
    Decoded decoded = decode_from(&file, &pcm, STEREO_FRAMES + 100);
    CHECK(decoded.ok);
    CHECK_EQUAL(STEREO_FRAMES, decoded.first);
    CHECK_EQUAL(0, decoded.frames);
    free(file.bytes);
    free(pcm.bytes);
}

static void the_tagged_fixture_plays_a_steady_tone(void)
{
    Buffer file = load("tags", "alac", "m4a");
    Mp4Track track;
    Mp4Stream stream;
    uint32_t first = 0;
    CHECK(mp4_probe(input_of(&file), &track));
    CHECK(mp4_stream_open(&stream, input_of(&file), &track, 0, &first));
    CHECK_EQUAL(44100, track.total_samples);
    Buffer records = drain(mp4_stream_input(&stream));
    AlacConfig config;
    CHECK(alac_parse_config(records.bytes + 4, mp4_record_length(records.bytes), &config));
    alac_start(&decoder, config);
    uint32_t at = 4 + ALAC_CONFIG_BYTES;
    uint32_t samples = 0;
    int32_t peak = 0;
    int32_t step = 0;
    int32_t previous = 0;
    bool same = true;
    while (at + 4 <= records.size) {
        uint32_t length = mp4_record_length(records.bytes + at);
        CHECK_EQUAL(ALAC_OK, alac_decode_frame(&decoder, input_of(&records), at + 4, length));
        for (uint32_t index = 0; index < decoder.block_samples; index++) {
            int32_t left = alac_channel(&decoder, 0)[index];
            same = same && left == alac_channel(&decoder, 1)[index];
            peak = abs(left) > peak ? abs(left) : peak;
            step = abs(left - previous) > step ? abs(left - previous) : step;
            previous = left;
        }
        samples += decoder.block_samples;
        at += 4 + length;
    }
    CHECK(same);
    CHECK_EQUAL(44100, samples);
    CHECK(peak > 2850 && peak < 2950);
    CHECK(step < 200);
    free(records.bytes);
    free(file.bytes);
}

static void a_cut_frame_is_an_error(void)
{
    Buffer file = load("alac", "stereo_16_44k", "m4a");
    Mp4Track track;
    Mp4Stream stream;
    uint32_t first = 0;
    CHECK(mp4_probe(input_of(&file), &track));
    CHECK(mp4_stream_open(&stream, input_of(&file), &track, 0, &first));
    Buffer records = drain(mp4_stream_input(&stream));
    AlacConfig config;
    CHECK(alac_parse_config(records.bytes + 4, ALAC_CONFIG_BYTES, &config));
    alac_start(&decoder, config);
    uint32_t at = 4 + ALAC_CONFIG_BYTES;
    uint32_t length = mp4_record_length(records.bytes + at);
    CHECK_EQUAL(ALAC_ERROR, alac_decode_frame(&decoder, input_of(&records), at + 4, length / 2));
    CHECK_EQUAL(ALAC_ERROR, alac_decode_frame(&decoder, input_of(&records), at + 4, records.size));
    CHECK_EQUAL(ALAC_OK, alac_decode_frame(&decoder, input_of(&records), at + 4, length));
    free(records.bytes);
    free(file.bytes);
}

static void unsupported_configs_are_refused(void)
{
    uint8_t bytes[ALAC_CONFIG_BYTES] = {0, 0, 0x10, 0, 0, 16, 40, 10, 14, 2, 0,    255,
                                        0, 0, 0,    0, 0, 0,  0,  0,  0,  0, 0xAC, 0x44};
    AlacConfig config;
    CHECK(alac_parse_config(bytes, sizeof bytes, &config));
    CHECK_EQUAL(4096, config.frame_length);
    CHECK_EQUAL(44100, config.sample_rate);
    CHECK_EQUAL(255, config.max_run);
    CHECK(!alac_parse_config(bytes, sizeof bytes - 1, &config));
    bytes[5] = 32;
    CHECK(!alac_parse_config(bytes, sizeof bytes, &config));
    bytes[5] = 20;
    CHECK(alac_parse_config(bytes, sizeof bytes, &config));
    bytes[9] = 3;
    CHECK(!alac_parse_config(bytes, sizeof bytes, &config));
    bytes[9] = 2;
    bytes[2] = 0x20;
    CHECK(!alac_parse_config(bytes, sizeof bytes, &config));
    bytes[2] = 0x10;
    bytes[4] = 1;
    CHECK(!alac_parse_config(bytes, sizeof bytes, &config));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(sixteen_bit_stereo_decodes_exactly),
        TEST_CASE(twenty_four_bit_stereo_decodes_exactly),
        TEST_CASE(mono_decodes_exactly),
        TEST_CASE(noise_falls_back_to_verbatim_frames_and_decodes_exactly),
        TEST_CASE(silence_runs_decode_exactly),
        TEST_CASE(the_fastest_setting_decodes_exactly),
        TEST_CASE(a_file_with_the_index_first_decodes_exactly),
        TEST_CASE(seeking_starts_at_the_frame_holding_the_sample),
        TEST_CASE(seeking_past_the_end_leaves_only_the_config),
        TEST_CASE(the_tagged_fixture_plays_a_steady_tone),
        TEST_CASE(a_cut_frame_is_an_error),
        TEST_CASE(unsupported_configs_are_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
