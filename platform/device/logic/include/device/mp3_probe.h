#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    MP3_TOC_ENTRIES = 100,
    MP3_SIGNATURE_BYTES = 4
};

typedef struct Mp3Stream {
    uint32_t sample_rate;
    uint8_t channels;
    uint16_t samples_per_frame;
    uint32_t total_samples;
    uint32_t audio_start;
    uint32_t audio_end;
    bool has_toc;
    uint8_t toc[MP3_TOC_ENTRIES];
} Mp3Stream;

bool mp3_signature(const uint8_t bytes[MP3_SIGNATURE_BYTES]);
bool mp3_probe(FlacInput input, Mp3Stream *stream);
uint32_t mp3_seek_offset(const Mp3Stream *stream, uint32_t sample);
