#include "check.h"
#include "device/usb_audio_sink.h"

enum {
    RING_FRAMES = 4000,
    RATE = 48000,
    TARGET = RATE / 50,
    PACKET_FRAMES = 48
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

static void a_full_buffer_while_playing_drops_one_frame_per_packet(void)
{
    UsbAudioSink sink = started_sink(2);
    write_packets(&sink, 40, 0);
    usb_audio_sink_set_playing(&sink, true);
    uint32_t before = sink.written;
    write_packets(&sink, 1, before - TARGET - 400);
    CHECK_EQUAL(before + PACKET_FRAMES - 1, sink.written);
    CHECK_EQUAL(1, sink.stats.dropped);
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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(sixteen_bit_samples_go_to_the_top_of_each_word),
        TEST_CASE(twenty_four_bit_samples_are_left_justified),
        TEST_CASE(a_partial_frame_at_the_end_of_a_packet_is_ignored),
        TEST_CASE(nothing_is_written_while_streaming_is_off),
        TEST_CASE(the_sink_is_primed_after_twenty_milliseconds),
        TEST_CASE(silence_is_kept_ahead_of_the_last_packet),
        TEST_CASE(a_full_buffer_while_playing_drops_one_frame_per_packet),
        TEST_CASE(a_low_buffer_while_playing_repeats_the_last_frame),
        TEST_CASE(no_frames_are_dropped_or_repeated_before_playing),
        TEST_CASE(an_underrun_restarts_twenty_milliseconds_ahead_of_the_output),
        TEST_CASE(an_overflow_also_restarts_near_the_output),
        TEST_CASE(writing_wraps_around_the_end_of_the_ring),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
