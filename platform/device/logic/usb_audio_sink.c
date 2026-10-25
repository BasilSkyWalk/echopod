#include "device/usb_audio_sink.h"

enum {
    TARGET_DIVISOR = 50,
    WINDOW_DIVISOR = 250,
    SETTLE_DIVISOR = 1000,
    GUARD_DIVISOR = 50,
    WORDS_PER_FRAME = 2,
    WORD_BYTES = 4
};

void usb_audio_sink_init(UsbAudioSink *sink, uint32_t *ring, uint32_t ring_frames)
{
    *sink = (UsbAudioSink){.ring = ring, .ring_frames = ring_frames};
}

void usb_audio_sink_start(UsbAudioSink *sink, uint32_t rate, uint8_t sample_bytes)
{
    sink->written = 0;
    sink->cleared = 0;
    sink->target = rate / TARGET_DIVISOR;
    sink->guard = rate / GUARD_DIVISOR;
    sink->sample_bytes = sample_bytes;
    sink->playing = false;
    StreamDriftLimits limits = {(int32_t)sink->target, (int32_t)(rate / WINDOW_DIVISOR),
                                (int32_t)(rate / SETTLE_DIVISOR)};
    stream_drift_init(&sink->drift, limits);
}

void usb_audio_sink_set_playing(UsbAudioSink *sink, bool playing)
{
    sink->playing = playing;
}

bool usb_audio_sink_primed(const UsbAudioSink *sink)
{
    return sink->sample_bytes != 0 && sink->written >= sink->target;
}

static void put_frame(UsbAudioSink *sink, uint32_t frame, uint32_t left, uint32_t right)
{
    uint32_t *words = sink->ring + (frame % sink->ring_frames) * WORDS_PER_FRAME;
    words[0] = left;
    words[1] = right;
}

static uint32_t sample_word(const uint8_t *bytes, uint8_t count)
{
    uint32_t word = 0;
    for (uint8_t index = 0; index < count; index++) {
        word |= (uint32_t)bytes[index] << (8 * (WORD_BYTES - count + index));
    }
    return word;
}

static void clear_range(UsbAudioSink *sink, uint32_t frame, uint32_t end)
{
    for (; frame != end; frame++) {
        put_frame(sink, frame, 0, 0);
    }
}

static void clear_ahead(UsbAudioSink *sink)
{
    uint32_t end = sink->written + sink->guard;
    bool ahead = (int32_t)(sink->cleared - sink->written) > 0;
    clear_range(sink, ahead ? sink->cleared : sink->written, end);
    sink->cleared = end;
}

static void realign(UsbAudioSink *sink, uint32_t consumed)
{
    sink->written = consumed + sink->target;
    clear_range(sink, consumed, sink->written);
    sink->cleared = sink->written;
    sink->stats.realigned++;
}

static bool fill_out_of_range(const UsbAudioSink *sink, uint32_t frames, uint32_t consumed)
{
    int32_t fill = (int32_t)(sink->written - consumed);
    return fill < 0 || (uint32_t)fill + frames + 1 + sink->guard > sink->ring_frames;
}

static int32_t adjustment(UsbAudioSink *sink, uint32_t consumed)
{
    if (!sink->playing) {
        return 0;
    }
    int32_t adjust = stream_drift_adjust(&sink->drift, (int32_t)(sink->written - consumed));
    sink->stats.dropped += adjust < 0 ? 1u : 0u;
    sink->stats.repeated += adjust > 0 ? 1u : 0u;
    return adjust;
}

static void write_frames(UsbAudioSink *sink, const uint8_t *bytes, uint32_t frames, int32_t adjust)
{
    uint32_t frame_bytes = 2u * sink->sample_bytes;
    uint32_t kept = adjust < 0 && frames > 1 ? frames - 1 : frames;
    uint32_t total = adjust > 0 ? kept + 1 : kept;
    for (uint32_t index = 0; index < total; index++) {
        const uint8_t *frame = bytes + (index < kept ? index : kept - 1) * frame_bytes;
        put_frame(sink, sink->written++, sample_word(frame, sink->sample_bytes),
                  sample_word(frame + sink->sample_bytes, sink->sample_bytes));
    }
}

void usb_audio_sink_write(UsbAudioSink *sink, const uint8_t *bytes, uint32_t length,
                          uint32_t consumed)
{
    uint32_t frames = sink->sample_bytes == 0 ? 0 : length / (2u * sink->sample_bytes);
    if (frames == 0) {
        return;
    }
    sink->stats.packets++;
    if (fill_out_of_range(sink, frames, consumed)) {
        realign(sink, consumed);
    }
    write_frames(sink, bytes, frames, adjustment(sink, consumed));
    clear_ahead(sink);
}
