#include "device/core1_alac.h"

#include "codec/mp4.h"

static void start_alac(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                       const bool *starved)
{
    Core1Alac *alac = context;
    (void)info;
    alac->input = input;
    alac->starved = starved;
    alac->position = 0;
    alac->next_sample = first_sample;
    alac->configured = false;
}

static bool read_record(Core1Alac *alac, uint32_t *start, uint32_t *length)
{
    uint8_t bytes[MP4_LENGTH_BYTES];
    FlacInput input = alac->input;
    if (!input.read(input.context, alac->position, bytes, sizeof bytes)) {
        return false;
    }
    *start = alac->position + MP4_LENGTH_BYTES;
    *length = mp4_record_length(bytes);
    if (*length == 0 || *start > input.size || *length > input.size - *start) {
        return false;
    }
    uint8_t last = 0;
    return input.read(input.context, *start + *length - 1, &last, 1);
}

static bool configure(Core1Alac *alac, uint32_t start, uint32_t length)
{
    uint8_t bytes[ALAC_CONFIG_BYTES];
    AlacConfig config;
    if (length < ALAC_CONFIG_BYTES ||
        !alac->input.read(alac->input.context, start, bytes, sizeof bytes) ||
        !alac_parse_config(bytes, sizeof bytes, &config)) {
        return false;
    }
    alac_start(alac->decoder, config);
    alac->configured = true;
    return true;
}

static Core1DecodeResult decode_alac(void *context, Core1Block *block)
{
    Core1Alac *alac = context;
    for (;;) {
        if (alac->position >= alac->input.size) {
            return CORE1_FINISHED;
        }
        uint32_t start = 0;
        uint32_t length = 0;
        if (!read_record(alac, &start, &length)) {
            return *alac->starved ? CORE1_STARVED : CORE1_FAILED;
        }
        if (!alac->configured) {
            if (!configure(alac, start, length)) {
                return CORE1_FAILED;
            }
            alac->position = start + length;
            continue;
        }
        AlacResult result = alac_decode_frame(alac->decoder, alac->input, start, length);
        if (*alac->starved) {
            return CORE1_STARVED;
        }
        if (result != ALAC_OK) {
            return CORE1_FAILED;
        }
        AlacDecoder *decoder = alac->decoder;
        for (int channel = 0; channel < FLAC_MAX_CHANNELS; channel++) {
            block->channels[channel] =
                alac_channel(decoder, channel < decoder->config.channels ? channel : 0);
        }
        alac->position = start + length;
        block->first_sample = alac->next_sample;
        block->samples = decoder->block_samples;
        block->position = alac->position;
        alac->next_sample += decoder->block_samples;
        return CORE1_DECODED;
    }
}

Core1Decoder core1_alac_decoder(Core1Alac *alac, AlacDecoder *decoder)
{
    *alac = (Core1Alac){.decoder = decoder};
    return (Core1Decoder){start_alac, decode_alac, alac};
}
