#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/mp3_probe.h"

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
} Fixture;

static Fixture fixture;

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
    fixture.bytes = malloc((size_t)size);
    if (fixture.bytes != NULL && fread(fixture.bytes, 1, (size_t)size, file) == (size_t)size) {
        fixture.size = (uint32_t)size;
    }
    fclose(file);
}

static bool read_fixture(void *context, uint32_t offset, void *destination, uint32_t length)
{
    (void)context;
    if (offset > fixture.size || length > fixture.size - offset) {
        return false;
    }
    memcpy(destination, fixture.bytes + offset, length);
    return true;
}

static bool probe(const char *name, Mp3Stream *stream)
{
    load(name);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    return mp3_probe(input, stream);
}

static void a_tagged_file_skips_both_tags_and_the_info_frame(void)
{
    Mp3Stream stream;
    CHECK(probe("cbr_128_44k_stereo_tagged", &stream));
    CHECK_EQUAL(44100u, stream.sample_rate);
    CHECK_EQUAL(2, stream.channels);
    CHECK_EQUAL(1152, stream.samples_per_frame);
    CHECK_EQUAL(481u, stream.audio_start);
    CHECK_EQUAL(25140u, stream.audio_end);
    CHECK_EQUAL(59u * 1152u, stream.total_samples);
    CHECK(stream.has_toc);
}

static void a_bare_constant_rate_file_is_timed_from_its_size(void)
{
    Mp3Stream stream;
    CHECK(probe("cbr_192_48k_stereo_bare", &stream));
    CHECK_EQUAL(48000u, stream.sample_rate);
    CHECK_EQUAL(0u, stream.audio_start);
    CHECK_EQUAL(24768u, stream.audio_end);
    CHECK_EQUAL(43u * 1152u, stream.total_samples);
    CHECK(!stream.has_toc);
}

static void a_variable_rate_file_takes_its_length_from_the_xing_header(void)
{
    Mp3Stream stream;
    CHECK(probe("vbr_48k_mono", &stream));
    CHECK_EQUAL(1, stream.channels);
    CHECK_EQUAL(192u, stream.audio_start);
    CHECK_EQUAL(85u * 1152u, stream.total_samples);
    CHECK(stream.has_toc);
}

static void mpeg_2_frames_hold_576_samples(void)
{
    Mp3Stream stream;
    CHECK(probe("mpeg2_22k_stereo", &stream));
    CHECK_EQUAL(22050u, stream.sample_rate);
    CHECK_EQUAL(576, stream.samples_per_frame);
    CHECK_EQUAL(208u, stream.audio_start);
    CHECK_EQUAL(41u * 576u, stream.total_samples);
}

static void seeking_a_constant_rate_file_is_proportional(void)
{
    Mp3Stream stream;
    CHECK(probe("cbr_192_48k_stereo_bare", &stream));
    CHECK_EQUAL(0u, mp3_seek_offset(&stream, 0));
    CHECK_EQUAL(12384u, mp3_seek_offset(&stream, stream.total_samples / 2));
    CHECK_EQUAL(stream.audio_end, mp3_seek_offset(&stream, stream.total_samples * 2));
}

static void seeking_with_a_table_follows_it(void)
{
    Mp3Stream stream;
    CHECK(probe("vbr_48k_mono", &stream));
    CHECK_EQUAL(stream.audio_start, mp3_seek_offset(&stream, 0));
    uint32_t previous = stream.audio_start;
    for (uint32_t step = 1; step <= 10; step++) {
        uint32_t offset = mp3_seek_offset(&stream, stream.total_samples / 10 * step);
        CHECK(offset >= previous);
        CHECK(offset <= stream.audio_end);
        previous = offset;
    }
    uint32_t bytes = stream.audio_end - stream.audio_start;
    uint32_t middle = mp3_seek_offset(&stream, stream.total_samples / 2);
    uint32_t expected = stream.audio_start + (uint32_t)((uint64_t)stream.toc[50] * bytes / 256);
    CHECK_EQUAL(expected, middle);
}

static void a_lame_tag_trims_the_encoder_delay_and_padding(void)
{
    Mp3Stream stream;
    CHECK(probe("cbr_128_44k_stereo_tagged", &stream));
    CHECK_EQUAL(576u + MP3_DECODER_DELAY, stream.lead_trim);
    CHECK_EQUAL(1242u - MP3_DECODER_DELAY, stream.tail_trim);
    CHECK_EQUAL(59u * 1152u, stream.total_samples);
    CHECK_EQUAL(66150u, mp3_playable_samples(&stream));
}

static void trimming_leaves_exactly_the_encoded_length(void)
{
    Mp3Stream stream;
    CHECK(probe("vbr_48k_mono", &stream));
    CHECK_EQUAL(96000u, mp3_playable_samples(&stream));
    CHECK(probe("mpeg2_22k_stereo", &stream));
    CHECK_EQUAL(22050u, mp3_playable_samples(&stream));
}

static void a_file_without_an_info_frame_is_not_trimmed(void)
{
    Mp3Stream stream;
    CHECK(probe("cbr_192_48k_stereo_bare", &stream));
    CHECK_EQUAL(0u, stream.lead_trim);
    CHECK_EQUAL(0u, stream.tail_trim);
    CHECK_EQUAL(stream.total_samples, mp3_playable_samples(&stream));
}

static void an_unknown_encoder_tag_is_not_trusted(void)
{
    Mp3Stream stream;
    load("cbr_128_44k_stereo_tagged");
    CHECK(memcmp(fixture.bytes + 220, "Lavc", 4) == 0);
    memcpy(fixture.bytes + 220, "Xxxx", 4);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    CHECK(mp3_probe(input, &stream));
    CHECK_EQUAL(0u, stream.lead_trim);
    CHECK_EQUAL(0u, stream.tail_trim);
    CHECK_EQUAL(59u * 1152u, mp3_playable_samples(&stream));
}

static void trims_longer_than_the_track_are_ignored(void)
{
    Mp3Stream stream;
    load("mpeg2_22k_stereo");
    CHECK(memcmp(fixture.bytes + 21, "Info", 4) == 0);
    CHECK(memcmp(fixture.bytes + 141, "Lavc", 4) == 0);
    memcpy(fixture.bytes + 29, "\0\0\0\5", 4);
    fixture.bytes[141 + 21] = 0xFF;
    fixture.bytes[141 + 22] = 0xFF;
    fixture.bytes[141 + 23] = 0xFF;
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    CHECK(mp3_probe(input, &stream));
    CHECK_EQUAL(5u * 576u, stream.total_samples);
    CHECK_EQUAL(0u, stream.lead_trim);
    CHECK_EQUAL(0u, stream.tail_trim);
}

static void other_files_are_not_mp3(void)
{
    Mp3Stream stream;
    load("cbr_128_44k_stereo_tagged");
    memcpy(fixture.bytes, "fLaC", 4);
    memset(fixture.bytes + 4, 0, fixture.size - 4);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    CHECK(!mp3_probe(input, &stream));
}

static void the_signature_spots_tags_and_frame_headers(void)
{
    static const uint8_t TAG[4] = {'I', 'D', '3', 3};
    static const uint8_t FRAME[4] = {0xFF, 0xFB, 0x90, 0x64};
    static const uint8_t FLAC[4] = {'f', 'L', 'a', 'C'};
    static const uint8_t BAD_RATE[4] = {0xFF, 0xFB, 0x9C, 0x64};
    CHECK(mp3_signature(TAG));
    CHECK(mp3_signature(FRAME));
    CHECK(!mp3_signature(FLAC));
    CHECK(!mp3_signature(BAD_RATE));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_tagged_file_skips_both_tags_and_the_info_frame),
        TEST_CASE(a_bare_constant_rate_file_is_timed_from_its_size),
        TEST_CASE(a_variable_rate_file_takes_its_length_from_the_xing_header),
        TEST_CASE(mpeg_2_frames_hold_576_samples),
        TEST_CASE(seeking_a_constant_rate_file_is_proportional),
        TEST_CASE(seeking_with_a_table_follows_it),
        TEST_CASE(a_lame_tag_trims_the_encoder_delay_and_padding),
        TEST_CASE(trimming_leaves_exactly_the_encoded_length),
        TEST_CASE(a_file_without_an_info_frame_is_not_trimmed),
        TEST_CASE(an_unknown_encoder_tag_is_not_trusted),
        TEST_CASE(trims_longer_than_the_track_are_ignored),
        TEST_CASE(other_files_are_not_mp3),
        TEST_CASE(the_signature_spots_tags_and_frame_headers),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    free(fixture.bytes);
    return result;
}
