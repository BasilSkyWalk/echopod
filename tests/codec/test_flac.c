#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "codec/flac.h"

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t size;
} Buffer;

static FlacDecoder decoder;

static Buffer load(const char *name, const char *extension)
{
    char path[512];
    snprintf(path, sizeof path, "%s/flac/%s.%s", ECHO_FIXTURE_DIR, name, extension);
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

static uint32_t bytes_read;

static bool read_memory(void *context, uint32_t offset, void *destination, uint32_t length)
{
    const Buffer *buffer = context;
    bytes_read += length;
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

static bool decodes_exactly(const char *name, uint32_t rate, int channels, int bits)
{
    Buffer flac = load(name, "flac");
    Buffer pcm = load(name, "pcm");
    bool ok = flac.size > 0 && pcm.size > 0 && flac_open(&decoder, input_of(&flac)) == FLAC_OK;
    ok = ok && decoder.info.sample_rate == rate && decoder.info.channels == channels &&
         decoder.info.bits_per_sample == bits &&
         decoder.info.total_samples == pcm.size / 4 / (uint32_t)channels;
    uint32_t frame = 0;
    FlacResult result = FLAC_OK;
    while (ok && (result = flac_decode_frame(&decoder)) == FLAC_OK) {
        for (uint32_t sample = 0; ok && sample < decoder.block_samples; sample++) {
            for (int channel = 0; ok && channel < channels; channel++) {
                uint32_t index = (frame + sample) * (uint32_t)channels + (uint32_t)channel;
                ok = 4 * index < pcm.size &&
                     flac_channel(&decoder, channel)[sample] == expected_at(&pcm, index);
            }
        }
        frame += decoder.block_samples;
    }
    ok = ok && result == FLAC_END && frame == decoder.info.total_samples;
    free(flac.bytes);
    free(pcm.bytes);
    return ok;
}

static void sixteen_bit_lpc_stereo_decodes_exactly(void)
{
    CHECK(decodes_exactly("lpc_16_44k_stereo", 44100, 2, 16));
}

static void twenty_four_bit_96k_decodes_exactly(void)
{
    CHECK(decodes_exactly("lpc_24_96k_stereo", 96000, 2, 24));
}

static void fixed_predictors_decode_exactly(void)
{
    CHECK(decodes_exactly("fixed_16_mono", 44100, 1, 16));
}

static void constant_and_verbatim_subframes_decode_exactly(void)
{
    CHECK(decodes_exactly("constant_and_noise", 44100, 2, 16));
}

static void wasted_bits_decode_exactly(void)
{
    CHECK(decodes_exactly("wasted_bits_24", 48000, 2, 24));
}

static void odd_block_sizes_decode_exactly(void)
{
    CHECK(decodes_exactly("odd_blocks_16", 44100, 2, 16));
}

static void independent_channels_decode_exactly(void)
{
    CHECK(decodes_exactly("independent_16", 22050, 2, 16));
}

static void blocks_larger_than_the_buffer_are_refused(void)
{
    Buffer flac = load("big_blocks_16", "flac");
    CHECK(flac.size > 0);
    CHECK_EQUAL(FLAC_UNSUPPORTED, flac_open(&decoder, input_of(&flac)));
    free(flac.bytes);
}

static void a_file_that_is_not_flac_is_refused(void)
{
    Buffer pcm = load("fixed_16_mono", "pcm");
    CHECK_EQUAL(FLAC_ERROR, flac_open(&decoder, input_of(&pcm)));
    free(pcm.bytes);
}

static FlacResult decode_to_the_end(void)
{
    FlacResult result;
    while ((result = flac_decode_frame(&decoder)) == FLAC_OK) {
    }
    return result;
}

static void a_truncated_file_ends_with_an_error(void)
{
    Buffer flac = load("lpc_16_44k_stereo", "flac");
    flac.size -= 100;
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    CHECK_EQUAL(FLAC_ERROR, decode_to_the_end());
    free(flac.bytes);
}

static void a_corrupted_frame_is_caught(void)
{
    Buffer flac = load("lpc_16_44k_stereo", "flac");
    flac.bytes[flac.size - 2000] ^= 0x10;
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    CHECK_EQUAL(FLAC_ERROR, decode_to_the_end());
    free(flac.bytes);
}

static bool seeks_to(const char *name, uint32_t target)
{
    Buffer flac = load(name, "flac");
    Buffer pcm = load(name, "pcm");
    bool ok = flac_open(&decoder, input_of(&flac)) == FLAC_OK &&
              flac_seek(&decoder, target) == FLAC_OK && flac_decode_frame(&decoder) == FLAC_OK;
    uint32_t first = decoder.frame_first_sample;
    ok = ok && first <= target && target < first + decoder.block_samples;
    int channels = decoder.info.channels;
    for (uint32_t sample = 0; ok && sample < decoder.block_samples; sample++) {
        for (int channel = 0; ok && channel < channels; channel++) {
            uint32_t index = (first + sample) * (uint32_t)channels + (uint32_t)channel;
            ok = flac_channel(&decoder, channel)[sample] == expected_at(&pcm, index);
        }
    }
    free(flac.bytes);
    free(pcm.bytes);
    return ok;
}

static void seeking_lands_on_the_frame_that_holds_the_target(void)
{
    CHECK(seeks_to("lpc_16_44k_stereo", 0));
    CHECK(seeks_to("lpc_16_44k_stereo", 4095));
    CHECK(seeks_to("lpc_16_44k_stereo", 4096));
    CHECK(seeks_to("lpc_16_44k_stereo", 5999));
    CHECK(seeks_to("lpc_24_96k_stereo", 2500));
    CHECK(seeks_to("odd_blocks_16", 1152));
    CHECK(seeks_to("odd_blocks_16", 3000));
    CHECK(seeks_to("constant_and_noise", 7777));
}

static void decoding_carries_on_after_a_seek(void)
{
    Buffer flac = load("odd_blocks_16", "flac");
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    CHECK_EQUAL(FLAC_OK, flac_seek(&decoder, 1500));
    uint32_t total = 0;
    FlacResult result;
    while ((result = flac_decode_frame(&decoder)) == FLAC_OK) {
        total += decoder.block_samples;
    }
    CHECK_EQUAL(FLAC_END, result);
    CHECK_EQUAL(3001u - 1152u, total);
    free(flac.bytes);
}

static void seeking_past_the_end_is_refused(void)
{
    Buffer flac = load("odd_blocks_16", "flac");
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    CHECK_EQUAL(FLAC_END, flac_seek(&decoder, 3001));
    free(flac.bytes);
}

static void frames_report_their_first_sample(void)
{
    Buffer flac = load("odd_blocks_16", "flac");
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    CHECK_EQUAL(FLAC_OK, flac_decode_frame(&decoder));
    CHECK_EQUAL(0u, decoder.frame_first_sample);
    CHECK_EQUAL(FLAC_OK, flac_decode_frame(&decoder));
    CHECK_EQUAL(1152u, decoder.frame_first_sample);
    free(flac.bytes);
}

static void a_seek_reads_little_of_the_file(void)
{
    Buffer flac = load("lpc_24_96k_stereo", "flac");
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    bytes_read = 0;
    CHECK_EQUAL(FLAC_OK, flac_seek(&decoder, 4500));
    CHECK(bytes_read < 64 * 1024);
    free(flac.bytes);
}

static FlacBits header_bits;

static void the_header_alone_gives_the_stream_info_and_audio_offset(void)
{
    Buffer flac = load("lpc_24_96k_stereo", "flac");
    FlacInfo info = {0};
    uint32_t audio_offset = 0;
    CHECK_EQUAL(FLAC_OK, flac_read_header(&header_bits, input_of(&flac), &info, &audio_offset));
    CHECK_EQUAL(FLAC_OK, flac_open(&decoder, input_of(&flac)));
    CHECK_EQUAL(decoder.audio_offset, audio_offset);
    CHECK_EQUAL(decoder.info.sample_rate, info.sample_rate);
    CHECK_EQUAL(decoder.info.bits_per_sample, info.bits_per_sample);
    CHECK_EQUAL(decoder.info.channels, info.channels);
    CHECK_EQUAL(decoder.info.max_block, info.max_block);
    CHECK_EQUAL(decoder.info.total_samples, info.total_samples);
    free(flac.bytes);
}

static void a_header_that_is_not_flac_is_refused(void)
{
    uint8_t bytes[64] = {'R', 'I', 'F', 'F'};
    Buffer buffer = {bytes, sizeof bytes};
    FlacInfo info = {0};
    uint32_t audio_offset = 0;
    CHECK_EQUAL(FLAC_ERROR,
                flac_read_header(&header_bits, input_of(&buffer), &info, &audio_offset));
}

static void decoding_can_start_from_audio_bytes_alone(void)
{
    Buffer flac = load("lpc_24_96k_stereo", "flac");
    Buffer pcm = load("lpc_24_96k_stereo", "pcm");
    FlacInfo info = {0};
    uint32_t audio_offset = 0;
    CHECK_EQUAL(FLAC_OK, flac_read_header(&header_bits, input_of(&flac), &info, &audio_offset));
    Buffer audio = {flac.bytes + audio_offset, flac.size - audio_offset};
    flac_start(&decoder, input_of(&audio), info);
    CHECK_EQUAL(FLAC_OK, flac_decode_frame(&decoder));
    CHECK_EQUAL(0u, decoder.frame_first_sample);
    CHECK(decoder.block_samples > 0);
    bool exact = true;
    for (uint32_t sample = 0; sample < decoder.block_samples; sample++) {
        exact = exact && flac_channel(&decoder, 0)[sample] == expected_at(&pcm, 2 * sample) &&
                flac_channel(&decoder, 1)[sample] == expected_at(&pcm, 2 * sample + 1);
    }
    CHECK(exact);
    free(flac.bytes);
    free(pcm.bytes);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(sixteen_bit_lpc_stereo_decodes_exactly),
        TEST_CASE(twenty_four_bit_96k_decodes_exactly),
        TEST_CASE(fixed_predictors_decode_exactly),
        TEST_CASE(constant_and_verbatim_subframes_decode_exactly),
        TEST_CASE(wasted_bits_decode_exactly),
        TEST_CASE(odd_block_sizes_decode_exactly),
        TEST_CASE(independent_channels_decode_exactly),
        TEST_CASE(blocks_larger_than_the_buffer_are_refused),
        TEST_CASE(a_file_that_is_not_flac_is_refused),
        TEST_CASE(a_truncated_file_ends_with_an_error),
        TEST_CASE(a_corrupted_frame_is_caught),
        TEST_CASE(frames_report_their_first_sample),
        TEST_CASE(seeking_lands_on_the_frame_that_holds_the_target),
        TEST_CASE(decoding_carries_on_after_a_seek),
        TEST_CASE(seeking_past_the_end_is_refused),
        TEST_CASE(a_seek_reads_little_of_the_file),
        TEST_CASE(the_header_alone_gives_the_stream_info_and_audio_offset),
        TEST_CASE(a_header_that_is_not_flac_is_refused),
        TEST_CASE(decoding_can_start_from_audio_bytes_alone),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
