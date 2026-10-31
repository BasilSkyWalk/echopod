#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"
#include "codec/mp4.h"
#include "device/audio_engine.h"
#include "device/flac_source.h"

enum {
    STREAM_SLOTS = 2,
    STREAM_SLOT_EMPTY = 0,
    STREAM_SLOT_FULL = 1
};

enum {
    STREAM_CODEC_NATIVE = 0,
    STREAM_CODEC_ALAC = 1,
    STREAM_CODEC_AAC = 2,
    STREAM_CODEC_OPUS = 3,
    STREAM_CODECS = 4
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
    uint32_t first_sample;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t written;
    uint32_t acked_generation;
    uint32_t released;
    uint32_t codec;
    uint32_t load_percent;
    uint32_t ring_bytes;
    StreamSlot slots[STREAM_SLOTS];
} StreamControl;

typedef struct StreamRegion {
    volatile StreamControl *control;
    int32_t (*samples)[FLAC_MAX_CHANNELS][FLAC_MAX_BLOCK];
    volatile uint8_t *ring;
    uint32_t ring_bytes;
} StreamRegion;

typedef enum Core1DecodeResult {
    CORE1_DECODED,
    CORE1_STARVED,
    CORE1_FINISHED,
    CORE1_FAILED
} Core1DecodeResult;

typedef struct Core1Block {
    const int32_t *channels[FLAC_MAX_CHANNELS];
    const int16_t *narrow[FLAC_MAX_CHANNELS];
    uint32_t first_sample;
    uint32_t samples;
    uint32_t position;
    uint32_t stride;
} Core1Block;

typedef struct Core1Decoder {
    void (*start)(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                  const bool *starved);
    Core1DecodeResult (*decode)(void *context, Core1Block *block);
    void *context;
} Core1Decoder;

typedef struct Core1Flac {
    FlacDecoder *decoder;
    const bool *starved;
} Core1Flac;

Core1Decoder core1_flac_decoder(Core1Flac *flac, FlacDecoder *decoder);

typedef struct Core1Stream {
    StreamRegion region;
    uint32_t ring_capacity;
    Core1Decoder decoders[STREAM_CODECS];
    Core1Decoder decoder;
    uint32_t channels;
    uint32_t generation;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t slot;
    uint32_t (*ticks)(void);
    uint32_t tick_mask;
    uint32_t load_window;
    uint32_t last_tick;
    uint32_t busy_ticks;
    uint32_t wall_ticks;
    bool finished;
    bool starved;
} Core1Stream;

void core1_stream_init(Core1Stream *stream, StreamRegion region, Core1Decoder decoder);
void core1_stream_set_decoder(Core1Stream *stream, uint32_t codec, Core1Decoder decoder);
void core1_stream_set_clock(Core1Stream *stream, uint32_t (*ticks)(void), uint32_t mask,
                            uint32_t window);
bool core1_stream_step(Core1Stream *stream);

typedef struct Core1Source {
    StreamRegion region;
    uint32_t ring_capacity;
    FlacSource flac;
    Mp4Stream mp4;
    FlacInput input;
    uint32_t generation;
    uint32_t slot;
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t written;
    uint32_t lead_trim;
    uint32_t play_from;
    uint32_t play_end;
    uint32_t fill_budget;
    uint8_t bits_per_sample;
    uint8_t channels;
    bool streaming;
    bool read_failed;
    bool file_open;
} Core1Source;

void core1_source_init(Core1Source *source, StreamRegion region, FlacFileOpener files,
                       FlacDecoder *decoder, uint32_t fill_budget);
void core1_source_fill(Core1Source *source);
AudioSource core1_source_interface(Core1Source *source);
AudioSource core1_source_mp3_interface(Core1Source *source);
AudioSource core1_source_mp4_interface(Core1Source *source);
AudioSource core1_source_ogg_interface(Core1Source *source);
