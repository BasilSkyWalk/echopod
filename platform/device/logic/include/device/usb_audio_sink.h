#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/stream_drift.h"

enum {
    USB_AUDIO_SINK_MUTED = 0xFF,
    USB_AUDIO_SINK_UNITY_GAIN = 1 << 24,
    USB_AUDIO_SINK_FRAME_MASK = 0x7FF
};

typedef struct UsbAudioSinkStats {
    uint32_t packets;
    uint32_t missed;
    uint32_t dropped;
    uint32_t repeated;
    uint32_t realigned;
} UsbAudioSinkStats;

typedef struct UsbAudioSink {
    uint32_t *ring;
    uint32_t ring_frames;
    uint32_t written;
    uint32_t cleared;
    uint32_t target;
    uint32_t guard;
    uint8_t sample_bytes;
    bool playing;
    bool frame_known;
    uint16_t last_frame;
    int32_t gain;
    int32_t gain_target;
    StreamDrift drift;
    UsbAudioSinkStats stats;
} UsbAudioSink;

void usb_audio_sink_init(UsbAudioSink *sink, uint32_t *ring, uint32_t ring_frames);
void usb_audio_sink_start(UsbAudioSink *sink, uint32_t rate, uint8_t sample_bytes);
void usb_audio_sink_set_playing(UsbAudioSink *sink, bool playing);
void usb_audio_sink_set_attenuation(UsbAudioSink *sink, uint8_t half_decibels);
int32_t usb_audio_sink_gain_for(uint8_t half_decibels);
void usb_audio_sink_write(UsbAudioSink *sink, const uint8_t *bytes, uint32_t length,
                          uint32_t consumed);
void usb_audio_sink_count_frame(UsbAudioSink *sink, uint16_t frame);
bool usb_audio_sink_primed(const UsbAudioSink *sink);
