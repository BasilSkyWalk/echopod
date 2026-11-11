#include "codec/qoa.h"

#include "qoa_reference.h"

enum {
    QOA_FRAME_HEADER_BYTES = 8,
    QOA_LMS_BYTES = 16,
    QOA_SLICE_BYTES = 8,
    QOA_MAX_RATE = 0xFFFFFF
};

typedef struct QoaFrameHeader {
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t samples;
    uint32_t frame_bytes;
} QoaFrameHeader;

static uint32_t read_be16(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 8 | (uint32_t)bytes[1];
}

static uint64_t read_be64(const uint8_t *bytes)
{
    return (uint64_t)read_be16(bytes) << 48 | (uint64_t)read_be16(bytes + 2) << 32 |
           (uint64_t)read_be16(bytes + 4) << 16 | (uint64_t)read_be16(bytes + 6);
}

static bool frame_header(const uint8_t *bytes, QoaFrameHeader *header)
{
    uint64_t word = read_be64(bytes);
    header->channels = (uint32_t)(word >> 56);
    header->sample_rate = (uint32_t)(word >> 32) & QOA_MAX_RATE;
    header->samples = (uint32_t)(word >> 16) & 0xFFFFu;
    header->frame_bytes = (uint32_t)word & 0xFFFFu;
    return header->channels > 0 && header->sample_rate > 0 && header->samples > 0 &&
           header->frame_bytes > 0;
}

static bool frame_fits(const QoaFrameHeader *header, const QoaInfo *info)
{
    uint32_t channels = info->channels;
    uint32_t header_bytes = QOA_FRAME_HEADER_BYTES + QOA_LMS_BYTES * channels;
    uint32_t slices = (header->samples + QOA_SLICE_SAMPLES - 1) / QOA_SLICE_SAMPLES;
    return header->channels == info->channels && header->sample_rate == info->sample_rate &&
           header->samples <= QOA_FRAME_SAMPLES &&
           header_bytes + QOA_SLICE_BYTES * slices * channels <= header->frame_bytes &&
           header->frame_bytes <= header_bytes + QOA_SLICE_BYTES * QOA_FRAME_SLICES * channels;
}

QoaResult qoa_parse_header(FlacInput input, QoaInfo *info)
{
    uint8_t bytes[QOA_SNIFF_BYTES];
    if (input.size < QOA_SNIFF_BYTES || !input.read(input.context, 0, bytes, sizeof bytes)) {
        return QOA_ERROR;
    }
    qoa_desc desc;
    if (qoa_decode_header(bytes, (int)sizeof bytes, &desc) == 0) {
        return QOA_ERROR;
    }
    if (desc.channels == 0 || desc.samplerate == 0 || desc.samples == 0) {
        return QOA_ERROR;
    }
    if (desc.channels > QOA_MAX_PLAYBACK_CHANNELS) {
        return QOA_UNSUPPORTED;
    }
    QoaInfo parsed = {
        .sample_rate = desc.samplerate,
        .channels = (uint8_t)desc.channels,
        .total_samples = desc.samples,
    };
    QoaFrameHeader first;
    if (!frame_header(bytes + QOA_FRAME_HEADER_BYTES, &first) || !frame_fits(&first, &parsed) ||
        first.frame_bytes > input.size - QOA_FILE_HEADER_BYTES) {
        return QOA_ERROR;
    }
    parsed.frame_samples = (uint16_t)first.samples;
    parsed.frame_bytes = (uint16_t)first.frame_bytes;
    *info = parsed;
    return QOA_OK;
}

static QoaResult decode_frame(QoaDecoder *decoder, const QoaFrameHeader *header)
{
    qoa_desc desc = {
        .channels = decoder->info.channels,
        .samplerate = decoder->info.sample_rate,
        .samples = decoder->info.total_samples,
    };
    unsigned frame_len = 0;
    unsigned used = qoa_decode_frame(decoder->frame, header->frame_bytes, &desc,
                                     (short *)decoder->samples, &frame_len);
    if (used != header->frame_bytes || frame_len == 0 || frame_len > header->samples) {
        return QOA_ERROR;
    }
    uint32_t remaining = decoder->info.total_samples - decoder->next_sample;
    uint32_t total = frame_len < remaining ? frame_len : remaining;
    decoder->frame_first_sample = decoder->next_sample;
    decoder->frame_count = total;
    decoder->next_sample += total;
    decoder->frame_position += header->frame_bytes;
    decoder->cursor = decoder->pending_skip < total ? decoder->pending_skip : total;
    decoder->pending_skip = 0;
    decoder->have_frame = true;
    return QOA_OK;
}

static QoaResult load_frame(QoaDecoder *decoder)
{
    if (decoder->next_sample >= decoder->info.total_samples) {
        return QOA_END;
    }
    if (decoder->frame_position > decoder->input.size ||
        decoder->input.size - decoder->frame_position < QOA_FRAME_HEADER_BYTES) {
        return QOA_ERROR;
    }
    if (!decoder->input.read(decoder->input.context, decoder->frame_position, decoder->frame,
                             QOA_FRAME_HEADER_BYTES)) {
        return QOA_ERROR;
    }
    QoaFrameHeader header;
    if (!frame_header(decoder->frame, &header) || !frame_fits(&header, &decoder->info) ||
        header.frame_bytes > decoder->input.size - decoder->frame_position) {
        return QOA_ERROR;
    }
    if (header.frame_bytes > QOA_FRAME_HEADER_BYTES &&
        !decoder->input.read(
            decoder->input.context, decoder->frame_position + QOA_FRAME_HEADER_BYTES,
            decoder->frame + QOA_FRAME_HEADER_BYTES, header.frame_bytes - QOA_FRAME_HEADER_BYTES)) {
        return QOA_ERROR;
    }
    return decode_frame(decoder, &header);
}

QoaResult qoa_open(QoaDecoder *decoder, FlacInput input)
{
    QoaInfo info;
    QoaResult result = qoa_parse_header(input, &info);
    if (result != QOA_OK) {
        return result;
    }
    qoa_start(decoder, input, info, 0);
    decoder->frame_position = QOA_FILE_HEADER_BYTES;
    return QOA_OK;
}

void qoa_start(QoaDecoder *decoder, FlacInput input, QoaInfo info, uint32_t first_sample)
{
    decoder->input = input;
    decoder->info = info;
    decoder->frame_position = 0;
    decoder->frame_first_sample = first_sample;
    decoder->frame_count = 0;
    decoder->cursor = 0;
    decoder->pending_skip = 0;
    decoder->next_sample = first_sample;
    decoder->have_frame = false;
}

QoaResult qoa_next_block(QoaDecoder *decoder, const int16_t **left, const int16_t **right,
                         uint32_t *stride, uint32_t *first_sample, uint32_t *samples)
{
    for (;;) {
        if (!decoder->have_frame) {
            QoaResult result = load_frame(decoder);
            if (result != QOA_OK) {
                return result;
            }
        }
        if (decoder->cursor < decoder->frame_count) {
            break;
        }
        decoder->have_frame = false;
    }
    uint32_t count = decoder->frame_count - decoder->cursor;
    if (count > QOA_BLOCK_SAMPLES) {
        count = QOA_BLOCK_SAMPLES;
    }
    uint32_t channels = decoder->info.channels;
    const int16_t *base = &decoder->samples[decoder->cursor * channels];
    *left = base;
    *right = base + (channels > 1 ? 1 : 0);
    *stride = channels;
    *first_sample = decoder->frame_first_sample + decoder->cursor;
    *samples = count;
    decoder->cursor += count;
    return QOA_OK;
}

bool qoa_frame_at(FlacInput input, const QoaInfo *info, uint32_t sample, uint32_t *offset,
                  uint32_t *first_sample)
{
    if (input.size < QOA_SNIFF_BYTES || sample >= info->total_samples || info->frame_samples == 0 ||
        info->frame_bytes == 0) {
        return false;
    }
    uint8_t bytes[QOA_FRAME_HEADER_BYTES];
    uint32_t index = sample / info->frame_samples;
    uint64_t candidate = QOA_FILE_HEADER_BYTES + (uint64_t)index * info->frame_bytes;
    if (index == 0 || (candidate + QOA_FRAME_HEADER_BYTES <= input.size &&
                       input.read(input.context, (uint32_t)candidate, bytes, sizeof bytes))) {
        QoaFrameHeader header;
        if (index == 0 ||
            (frame_header(bytes, &header) && frame_fits(&header, info) &&
             header.samples <= info->frame_samples && header.frame_bytes <= info->frame_bytes)) {
            *offset = (uint32_t)candidate;
            *first_sample = index * info->frame_samples;
            return true;
        }
    }
    *offset = QOA_FILE_HEADER_BYTES;
    *first_sample = 0;
    while (*offset <= input.size - QOA_FRAME_HEADER_BYTES) {
        QoaFrameHeader header;
        if (!input.read(input.context, *offset, bytes, sizeof bytes) ||
            !frame_header(bytes, &header) || !frame_fits(&header, info) ||
            header.frame_bytes > input.size - *offset) {
            return false;
        }
        if (*first_sample + header.samples > sample) {
            return true;
        }
        *offset += header.frame_bytes;
        *first_sample += header.samples;
    }
    return false;
}

QoaResult qoa_seek(QoaDecoder *decoder, uint32_t sample)
{
    if (sample >= decoder->info.total_samples) {
        decoder->next_sample = decoder->info.total_samples;
        decoder->frame_position = decoder->input.size;
        decoder->frame_count = 0;
        decoder->cursor = 0;
        decoder->pending_skip = 0;
        decoder->have_frame = false;
        return QOA_END;
    }
    uint32_t offset = 0;
    uint32_t first = 0;
    if (!qoa_frame_at(decoder->input, &decoder->info, sample, &offset, &first)) {
        return QOA_ERROR;
    }
    decoder->frame_position = offset;
    decoder->frame_first_sample = first;
    decoder->frame_count = 0;
    decoder->cursor = 0;
    decoder->pending_skip = sample - first;
    decoder->next_sample = first;
    decoder->have_frame = false;
    return QOA_OK;
}
