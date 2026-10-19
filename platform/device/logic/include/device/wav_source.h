#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"
#include "device/audio_engine.h"
#include "device/flac_source.h"

enum {
    WAV_SOURCE_BLOCK_FRAMES = 4096,
    WAV_SOURCE_RAW_BYTES = 4096
};

typedef struct WavBuffers {
    int32_t left[WAV_SOURCE_BLOCK_FRAMES];
    int32_t right[WAV_SOURCE_BLOCK_FRAMES];
    uint8_t raw[WAV_SOURCE_RAW_BYTES];
} WavBuffers;

typedef struct WavFormat {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits;
    uint16_t frame_bytes;
    uint32_t data_offset;
    uint32_t frames;
} WavFormat;

typedef struct WavSource {
    FlacFileOpener files;
    WavBuffers *buffers;
    FlacInput input;
    WavFormat format;
    uint32_t next_frame;
    bool open;
} WavSource;

void wav_source_init(WavSource *source, FlacFileOpener files, WavBuffers *buffers);
AudioSource wav_source_interface(WavSource *source);
