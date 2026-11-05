#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "codec/qoa.h"

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t size;
} Buffer;

static QoaDecoder decoder;

static Buffer load(const char *name, const char *extension)
{
    char path[512];
    snprintf(path, sizeof path, "%s/qoa/%s.%s", ECHO_FIXTURE_DIR, name, extension);
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

static int16_t expected_at(const Buffer *pcm, uint32_t frame, uint32_t channel, uint32_t channels)
{
    int16_t value;
    memcpy(&value, pcm->bytes + 2 * (frame * channels + channel), sizeof value);
    return value;
}

static uint32_t decode_into(QoaDecoder *opened, int16_t *output, uint32_t frames, uint32_t channels,
                            uint32_t base)
{
    uint32_t done = 0;
    while (done < frames) {
        const int16_t *left = NULL;
        const int16_t *right = NULL;
        uint32_t stride = 0;
        uint32_t first = 0;
        uint32_t count = 0;
        if (qoa_next_block(opened, &left, &right, &stride, &first, &count) != QOA_OK) {
            return done;
        }
        CHECK_EQUAL(base + done, first);
        CHECK_EQUAL(channels, stride);
        CHECK(count <= QOA_BLOCK_SAMPLES);
        for (uint32_t index = 0; index < count && done + index < frames; index++) {
            output[(done + index) * channels] = left[index * stride];
            if (channels > 1) {
                output[(done + index) * channels + 1] = right[index * stride];
            }
        }
        done += count;
        if (done >= frames) {
            return frames;
        }
    }
    return done;
}

static void every_fixture_decodes_to_the_reference_pcm(void)
{
    static const struct {
        const char *name;
        uint32_t frames;
        uint32_t rate;
        uint32_t channels;
    } FIXTURES[] = {
        {"stereo_44k", 12000, 44100, 2},
        {"mono_22k", 777, 22050, 1},
        {"stereo_48k", 5171, 48000, 2},
    };
    for (size_t index = 0; index < sizeof FIXTURES / sizeof FIXTURES[0]; index++) {
        Buffer encoded = load(FIXTURES[index].name, "qoa");
        Buffer pcm = load(FIXTURES[index].name, "pcm");
        CHECK(encoded.size > 0 &&
              pcm.size == FIXTURES[index].frames * FIXTURES[index].channels * 2);
        FlacInput input = input_of(&encoded);
        CHECK_EQUAL(QOA_OK, qoa_open(&decoder, input));
        CHECK_EQUAL(FIXTURES[index].rate, decoder.info.sample_rate);
        CHECK_EQUAL(FIXTURES[index].channels, decoder.info.channels);
        CHECK_EQUAL(FIXTURES[index].frames, decoder.info.total_samples);
        static int16_t decoded[25000 * QOA_MAX_PLAYBACK_CHANNELS];
        CHECK_EQUAL(FIXTURES[index].frames, decode_into(&decoder, decoded, FIXTURES[index].frames,
                                                        FIXTURES[index].channels, 0));
        for (uint32_t frame = 0; frame < FIXTURES[index].frames; frame++) {
            for (uint32_t channel = 0; channel < FIXTURES[index].channels; channel++) {
                CHECK_EQUAL(expected_at(&pcm, frame, channel, FIXTURES[index].channels),
                            decoded[frame * FIXTURES[index].channels + channel]);
            }
        }
        const int16_t *left = NULL;
        const int16_t *right = NULL;
        uint32_t stride = 0;
        uint32_t first = 0;
        uint32_t count = 0;
        CHECK_EQUAL(QOA_END, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
        free(encoded.bytes);
        free(pcm.bytes);
    }
}

static void frames_are_split_into_blocks_without_a_gap(void)
{
    Buffer encoded = load("stereo_44k", "qoa");
    CHECK(qoa_open(&decoder, input_of(&encoded)) == QOA_OK);
    const int16_t *left = NULL;
    const int16_t *right = NULL;
    uint32_t stride = 0;
    uint32_t first = 0;
    uint32_t count = 0;
    CHECK_EQUAL(QOA_OK, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    CHECK_EQUAL(0, first);
    CHECK_EQUAL(QOA_BLOCK_SAMPLES, count);
    CHECK_EQUAL(QOA_OK, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    CHECK_EQUAL(QOA_BLOCK_SAMPLES, first);
    CHECK_EQUAL(QOA_FRAME_SAMPLES - QOA_BLOCK_SAMPLES, count);
    CHECK_EQUAL(QOA_OK, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    CHECK_EQUAL(QOA_FRAME_SAMPLES, first);
    free(encoded.bytes);
}

static void a_seek_lands_on_the_exact_sample(void)
{
    static const uint32_t TARGETS[] = {0, 19, 20, 4599, 4608, 5119, 5120, 9999, 11999};
    Buffer encoded = load("stereo_44k", "qoa");
    Buffer pcm = load("stereo_44k", "pcm");
    CHECK(qoa_open(&decoder, input_of(&encoded)) == QOA_OK);
    static int16_t decoded[6000 * QOA_MAX_PLAYBACK_CHANNELS];
    for (size_t index = 0; index < sizeof TARGETS / sizeof TARGETS[0]; index++) {
        uint32_t target = TARGETS[index];
        CHECK_EQUAL(QOA_OK, qoa_seek(&decoder, target));
        uint32_t wanted = 12000 - target < 3000 ? 12000 - target : 3000;
        CHECK_EQUAL(wanted, decode_into(&decoder, decoded, wanted, 2, target));
        for (uint32_t frame = 0; frame < wanted; frame++) {
            CHECK_EQUAL(expected_at(&pcm, target + frame, 0, 2), decoded[frame * 2]);
            CHECK_EQUAL(expected_at(&pcm, target + frame, 1, 2), decoded[frame * 2 + 1]);
        }
    }
    free(encoded.bytes);
    free(pcm.bytes);
}

static void a_damaged_later_frame_fails(void)
{
    Buffer encoded = load("stereo_44k", "qoa");
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_OK, qoa_open(&decoder, input));
    memset(encoded.bytes + QOA_FILE_HEADER_BYTES + decoder.info.frame_bytes, 0, 8);
    const int16_t *left = NULL;
    const int16_t *right = NULL;
    uint32_t stride = 0;
    uint32_t first = 0;
    uint32_t count = 0;
    CHECK_EQUAL(QOA_OK, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    CHECK_EQUAL(QOA_OK, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    CHECK_EQUAL(QOA_ERROR, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    free(encoded.bytes);
}

static void a_seek_past_the_end_finishes(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    CHECK(qoa_open(&decoder, input_of(&encoded)) == QOA_OK);
    CHECK_EQUAL(QOA_END, qoa_seek(&decoder, 777));
    const int16_t *left = NULL;
    const int16_t *right = NULL;
    uint32_t stride = 0;
    uint32_t first = 0;
    uint32_t count = 0;
    CHECK_EQUAL(QOA_END, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    free(encoded.bytes);
}

static void a_truncated_frame_fails(void)
{
    Buffer encoded = load("stereo_44k", "qoa");
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_OK, qoa_open(&decoder, input));
    const int16_t *left = NULL;
    const int16_t *right = NULL;
    uint32_t stride = 0;
    uint32_t first = 0;
    uint32_t count = 0;
    decoder.input.size = 2000;
    CHECK_EQUAL(QOA_ERROR, qoa_next_block(&decoder, &left, &right, &stride, &first, &count));
    free(encoded.bytes);
}

static void a_bad_magic_is_refused(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    encoded.bytes[0] = 'n';
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_ERROR, qoa_open(&decoder, input));
    free(encoded.bytes);
}

static void a_missing_header_is_refused(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    encoded.size = 8;
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_ERROR, qoa_open(&decoder, input));
    free(encoded.bytes);
}

static void more_than_two_channels_are_unsupported(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    encoded.bytes[8] = 3;
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_UNSUPPORTED, qoa_open(&decoder, input));
    free(encoded.bytes);
}

static void drain_check(QoaDecoder *opened)
{
    uint32_t guard = opened->info.total_samples / QOA_BLOCK_SAMPLES + 16;
    for (uint32_t round = 0; round < guard; round++) {
        const int16_t *left = NULL;
        const int16_t *right = NULL;
        uint32_t stride = 0;
        uint32_t first = 0;
        uint32_t count = 0;
        if (qoa_next_block(opened, &left, &right, &stride, &first, &count) != QOA_OK) {
            return;
        }
        CHECK(left != NULL && right != NULL);
        CHECK_EQUAL(opened->info.channels, stride);
        CHECK(count > 0 && count <= QOA_BLOCK_SAMPLES);
        CHECK(first <= opened->info.total_samples);
        CHECK(count <= opened->info.total_samples - first);
    }
}

static void damaged_files_never_produce_blocks_outside_the_stream(void)
{
    static const char *const FIXTURES[] = {"stereo_44k", "mono_22k", "stereo_48k"};
    static QoaDecoder damaged;
    for (size_t index = 0; index < sizeof FIXTURES / sizeof FIXTURES[0]; index++) {
        Buffer encoded = load(FIXTURES[index], "qoa");
        uint8_t *copy = malloc(encoded.size);
        CHECK(copy != NULL);
        uint32_t step = 1 + encoded.size / 200;
        for (uint32_t cut = 0; cut <= encoded.size; cut += step) {
            FlacInput input = {read_memory, &encoded, cut};
            if (qoa_open(&damaged, input) == QOA_OK) {
                drain_check(&damaged);
            }
        }
        for (uint32_t flip = 0; flip < encoded.size; flip += step) {
            memcpy(copy, encoded.bytes, encoded.size);
            copy[flip] ^= 0xFF;
            if (flip + 1 < encoded.size) {
                copy[flip + 1] = 0xFF;
            }
            Buffer mutated = {copy, encoded.size};
            FlacInput input = {read_memory, &mutated, mutated.size};
            if (qoa_open(&damaged, input) == QOA_OK) {
                drain_check(&damaged);
            }
        }
        free(copy);
        free(encoded.bytes);
    }
}

static void an_empty_sample_count_is_refused(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    memset(encoded.bytes + 4, 0, 4);
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_ERROR, qoa_open(&decoder, input));
    free(encoded.bytes);
}

static void a_zero_sample_rate_is_refused(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    memset(encoded.bytes + 9, 0, 3);
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_ERROR, qoa_open(&decoder, input));
    free(encoded.bytes);
}

static void a_zero_channel_count_is_refused(void)
{
    Buffer encoded = load("mono_22k", "qoa");
    encoded.bytes[8] = 0;
    FlacInput input = input_of(&encoded);
    CHECK_EQUAL(QOA_ERROR, qoa_open(&decoder, input));
    free(encoded.bytes);
}

int main(void)
{
    static const TestCase CASES[] = {
        TEST_CASE(every_fixture_decodes_to_the_reference_pcm),
        TEST_CASE(frames_are_split_into_blocks_without_a_gap),
        TEST_CASE(a_seek_lands_on_the_exact_sample),
        TEST_CASE(a_seek_past_the_end_finishes),
        TEST_CASE(a_damaged_later_frame_fails),
        TEST_CASE(a_truncated_frame_fails),
        TEST_CASE(a_bad_magic_is_refused),
        TEST_CASE(a_missing_header_is_refused),
        TEST_CASE(more_than_two_channels_are_unsupported),
        TEST_CASE(an_empty_sample_count_is_refused),
        TEST_CASE(a_zero_sample_rate_is_refused),
        TEST_CASE(a_zero_channel_count_is_refused),
        TEST_CASE(damaged_files_never_produce_blocks_outside_the_stream),
    };
    return test_run_all(CASES, TEST_CASE_COUNT(CASES));
}
