#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"
#include "device/audio_engine.h"
#include "device/flac_source.h"

enum {
    STREAM_SLOTS = 2,
    STREAM_SLOT_EMPTY = 0,
    STREAM_SLOT_FULL = 1
};

typedef struct StreamSlot {
    uint32_t state;
    uint32_t generation;
    uint32_t result;
    uint32_t first_sample;
    uint32_t samples;
} StreamSlot;

typedef struct StreamControl {
    uint32_t generation;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint32_t min_block;
    uint32_t max_block;
    uint32_t total_samples;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t written;
    uint32_t acked_generation;
    uint32_t released;
    StreamSlot slots[STREAM_SLOTS];
} StreamControl;

typedef struct StreamRegion {
    volatile StreamControl *control;
    int32_t (*samples)[FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK];
    volatile uint8_t *ring;
    uint32_t ring_bytes;
} StreamRegion;

typedef struct Core1Stream {
    StreamRegion region;
    FlacDecoder *decoder;
    uint32_t generation;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t slot;
    bool finished;
    bool starved;
} Core1Stream;

void core1_stream_init(Core1Stream *stream, StreamRegion region, FlacDecoder *decoder);
bool core1_stream_step(Core1Stream *stream);

typedef struct Core1Source {
    StreamRegion region;
    FlacSource flac;
    FlacInput input;
    uint32_t generation;
    uint32_t slot;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t written;
    uint32_t fill_budget;
    uint8_t bits_per_sample;
    uint8_t channels;
    bool streaming;
    bool read_failed;
} Core1Source;

void core1_source_init(Core1Source *source, StreamRegion region, FlacFileOpener files,
                       FlacDecoder *decoder, uint32_t fill_budget);
void core1_source_fill(Core1Source *source);
AudioSource core1_source_interface(Core1Source *source);
