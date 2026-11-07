#include "check.h"
#include "device/usb_audio_sink.h"

enum {
    RING_FRAMES = 4000,
    RATE = 48000,
    TARGET = RATE / 50,
    PACKET_FRAMES = 48,
    UNITY = USB_AUDIO_SINK_UNITY_GAIN,
    RAMP_FRAMES = 2000,
    SIX_DECIBELS = 12,
    TWENTY_DECIBELS = 40,
    FULL_SCALE = 0x7FFFFF00
};

static uint32_t ring[RING_FRAMES * 2];

static UsbAudioSink started_sink(uint8_t sample_bytes)
{
    UsbAudioSink sink;
    for (uint32_t index = 0; index < RING_FRAMES * 2; index++) {
        ring[index] = 0xDEADBEEFu;
    }
    usb_audio_sink_init(&sink, ring, RING_FRAMES);
    usb_audio_sink_start(&sink, RATE, sample_bytes);
    sink.gain = UNITY;
    return sink;
}

static void packet_16(uint8_t *bytes, uint32_t frames, uint8_t first)
{
    for (uint32_t frame = 0; frame < frames; frame++) {
        for (uint32_t byte = 0; byte < 4; byte++) {
            bytes[frame * 4 + byte] = (uint8_t)(first + frame);
        }
    }
}

static void write_packets(UsbAudioSink *sink, uint32_t count, uint32_t consumed)
{
    uint8_t bytes[PACKET_FRAMES * 4];
    packet_16(bytes, PACKET_FRAMES, 1);
    for (uint32_t index = 0; index < count; index++) {
        usb_audio_sink_write(sink, bytes, sizeof bytes, consumed);
    }
}

static void write_full_scale_packets(UsbAudioSink *sink, uint32_t count)
{
    uint8_t bytes[PACKET_FRAMES * 6];
    for (uint32_t frame = 0; frame < PACKET_FRAMES; frame++) {
        for (uint32_t channel = 0; channel < 2; channel++) {
            uint8_t *sample = bytes + frame * 6 + channel * 3;
            sample[0] = 0xFF;
            sample[1] = 0xFF;
            sample[2] = 0x7F;
        }
    }
    for (uint32_t index = 0; index < count; index++) {
        usb_audio_sink_write(sink, bytes, sizeof bytes, 0);
    }
}

static int32_t frame_left(uint32_t frame)
{
    return (int32_t)ring[2 * (frame % RING_FRAMES)];
}

static void sixteen_bit_samples_go_to_the_top_of_each_word(void)
{
    UsbAudioSink sink = started_sink(2);
    static const uint8_t bytes[] = {0x34, 0x12, 0xCD, 0xAB};
    usb_audio_sink_write(&sink, bytes, sizeof bytes, 0);
    CHECK_EQUAL(0x12340000u, ring[0]);
    CHECK_EQUAL(0xABCD0000u, ring[1]);
    CHECK_EQUAL(1, sink.written);
}

static void twenty_four_bit_samples_are_left_justified(void)
{
    UsbAudioSink sink = started_sink(3);
    static const uint8_t bytes[] = {0x56, 0x34, 0x12, 0xEF, 0xCD, 0xAB};
    usb_audio_sink_write(&sink, bytes, sizeof bytes, 0);
    CHECK_EQUAL(0x12345600u, ring[0]);
    CHECK_EQUAL(0xABCDEF00u, ring[1]);
}

static void a_partial_frame_at_the_end_of_a_packet_is_ignored(void)
{
    UsbAudioSink sink = started_sink(3);
    static const uint8_t bytes[8] = {0};
    usb_audio_sink_write(&sink, bytes, sizeof bytes, 0);
    CHECK_EQUAL(1, sink.written);
}

static void nothing_is_written_while_streaming_is_off(void)
{
    UsbAudioSink sink = started_sink(0);
    static const uint8_t bytes[8] = {0};
    usb_audio_sink_write(&sink, bytes, sizeof bytes, 0);
    CHECK_EQUAL(0, sink.written);
    CHECK(!usb_audio_sink_primed(&sink));
}

static void the_sink_is_primed_after_twenty_milliseconds(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, TARGET / PACKET_FRAMES - 1, 0);
    CHECK(!usb_audio_sink_primed(&sink));
    write_packets(&sink, 1, 0);
    CHECK(usb_audio_sink_primed(&sink));
}

static void silence_is_kept_ahead_of_the_last_packet(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 1, 0);
    CHECK_EQUAL(0u, ring[2 * PACKET_FRAMES]);
    CHECK_EQUAL(0u, ring[2 * (PACKET_FRAMES + TARGET) - 1]);
    CHECK_EQUAL(0xDEADBEEFu, ring[2 * (PACKET_FRAMES + TARGET)]);
}

static void a_full_buffer_while_playing_drops_a_frame(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 40, 0);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t before = sink.written;
    write_packets(&sink, 1, before - TARGET - 400);
    CHECK_EQUAL(before + PACKET_FRAMES - 1, sink.written);
    CHECK_EQUAL(1, sink.stats.dropped);
}

static void corrections_are_spread_one_in_eight_packets(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 40, 0);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t consumed = sink.written - TARGET - 400;
    for (int packet = 0; packet < 16; packet++) {
        write_packets(&sink, 1, consumed);
        consumed += PACKET_FRAMES;
    }
    CHECK_EQUAL(2, sink.stats.dropped);
}

static void a_low_buffer_while_playing_repeats_the_last_frame(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 10, 0);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t before = sink.written;
    uint8_t bytes[PACKET_FRAMES * 4];
    packet_16(bytes, PACKET_FRAMES, 1);
    usb_audio_sink_write(&sink, bytes, sizeof bytes, before - TARGET + 400);
    CHECK_EQUAL(before + PACKET_FRAMES + 1, sink.written);
    CHECK_EQUAL(ring[2 * (before + PACKET_FRAMES - 1)], ring[2 * (before + PACKET_FRAMES)]);
    CHECK_EQUAL(1, sink.stats.repeated);
}

static void no_frames_are_dropped_or_repeated_before_playing(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 3, 0);
    CHECK_EQUAL(3 * PACKET_FRAMES, sink.written);
}

static void an_underrun_restarts_twenty_milliseconds_ahead_of_the_output(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 30, 0);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t consumed = sink.written + 100;
    write_packets(&sink, 1, consumed);
    CHECK_EQUAL(1, sink.stats.realigned);
    CHECK_EQUAL(consumed + TARGET + PACKET_FRAMES, sink.written);
    CHECK_EQUAL(0u, ring[2 * consumed]);
}

static void an_overflow_also_restarts_near_the_output(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, RING_FRAMES / PACKET_FRAMES, 0);
    CHECK(sink.stats.realigned > 0);
    CHECK(sink.written <= RING_FRAMES);
}

static void writing_wraps_around_the_end_of_the_ring(void)
{
    UsbAudioSink sink = started_sink(2);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t consumed = 0;
    for (int packet = 0; packet < 200; packet++) {
        write_packets(&sink, 1, consumed);
        consumed += PACKET_FRAMES;
    }
    CHECK_EQUAL(0, sink.stats.realigned);
    CHECK(sink.written > RING_FRAMES);
    CHECK_EQUAL(0x30300000u, ring[2 * ((sink.written - 1) % RING_FRAMES)]);
}

static void a_gap_in_the_packets_is_made_up_by_repeats_until_the_fill_is_back(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, TARGET / PACKET_FRAMES, 0);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t consumed = 0;
    for (int packet = 0; packet < 100; packet++) {
        consumed += PACKET_FRAMES;
        write_packets(&sink, 1, consumed);
    }
    CHECK_EQUAL(0, sink.stats.repeated);
    consumed += 5 * PACKET_FRAMES;
    uint32_t written_before = sink.written;
    for (int packet = 0; packet < 2000; packet++) {
        consumed += PACKET_FRAMES;
        write_packets(&sink, 1, consumed);
    }
    CHECK_EQUAL(0, sink.stats.dropped);
    CHECK(sink.stats.repeated > 3 * PACKET_FRAMES);
    CHECK_EQUAL(0, sink.stats.realigned);
    CHECK_EQUAL(written_before + 2000 * PACKET_FRAMES + sink.stats.repeated, sink.written);
    CHECK((int32_t)(sink.written - consumed) >= TARGET - RATE / 1000);
}

static void the_gain_table_follows_half_decibel_steps(void)
{
    CHECK_EQUAL(UNITY, usb_audio_sink_gain_for(0));
    CHECK(usb_audio_sink_gain_for(SIX_DECIBELS) > UNITY / 2 - UNITY / 200);
    CHECK(usb_audio_sink_gain_for(SIX_DECIBELS) < UNITY / 2 + UNITY / 200);
    CHECK(usb_audio_sink_gain_for(TWENTY_DECIBELS) >= UNITY / 10);
    CHECK(usb_audio_sink_gain_for(TWENTY_DECIBELS) <= UNITY / 10 + 1);
    CHECK(usb_audio_sink_gain_for(2 * TWENTY_DECIBELS) < UNITY / 99);
    CHECK(usb_audio_sink_gain_for(2 * TWENTY_DECIBELS) > UNITY / 101);
    CHECK(usb_audio_sink_gain_for(1) < usb_audio_sink_gain_for(0));
    CHECK(usb_audio_sink_gain_for(TWENTY_DECIBELS) < usb_audio_sink_gain_for(TWENTY_DECIBELS - 1));
    CHECK_EQUAL(0, usb_audio_sink_gain_for(USB_AUDIO_SINK_MUTED));
}

static void a_start_fades_in_from_silence(void)
{
    UsbAudioSink sink = started_sink(3);
    usb_audio_sink_start(&sink, RATE, 3);
    CHECK_EQUAL(0, sink.gain);
    write_full_scale_packets(&sink, RAMP_FRAMES / PACKET_FRAMES);
    CHECK(frame_left(0) < FULL_SCALE / 64);
    CHECK(frame_left(0) > 0);
    CHECK(frame_left(PACKET_FRAMES) > frame_left(0));
    CHECK_EQUAL(FULL_SCALE, frame_left(sink.written - 1));
    CHECK_EQUAL(UNITY, sink.gain);
}

static void attenuation_scales_the_samples(void)
{
    UsbAudioSink sink = started_sink(3);
    usb_audio_sink_set_attenuation(&sink, SIX_DECIBELS);
    write_full_scale_packets(&sink, RAMP_FRAMES / PACKET_FRAMES);
    int32_t half = frame_left(sink.written - 1);
    CHECK(half > FULL_SCALE / 2 - FULL_SCALE / 200);
    CHECK(half < FULL_SCALE / 2 + FULL_SCALE / 200);
    CHECK_EQUAL((uint32_t)half, ring[2 * (sink.written - 1) + 1]);
}

static void a_volume_change_ramps_instead_of_jumping(void)
{
    UsbAudioSink sink = started_sink(3);
    write_full_scale_packets(&sink, 1);
    usb_audio_sink_set_attenuation(&sink, TWENTY_DECIBELS);
    write_full_scale_packets(&sink, 1);
    int32_t first = frame_left(PACKET_FRAMES);
    int32_t last = frame_left(2 * PACKET_FRAMES - 1);
    CHECK(first < FULL_SCALE);
    CHECK(first > FULL_SCALE / 10 * 9);
    CHECK(last < first);
    CHECK(last > FULL_SCALE / 10);
    write_full_scale_packets(&sink, RAMP_FRAMES / PACKET_FRAMES);
    CHECK_EQUAL(usb_audio_sink_gain_for(TWENTY_DECIBELS), sink.gain);
}

static void mute_fades_to_silence(void)
{
    UsbAudioSink sink = started_sink(3);
    usb_audio_sink_set_attenuation(&sink, USB_AUDIO_SINK_MUTED);
    write_full_scale_packets(&sink, RAMP_FRAMES / PACKET_FRAMES);
    CHECK_EQUAL(0, sink.gain);
    CHECK_EQUAL(0u, ring[2 * (RAMP_FRAMES - 1)]);
    CHECK(frame_left(0) > 0);
}

static void negative_samples_scale_the_same_way(void)
{
    UsbAudioSink sink = started_sink(2);
    usb_audio_sink_set_attenuation(&sink, TWENTY_DECIBELS);
    sink.gain = sink.gain_target;
    static const uint8_t bytes[] = {0x00, 0x80, 0x00, 0x80};
    usb_audio_sink_write(&sink, bytes, sizeof bytes, 0);
    int32_t left = frame_left(0);
    int32_t tenth = -(FULL_SCALE / 10);
    CHECK(left < 0);
    CHECK(left > tenth - 0x10000);
    CHECK(left < tenth + 0x10000);
}

static void only_gaps_in_the_frame_numbers_while_playing_count_as_missed(void)
{
    UsbAudioSink sink = started_sink(2);
    usb_audio_sink_count_frame(&sink, 10);
    usb_audio_sink_count_frame(&sink, 14);
    CHECK_EQUAL(0, sink.stats.missed);
    usb_audio_sink_set_playing(&sink, true);
    usb_audio_sink_count_frame(&sink, 15);
    CHECK_EQUAL(0, sink.stats.missed);
    usb_audio_sink_count_frame(&sink, 18);
    CHECK_EQUAL(2, sink.stats.missed);
    usb_audio_sink_count_frame(&sink, 18);
    CHECK_EQUAL(2, sink.stats.missed);
}

static void full_speed_frame_numbers_wrap_after_2047_without_a_miss(void)
{
    UsbAudioSink sink = started_sink(2);
    usb_audio_sink_set_playing(&sink, true);
    for (uint16_t frame = 2040; frame < 2060; frame++) {
        usb_audio_sink_count_frame(&sink, frame & 0x7FF);
    }
    CHECK_EQUAL(0, sink.stats.missed);
    usb_audio_sink_count_frame(&sink, 13);
    CHECK_EQUAL(1, sink.stats.missed);
}

static void a_restart_forgets_the_last_frame_number(void)
{
    UsbAudioSink sink = started_sink(2);
    usb_audio_sink_set_playing(&sink, true);
    usb_audio_sink_count_frame(&sink, 5);
    usb_audio_sink_start(&sink, RATE, 2);
    usb_audio_sink_set_playing(&sink, true);
    usb_audio_sink_count_frame(&sink, 900);
    CHECK_EQUAL(0, sink.stats.missed);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(sixteen_bit_samples_go_to_the_top_of_each_word),
        TEST_CASE(twenty_four_bit_samples_are_left_justified),
        TEST_CASE(a_partial_frame_at_the_end_of_a_packet_is_ignored),
        TEST_CASE(nothing_is_written_while_streaming_is_off),
        TEST_CASE(the_sink_is_primed_after_twenty_milliseconds),
        TEST_CASE(silence_is_kept_ahead_of_the_last_packet),
        TEST_CASE(a_full_buffer_while_playing_drops_a_frame),
        TEST_CASE(corrections_are_spread_one_in_eight_packets),
        TEST_CASE(a_low_buffer_while_playing_repeats_the_last_frame),
        TEST_CASE(no_frames_are_dropped_or_repeated_before_playing),
        TEST_CASE(an_underrun_restarts_twenty_milliseconds_ahead_of_the_output),
        TEST_CASE(an_overflow_also_restarts_near_the_output),
        TEST_CASE(writing_wraps_around_the_end_of_the_ring),
        TEST_CASE(a_gap_in_the_packets_is_made_up_by_repeats_until_the_fill_is_back),
        TEST_CASE(the_gain_table_follows_half_decibel_steps),
        TEST_CASE(a_start_fades_in_from_silence),
        TEST_CASE(attenuation_scales_the_samples),
        TEST_CASE(a_volume_change_ramps_instead_of_jumping),
        TEST_CASE(mute_fades_to_silence),
        TEST_CASE(negative_samples_scale_the_same_way),
        TEST_CASE(only_gaps_in_the_frame_numbers_while_playing_count_as_missed),
        TEST_CASE(full_speed_frame_numbers_wrap_after_2047_without_a_miss),
        TEST_CASE(a_restart_forgets_the_last_frame_number),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
