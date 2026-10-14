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
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
