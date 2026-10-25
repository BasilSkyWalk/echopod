#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/stream_drift.h"

typedef struct UsbAudioSinkStats {
    uint32_t packets;
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
    StreamDrift drift;
    UsbAudioSinkStats stats;
} UsbAudioSink;

void usb_audio_sink_init(UsbAudioSink *sink, uint32_t *ring, uint32_t ring_frames);
void usb_audio_sink_start(UsbAudioSink *sink, uint32_t rate, uint8_t sample_bytes);
void usb_audio_sink_set_playing(UsbAudioSink *sink, bool playing);
void usb_audio_sink_write(UsbAudioSink *sink, const uint8_t *bytes, uint32_t length,
                          uint32_t consumed);
bool usb_audio_sink_primed(const UsbAudioSink *sink);
