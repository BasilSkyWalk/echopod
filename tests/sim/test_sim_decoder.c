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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_flac_reports_its_stream_and_average_bitrate),
        TEST_CASE(a_mono_flac_reports_one_channel),
        TEST_CASE(a_closed_decoder_has_no_format),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
