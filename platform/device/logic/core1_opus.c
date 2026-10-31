#include "device/core1_opus.h"

#include <stddef.h>

#include "opus.h"

uint32_t core1_opus_decoder_bytes(void)
{
    return (uint32_t)opus_decoder_get_size(FLAC_MAX_CHANNELS);
}

static void start_opus(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                       const bool *starved)
{
    Core1Opus *opus = context;
    (void)first_sample;
    opus->starved = starved;
    opus->channels = info.channels;
    OpusDecoder *decoder = opus->memory.decoder;
    opus->ready = info.channels >= 1 && info.channels <= FLAC_MAX_CHANNELS &&
                  opus->memory.decoder_bytes >= (uint32_t)opus_decoder_get_size(info.channels) &&
                  opus_decoder_init(decoder, OGG_OPUS_RATE, (int)info.channels) == OPUS_OK;
    ogg_reader_start(&opus->reader, input, 0, 0, opus->memory.packet, CORE1_OPUS_PACKET_BYTES);
    ogg_reader_follow_first(&opus->reader);
}

static Core1DecodeResult decode_opus(void *context, Core1Block *block)
{
    Core1Opus *opus = context;
    if (!opus->ready) {
        return CORE1_FAILED;
    }
    OggPacket packet;
    switch (ogg_reader_next(&opus->reader, &packet)) {
    case OGG_PACKET:
        break;
    case OGG_END:
        return CORE1_FINISHED;
    case OGG_MISSING:
        return *opus->starved ? CORE1_STARVED : CORE1_FAILED;
    case OGG_BAD:
        return CORE1_FAILED;
    }
    int frames = opus_decode(opus->memory.decoder, packet.data, (opus_int32)packet.length,
                             opus->memory.output, CORE1_OPUS_MAX_FRAME, 0);
    if (frames < 0) {
        return CORE1_FAILED;
    }
    for (uint32_t channel = 0; channel < FLAC_MAX_CHANNELS; channel++) {
        block->narrow[channel] = opus->memory.output + (channel < opus->channels ? channel : 0);
    }
    block->stride = opus->channels;
    block->first_sample = packet.first_sample;
    block->samples = (uint32_t)frames;
    block->position = opus->reader.position;
    return CORE1_DECODED;
}

Core1Decoder core1_opus_decoder(Core1Opus *opus, Core1OpusMemory memory)
{
    *opus = (Core1Opus){.memory = memory};
    return (Core1Decoder){start_opus, decode_opus, opus};
}
