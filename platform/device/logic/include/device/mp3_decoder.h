#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"
#include "mad.h"

enum {
    MP3_INPUT_BYTES = 4096,
    MP3_OUTPUT_BITS = 24,
    MP3_MAX_FRAME_SAMPLES = 1152
};

typedef enum Mp3Result {
    MP3_DECODED,
    MP3_STARVED,
    MP3_FINISHED,
    MP3_FAILED
} Mp3Result;

typedef struct Mp3Decoder {
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    unsigned char main_data[MAD_BUFFER_MDLEN];
    mad_fixed_t overlap[2][32][18];
    unsigned char input[MP3_INPUT_BYTES + MAD_BUFFER_GUARD];
    FlacInput source;
    uint32_t buffer_start;
    uint32_t buffered;
    bool refill;
    bool guarded;
} Mp3Decoder;

void mp3_decoder_start(Mp3Decoder *decoder, FlacInput source);
Mp3Result mp3_decode(Mp3Decoder *decoder);
uint32_t mp3_decoder_position(const Mp3Decoder *decoder);
uint32_t mp3_decoder_samples(const Mp3Decoder *decoder);
uint8_t mp3_decoder_channels(const Mp3Decoder *decoder);
const int32_t *mp3_decoder_channel(const Mp3Decoder *decoder, int channel);
