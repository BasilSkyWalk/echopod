#include "device/usb_audio_sink.h"

enum {
    TARGET_DIVISOR = 50,
    WINDOW_DIVISOR = 250,
    SETTLE_DIVISOR = 1000,
    CORRECTION_SPACING = 7,
    GUARD_DIVISOR = 50,
    WORDS_PER_FRAME = 2,
    WORD_BYTES = 4,
    GAIN_BITS = 24,
    HALF_DECIBELS_PER_DECADE = 40,
    RAMP_DIVISOR = 128
};

static const uint32_t HALF_DECIBEL_GAINS[HALF_DECIBELS_PER_DECADE] = {
    16777216u, 15838713u, 14952709u, 14116268u, 13326616u, 12581137u, 11877359u, 11212950u,
    10585708u, 9993552u,  9434522u,  8906763u,  8408526u,  7938161u,  7494107u,  7074893u,
    6679130u,  6305505u,  5952781u,  5619788u,  5305422u,  5008641u,  4728462u,  4463956u,
    4214246u,  3978505u,  3755951u,  3545846u,  3347495u,  3160239u,  2983458u,  2816566u,
    2659010u,  2510267u,  2369845u,  2237278u,  2112126u,  1993976u,  1882435u,  1777133u};

void usb_audio_sink_init(UsbAudioSink *sink, uint32_t *ring, uint32_t ring_frames)
{
    *sink = (UsbAudioSink){
        .ring = ring, .ring_frames = ring_frames, .gain_target = USB_AUDIO_SINK_UNITY_GAIN};
}

void usb_audio_sink_start(UsbAudioSink *sink, uint32_t rate, uint8_t sample_bytes)
{
    sink->written = 0;
    sink->cleared = 0;
    sink->target = rate / TARGET_DIVISOR;
    sink->guard = rate / GUARD_DIVISOR;
    sink->sample_bytes = sample_bytes;
    sink->playing = false;
    sink->frame_known = false;
    sink->gain = 0;
    StreamDriftLimits limits = {(int32_t)sink->target, (int32_t)(rate / WINDOW_DIVISOR),
                                (int32_t)(rate / SETTLE_DIVISOR), CORRECTION_SPACING};
    stream_drift_init(&sink->drift, limits);
}

void usb_audio_sink_set_playing(UsbAudioSink *sink, bool playing)
{
    sink->playing = playing;
}

int32_t usb_audio_sink_gain_for(uint8_t half_decibels)
{
    if (half_decibels == USB_AUDIO_SINK_MUTED) {
        return 0;
    }
    uint32_t gain = HALF_DECIBEL_GAINS[half_decibels % HALF_DECIBELS_PER_DECADE];
    for (uint32_t decade = half_decibels / HALF_DECIBELS_PER_DECADE; decade > 0; decade--) {
        gain /= 10;
    }
    return (int32_t)gain;
}

void usb_audio_sink_set_attenuation(UsbAudioSink *sink, uint8_t half_decibels)
{
    sink->gain_target = usb_audio_sink_gain_for(half_decibels);
}

void usb_audio_sink_count_frame(UsbAudioSink *sink, uint16_t frame)
{
    uint16_t step = (uint16_t)(frame - sink->last_frame) & USB_AUDIO_SINK_FRAME_MASK;
    if (sink->frame_known && sink->playing && step > 1) {
        sink->stats.missed += (uint32_t)step - 1u;
    }
    sink->frame_known = true;
    sink->last_frame = frame;
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

static void ramp_gain(UsbAudioSink *sink)
{
    int32_t distance = sink->gain_target - sink->gain;
    if (distance > -RAMP_DIVISOR && distance < RAMP_DIVISOR) {
        sink->gain = sink->gain_target;
    } else {
        sink->gain += distance / RAMP_DIVISOR;
    }
}

static uint32_t scaled(uint32_t word, int32_t gain)
{
    if (gain == USB_AUDIO_SINK_UNITY_GAIN) {
        return word;
    }
    return (uint32_t)(int32_t)(((int64_t)(int32_t)word * gain) >> GAIN_BITS);
}

static void write_frames(UsbAudioSink *sink, const uint8_t *bytes, uint32_t frames, int32_t adjust)
{
    uint32_t frame_bytes = 2u * sink->sample_bytes;
    uint32_t kept = adjust < 0 && frames > 1 ? frames - 1 : frames;
    uint32_t total = adjust > 0 ? kept + 1 : kept;
    for (uint32_t index = 0; index < total; index++) {
        const uint8_t *frame = bytes + (index < kept ? index : kept - 1) * frame_bytes;
        ramp_gain(sink);
        put_frame(sink, sink->written++, scaled(sample_word(frame, sink->sample_bytes), sink->gain),
                  scaled(sample_word(frame + sink->sample_bytes, sink->sample_bytes), sink->gain));
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
