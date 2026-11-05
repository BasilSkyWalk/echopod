#include "device/core1_qoa.h"

#include <stddef.h>

static void start_qoa(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                      const bool *starved)
{
    Core1Qoa *qoa = context;
    qoa->starved = starved;
    QoaInfo qoa_info = {
        .sample_rate = info.sample_rate,
        .channels = info.channels,
        .total_samples = info.total_samples,
        .frame_samples = info.max_block,
        .frame_bytes = 0,
    };
    qoa_start(qoa->decoder, input, qoa_info, first_sample);
}

static Core1DecodeResult decode_qoa(void *context, Core1Block *block)
{
    Core1Qoa *qoa = context;
    const int16_t *left = NULL;
    const int16_t *right = NULL;
    uint32_t stride = 0;
    uint32_t first_sample = 0;
    uint32_t samples = 0;
    QoaResult result =
        qoa_next_block(qoa->decoder, &left, &right, &stride, &first_sample, &samples);
    if (result != QOA_OK) {
        if (*qoa->starved) {
            return CORE1_STARVED;
        }
        return result == QOA_END ? CORE1_FINISHED : CORE1_FAILED;
    }
    block->narrow[0] = left;
    block->narrow[1] = right;
    block->stride = stride;
    block->first_sample = first_sample;
    block->samples = samples;
    block->position = qoa->decoder->frame_position;
    return CORE1_DECODED;
}

Core1Decoder core1_qoa_decoder(Core1Qoa *qoa, QoaDecoder *decoder)
{
    *qoa = (Core1Qoa){.decoder = decoder};
    return (Core1Decoder){start_qoa, decode_qoa, qoa};
}
