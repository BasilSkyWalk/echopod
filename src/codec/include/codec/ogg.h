#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    OGG_OPUS_RATE = 48000,
    OGG_OPUS_PREROLL = 3840,
    OGG_MAX_SEGMENTS = 255,
    OGG_PAGE_HEADER_BYTES = 27,
    OGG_SCAN_BYTES = 512
};

typedef struct OggOpusInfo {
    uint32_t serial;
    uint32_t input_rate;
    uint32_t audio_start;
    uint32_t last_granule;
    uint16_t pre_skip;
    int16_t gain;
    uint8_t channels;
} OggOpusInfo;

typedef enum OggResult {
    OGG_PACKET,
    OGG_END,
    OGG_MISSING,
    OGG_BAD
} OggResult;

typedef struct OggPacket {
    const uint8_t *data;
    uint32_t length;
    uint32_t first_sample;
    uint32_t samples;
} OggPacket;

typedef struct OggReader {
    FlacInput input;
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t serial;
    uint32_t position;
    uint32_t body;
    uint32_t page_end;
    uint32_t next_sample;
    uint32_t packet_length;
    uint32_t segments;
    uint32_t segment;
    bool page_loaded;
    bool entered;
    bool skipping;
    bool adopt_serial;
    uint8_t lacing[OGG_MAX_SEGMENTS];
} OggReader;

bool ogg_signature(const uint8_t *bytes);
bool ogg_opus_probe(FlacInput file, OggOpusInfo *info);
uint32_t ogg_opus_seek(FlacInput file, const OggOpusInfo *info, uint32_t granule);
uint32_t ogg_opus_packet_samples(const uint8_t *packet, uint32_t length);
void ogg_reader_start(OggReader *reader, FlacInput input, uint32_t position, uint32_t serial,
                      uint8_t *buffer, uint32_t capacity);
void ogg_reader_follow_first(OggReader *reader);
OggResult ogg_reader_next(OggReader *reader, OggPacket *packet);
