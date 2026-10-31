#include <math.h>

#include "check.h"
#include "sim_decoder.h"

static const char HI_RES_PATH[] = ECHO_FIXTURE_DIR "/flac/lpc_24_96k_stereo.flac";
static const char MONO_PATH[] = ECHO_FIXTURE_DIR "/flac/fixed_16_mono.flac";

static void a_flac_reports_its_stream_and_average_bitrate(void)
{
    SimDecoder decoder;
    CHECK(sim_decoder_open(&decoder, HI_RES_PATH));
    HalAudioFormat format = sim_decoder_format(&decoder);
    CHECK_EQUAL(HAL_AUDIO_CODEC_FLAC, format.codec);
    CHECK_EQUAL(96000, format.sample_rate);
    CHECK_EQUAL(24, format.bits_per_sample);
    CHECK_EQUAL(2, format.channels);
    CHECK_EQUAL(3917, format.bitrate_kbps);
    sim_decoder_close(&decoder);
}

static void a_mono_flac_reports_one_channel(void)
{
    SimDecoder decoder;
    CHECK(sim_decoder_open(&decoder, MONO_PATH));
    HalAudioFormat format = sim_decoder_format(&decoder);
    CHECK_EQUAL(44100, format.sample_rate);
    CHECK_EQUAL(16, format.bits_per_sample);
    CHECK_EQUAL(1, format.channels);
    sim_decoder_close(&decoder);
}

static void a_closed_decoder_has_no_format(void)
{
    SimDecoder decoder;
    CHECK(!sim_decoder_open(&decoder, ECHO_FIXTURE_DIR "/flac/missing.flac"));
    CHECK_EQUAL(HAL_AUDIO_CODEC_UNKNOWN, sim_decoder_format(&decoder).codec);
}

static void an_alac_track_plays_and_seeks_exactly(void)
{
    SimDecoder decoder;
    CHECK(sim_decoder_open(&decoder, ECHO_FIXTURE_DIR "/alac/stereo_16_44k.m4a"));
    HalAudioFormat format = sim_decoder_format(&decoder);
    CHECK_EQUAL(HAL_AUDIO_CODEC_ALAC, format.codec);
    CHECK_EQUAL(44100, format.sample_rate);
    CHECK_EQUAL(16, format.bits_per_sample);
    CHECK_EQUAL(2, format.channels);
    CHECK_EQUAL(10000, decoder.total_frames);
    FILE *file = fopen(ECHO_FIXTURE_DIR "/alac/stereo_16_44k.pcm", "rb");
    static int32_t expected[10000][2];
    CHECK(file != NULL && fread(expected, sizeof expected, 1, file) == 1);
    if (file != NULL) {
        fclose(file);
    }
    static float frames[1000 * 2];
    uint64_t total = 0;
    bool same = true;
    uint64_t read = 0;
    while ((read = sim_decoder_read_stereo(&decoder, frames, 1000)) > 0) {
        for (uint64_t index = 0; index < read; index++) {
            same = same && frames[2 * index] * 32768.0f == (float)expected[total + index][0] &&
                   frames[2 * index + 1] * 32768.0f == (float)expected[total + index][1];
        }
        total += read;
    }
    CHECK(same);
    CHECK_EQUAL(10000, total);
    CHECK(sim_decoder_seek(&decoder, 6000));
    CHECK_EQUAL(1000, sim_decoder_read_stereo(&decoder, frames, 1000));
    CHECK(frames[0] * 32768.0f == (float)expected[6000][0]);
    CHECK(frames[1999] * 32768.0f == (float)expected[6999][1]);
    sim_decoder_close(&decoder);
}

static void an_aac_track_plays_and_seeks_like_ffmpeg_decodes_it(void)
{
    SimDecoder decoder;
    CHECK(sim_decoder_open(&decoder, ECHO_FIXTURE_DIR "/aac/stereo_44k.m4a"));
    HalAudioFormat format = sim_decoder_format(&decoder);
    CHECK_EQUAL(HAL_AUDIO_CODEC_AAC, format.codec);
    CHECK_EQUAL(44100, format.sample_rate);
    CHECK_EQUAL(0, format.bits_per_sample);
    CHECK_EQUAL(12000, decoder.total_frames);
    CHECK(format.bitrate_kbps > 60 && format.bitrate_kbps < 200);
    FILE *file = fopen(ECHO_FIXTURE_DIR "/aac/stereo_44k.pcm", "rb");
    static int16_t expected[12288][2];
    CHECK(file != NULL && fread(expected, sizeof expected, 1, file) == 1);
    if (file != NULL) {
        fclose(file);
    }
    static float frames[1000 * 2];
    uint64_t total = 0;
    bool close = true;
    uint64_t read = 0;
    while ((read = sim_decoder_read_stereo(&decoder, frames, 1000)) > 0) {
        for (uint64_t index = 0; index < 2 * read; index++) {
            float want = (float)expected[total + index / 2][index % 2];
            close = close && fabsf(frames[index] * 32768.0f - want) <= 2.0f;
        }
        total += read;
    }
    CHECK(close);
    CHECK_EQUAL(12000, total);
    CHECK(sim_decoder_seek(&decoder, 7000));
    CHECK_EQUAL(1000, sim_decoder_read_stereo(&decoder, frames, 1000));
    CHECK(fabsf(frames[0] * 32768.0f - (float)expected[7000][0]) <= 2.0f);
    CHECK(fabsf(frames[1999] * 32768.0f - (float)expected[7999][1]) <= 2.0f);
    sim_decoder_close(&decoder);
}

static void an_opus_track_plays_and_seeks_like_libopus_decodes_it(void)
{
    SimDecoder decoder;
    CHECK(sim_decoder_open(&decoder, ECHO_FIXTURE_DIR "/opus/stereo_small_pages.opus"));
    HalAudioFormat format = sim_decoder_format(&decoder);
    CHECK_EQUAL(HAL_AUDIO_CODEC_OPUS, format.codec);
    CHECK_EQUAL(48000, format.sample_rate);
    CHECK_EQUAL(0, format.bits_per_sample);
    CHECK_EQUAL(19200, decoder.total_frames);
    FILE *file = fopen(ECHO_FIXTURE_DIR "/opus/stereo_small_pages.pcm", "rb");
    static int16_t expected[19200][2];
    CHECK(file != NULL && fread(expected, sizeof expected, 1, file) == 1);
    if (file != NULL) {
        fclose(file);
    }
    static float frames[1000 * 2];
    uint64_t total = 0;
    bool close = true;
    uint64_t read = 0;
    while ((read = sim_decoder_read_stereo(&decoder, frames, 1000)) > 0) {
        for (uint64_t index = 0; index < 2 * read; index++) {
            float want = (float)expected[total + index / 2][index % 2];
            close = close && fabsf(frames[index] * 32768.0f - want) <= 16.0f;
        }
        total += read;
    }
    CHECK(close);
    CHECK_EQUAL(19200, total);
    CHECK(sim_decoder_seek(&decoder, 15000));
    CHECK_EQUAL(1000, sim_decoder_read_stereo(&decoder, frames, 1000));
    CHECK(fabsf(frames[0] * 32768.0f - (float)expected[15000][0]) <= 96.0f);
    CHECK(fabsf(frames[1999] * 32768.0f - (float)expected[15999][1]) <= 96.0f);
    sim_decoder_close(&decoder);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(an_alac_track_plays_and_seeks_exactly),
        TEST_CASE(an_aac_track_plays_and_seeks_like_ffmpeg_decodes_it),
        TEST_CASE(an_opus_track_plays_and_seeks_like_libopus_decodes_it),
        TEST_CASE(a_flac_reports_its_stream_and_average_bitrate),
        TEST_CASE(a_mono_flac_reports_one_channel),
        TEST_CASE(a_closed_decoder_has_no_format),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
