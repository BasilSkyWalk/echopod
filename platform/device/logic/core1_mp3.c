#include "device/core1_mp3.h"

static void start_mp3(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                      const bool *starved)
{
    Core1Mp3 *mp3 = context;
    (void)info;
    (void)starved;
    mp3->next_sample = first_sample;
    mp3_decoder_start(mp3->decoder, input);
}

static Core1DecodeResult decode_mp3(void *context, Core1Block *block)
{
    Core1Mp3 *mp3 = context;
    switch (mp3_decode(mp3->decoder)) {
    case MP3_DECODED:
        break;
    case MP3_STARVED:
        return CORE1_STARVED;
    case MP3_FINISHED:
        return CORE1_FINISHED;
    case MP3_FAILED:
        return CORE1_FAILED;
    }
    int channels = mp3_decoder_channels(mp3->decoder);
    block->channels[0] = mp3_decoder_channel(mp3->decoder, 0);
    block->channels[1] = mp3_decoder_channel(mp3->decoder, channels > 1 ? 1 : 0);
    block->first_sample = mp3->next_sample;
    block->samples = mp3_decoder_samples(mp3->decoder);
    block->position = mp3_decoder_position(mp3->decoder);
    mp3->next_sample += block->samples;
    return CORE1_DECODED;
}

Core1Decoder core1_mp3_decoder(Core1Mp3 *mp3, Mp3Decoder *decoder)
{
    *mp3 = (Core1Mp3){.decoder = decoder, .next_sample = 0};
    return (Core1Decoder){start_mp3, decode_mp3, mp3};
}
