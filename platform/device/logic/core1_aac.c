#include "device/core1_aac.h"

#include <stddef.h>

#include "codec/mp4.h"
#include "neaacdec.h"

enum {
    CONFIG_CAPACITY = MP4_CONFIG_CAPACITY,
    OBJECT_LC = 2
};

void core1_aac_close(Core1Aac *aac)
{
    if (aac->handle != NULL) {
        NeAACDecClose(aac->handle);
        aac->handle = NULL;
    }
    if (aac->reset_heap != NULL) {
        aac->reset_heap();
    }
    aac->configured = false;
}

static void start_aac(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                      const bool *starved)
{
    Core1Aac *aac = context;
    core1_aac_close(aac);
    aac->frame_samples = info.min_block > 0 ? info.min_block : CORE1_AAC_FRAME_SAMPLES;
    aac->input = input;
    aac->starved = starved;
    aac->position = 0;
    aac->next_sample = first_sample;
}

static bool read_record(Core1Aac *aac, uint32_t *start, uint32_t *length)
{
    uint8_t bytes[MP4_LENGTH_BYTES];
    FlacInput input = aac->input;
    if (!input.read(input.context, aac->position, bytes, sizeof bytes)) {
        return false;
    }
    *start = aac->position + MP4_LENGTH_BYTES;
    *length = mp4_record_length(bytes);
    if (*length == 0 || *start > input.size || *length > input.size - *start) {
        return false;
    }
    uint8_t last = 0;
    return input.read(input.context, *start + *length - 1, &last, 1);
}

static bool configure(Core1Aac *aac, uint32_t start, uint32_t length)
{
    uint8_t config[CONFIG_CAPACITY];
    if (length > CONFIG_CAPACITY || !aac->input.read(aac->input.context, start, config, length)) {
        return false;
    }
    aac->handle = NeAACDecOpen();
    if (aac->handle == NULL) {
        return false;
    }
    NeAACDecConfigurationPtr settings = NeAACDecGetCurrentConfiguration(aac->handle);
    settings->defObjectType = OBJECT_LC;
    settings->outputFormat = FAAD_FMT_24BIT;
    settings->downMatrix = 0;
    settings->dontUpSampleImplicitSBR = 1;
    unsigned long rate = 0;
    unsigned char channels = 0;
    if (!NeAACDecSetConfiguration(aac->handle, settings) ||
        (signed char)NeAACDecInit2(aac->handle, config, length, &rate, &channels) < 0 ||
        channels == 0 || channels > FLAC_MAX_CHANNELS) {
        return false;
    }
    aac->channels = channels;
    aac->configured = true;
    return true;
}

static Core1DecodeResult decode_aac(void *context, Core1Block *block)
{
    Core1Aac *aac = context;
    for (;;) {
        if (aac->position >= aac->input.size) {
            return CORE1_FINISHED;
        }
        uint32_t start = 0;
        uint32_t length = 0;
        if (!read_record(aac, &start, &length)) {
            return *aac->starved ? CORE1_STARVED : CORE1_FAILED;
        }
        if (!aac->configured) {
            if (!configure(aac, start, length)) {
                return CORE1_FAILED;
            }
            aac->position = start + length;
            continue;
        }
        if (length > sizeof aac->frame ||
            !aac->input.read(aac->input.context, start, aac->frame, length)) {
            return *aac->starved ? CORE1_STARVED : CORE1_FAILED;
        }
        NeAACDecFrameInfo info;
        const int32_t *samples = NeAACDecDecode(aac->handle, &info, aac->frame, length);
        if (info.error != 0 || (info.samples > 0 && samples == NULL) ||
            (info.samples > 0 && info.channels != aac->channels)) {
            return CORE1_FAILED;
        }
        uint32_t frames = info.samples > 0 ? (uint32_t)info.samples / aac->channels : 0;
        if (frames > CORE1_AAC_FRAME_SAMPLES) {
            return CORE1_FAILED;
        }
        for (uint32_t channel = 0; channel < FLAC_MAX_CHANNELS; channel++) {
            block->channels[channel] = samples + (channel < aac->channels ? channel : 0);
        }
        aac->position = start + length;
        block->stride = aac->channels;
        block->first_sample = aac->next_sample;
        block->samples = frames;
        block->position = aac->position;
        aac->next_sample += aac->frame_samples;
        return CORE1_DECODED;
    }
}

Core1Decoder core1_aac_decoder(Core1Aac *aac, void (*reset_heap)(void))
{
    *aac = (Core1Aac){.reset_heap = reset_heap};
    return (Core1Decoder){start_aac, decode_aac, aac};
}
