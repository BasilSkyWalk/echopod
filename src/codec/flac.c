#include "codec/flac.h"

#include <stddef.h>

#include "flac_bits.h"

enum {
    MAGIC = 0x664C6143,
    ID3_MAGIC = 0x494433,
    ID3_HEADER_BYTES = 10,
    ID3_FOOTER_FLAG = 0x10,
    METADATA_STREAMINFO = 0,
    STREAMINFO_BYTES = 34,
    FRAME_SYNC = 0x3FFE,
    MAX_FIXED_ORDER = 4,
    MAX_LPC_ORDER = 32,
    CHANNEL_LEFT_SIDE = 8,
    CHANNEL_SIDE_RIGHT = 9,
    CHANNEL_MID_SIDE = 10,
    RICE_ESCAPE_4 = 15,
    RICE_ESCAPE_5 = 31
};

typedef enum Subframe {
    SUBFRAME_CONSTANT,
    SUBFRAME_VERBATIM,
    SUBFRAME_FIXED,
    SUBFRAME_LPC,
    SUBFRAME_RESERVED
} Subframe;

typedef struct FrameHeader {
    uint32_t block_size;
    uint32_t bits_per_sample;
    uint32_t channel_assignment;
    uint32_t channels;
} FrameHeader;

static const uint8_t SAMPLE_SIZES[8] = {0, 8, 12, 0, 16, 20, 24, 0};

static uint32_t read(FlacDecoder *decoder, uint32_t count)
{
    return flac_bits_read(&decoder->bits, count);
}

static uint32_t id3_size(FlacDecoder *decoder)
{
    if (read(decoder, 24) != ID3_MAGIC) {
        return 0;
    }
    read(decoder, 8);
    read(decoder, 8);
    uint32_t flags = read(decoder, 8);
    uint32_t size = 0;
    for (int index = 0; index < 4; index++) {
        size = size << 7 | (read(decoder, 8) & 0x7Fu);
    }
    return ID3_HEADER_BYTES + size + ((flags & ID3_FOOTER_FLAG) != 0 ? ID3_HEADER_BYTES : 0);
}

static FlacResult read_streaminfo(FlacDecoder *decoder)
{
    FlacInfo *info = &decoder->info;
    read(decoder, 16);
    info->max_block = (uint16_t)read(decoder, 16);
    read(decoder, 24);
    read(decoder, 24);
    info->sample_rate = read(decoder, 20);
    info->channels = (uint8_t)(read(decoder, 3) + 1);
    info->bits_per_sample = (uint8_t)(read(decoder, 5) + 1);
    uint32_t high = read(decoder, 4);
    info->total_samples = read(decoder, 32);
    for (int index = 0; index < 16; index++) {
        read(decoder, 8);
    }
    if (decoder->bits.failed) {
        return FLAC_ERROR;
    }
    bool supported = high == 0 && info->max_block <= FLAC_MAX_BLOCK &&
                     info->channels <= FLAC_MAX_CHANNELS && info->bits_per_sample <= FLAC_MAX_BITS;
    return supported ? FLAC_OK : FLAC_UNSUPPORTED;
}

static FlacResult read_metadata(FlacDecoder *decoder)
{
    bool last = false;
    bool found = false;
    while (!last) {
        last = read(decoder, 1) != 0;
        uint32_t type = read(decoder, 7);
        uint32_t length = read(decoder, 24);
        if (decoder->bits.failed) {
            return FLAC_ERROR;
        }
        uint32_t next = flac_bits_offset(&decoder->bits) + length;
        if (type == METADATA_STREAMINFO && length == STREAMINFO_BYTES) {
            FlacResult result = read_streaminfo(decoder);
            if (result != FLAC_OK) {
                return result;
            }
            found = true;
        }
        if (next > decoder->bits.input.size) {
            return FLAC_ERROR;
        }
        flac_bits_start(&decoder->bits, decoder->bits.input, next);
    }
    decoder->audio_offset = flac_bits_offset(&decoder->bits);
    return found ? FLAC_OK : FLAC_ERROR;
}

FlacResult flac_open(FlacDecoder *decoder, FlacInput input)
{
    decoder->info = (FlacInfo){0};
    decoder->decoded_samples = 0;
    decoder->block_samples = 0;
    flac_bits_start(&decoder->bits, input, 0);
    uint32_t start = id3_size(decoder);
    flac_bits_start(&decoder->bits, input, start);
    if (read(decoder, 32) != MAGIC || decoder->bits.failed) {
        return FLAC_ERROR;
    }
    return read_metadata(decoder);
}

static bool skip_coded_number(FlacDecoder *decoder)
{
    uint32_t first = read(decoder, 8);
    int extra = 0;
    while (extra < 7 && (first & (0x80u >> extra)) != 0) {
        extra++;
    }
    if (extra == 1 || extra == 7) {
        return false;
    }
    for (int index = 1; index < extra; index++) {
        if ((read(decoder, 8) & 0xC0u) != 0x80u) {
            return false;
        }
    }
    return true;
}

static uint32_t block_size_of(FlacDecoder *decoder, uint32_t code)
{
    if (code == 1) {
        return 192;
    }
    if (code >= 2 && code <= 5) {
        return 576u << (code - 2);
    }
    if (code == 6) {
        return read(decoder, 8) + 1;
    }
    if (code == 7) {
        return read(decoder, 16) + 1;
    }
    if (code >= 8) {
        return 256u << (code - 8);
    }
    return 0;
}

static void skip_sample_rate(FlacDecoder *decoder, uint32_t code)
{
    if (code == 12) {
        read(decoder, 8);
    } else if (code == 13 || code == 14) {
        read(decoder, 16);
    }
}

static bool read_frame_header(FlacDecoder *decoder, FrameHeader *header)
{
    flac_bits_reset_crc(&decoder->bits);
    if (read(decoder, 14) != FRAME_SYNC || read(decoder, 1) != 0) {
        return false;
    }
    read(decoder, 1);
    uint32_t block_code = read(decoder, 4);
    uint32_t rate_code = read(decoder, 4);
    header->channel_assignment = read(decoder, 4);
    uint32_t size_code = read(decoder, 3);
    if (read(decoder, 1) != 0 || rate_code == 15 || !skip_coded_number(decoder)) {
        return false;
    }
    header->block_size = block_size_of(decoder, block_code);
    skip_sample_rate(decoder, rate_code);
    uint8_t expected_crc = decoder->bits.crc8;
    if (read(decoder, 8) != expected_crc || decoder->bits.failed) {
        return false;
    }
    header->bits_per_sample =
        size_code == 0 ? decoder->info.bits_per_sample : SAMPLE_SIZES[size_code];
    header->channels =
        header->channel_assignment < CHANNEL_LEFT_SIDE ? header->channel_assignment + 1 : 2;
    return header->block_size > 0 && header->block_size <= FLAC_MAX_BLOCK &&
           header->bits_per_sample > 0 && header->bits_per_sample <= FLAC_MAX_BITS &&
           header->channel_assignment <= CHANNEL_MID_SIDE &&
           header->channels == decoder->info.channels;
}

static Subframe subframe_kind(uint32_t type, uint32_t *order)
{
    if (type == 0) {
        return SUBFRAME_CONSTANT;
    }
    if (type == 1) {
        return SUBFRAME_VERBATIM;
    }
    if (type >= 8 && type <= 8 + MAX_FIXED_ORDER) {
        *order = type - 8;
        return SUBFRAME_FIXED;
    }
    if (type >= 32) {
        *order = type - 31;
        return SUBFRAME_LPC;
    }
    return SUBFRAME_RESERVED;
}

static bool read_residual(FlacDecoder *decoder, int32_t *residual, uint32_t block_size,
                          uint32_t order)
{
    uint32_t method = read(decoder, 2);
    if (method > 1) {
        return false;
    }
    uint32_t parameter_bits = method == 0 ? 4 : 5;
    uint32_t escape = method == 0 ? RICE_ESCAPE_4 : RICE_ESCAPE_5;
    uint32_t partition_order = read(decoder, 4);
    uint32_t partitions = 1u << partition_order;
    uint32_t per_partition = block_size >> partition_order;
    if (per_partition << partition_order != block_size || per_partition < order) {
        return false;
    }
    uint32_t sample = order;
    for (uint32_t partition = 0; partition < partitions; partition++) {
        uint32_t count = partition == 0 ? per_partition - order : per_partition;
        uint32_t parameter = read(decoder, parameter_bits);
        if (parameter == escape) {
            uint32_t width = read(decoder, 5);
            for (uint32_t index = 0; index < count; index++) {
                residual[sample++] = flac_bits_read_signed(&decoder->bits, width);
            }
            continue;
        }
        for (uint32_t index = 0; index < count; index++) {
            uint32_t value = flac_bits_read_unary(&decoder->bits) << parameter;
            value |= read(decoder, parameter);
            residual[sample++] = (int32_t)(value >> 1) ^ -(int32_t)(value & 1u);
        }
        if (decoder->bits.failed) {
            return false;
        }
    }
    return true;
}

static void restore_fixed(int32_t *samples, uint32_t block_size, uint32_t order)
{
    for (uint32_t index = order; index < block_size; index++) {
        int32_t *s = &samples[index];
        switch (order) {
        case 1:
            s[0] += s[-1];
            break;
        case 2:
            s[0] += 2 * s[-1] - s[-2];
            break;
        case 3:
            s[0] += 3 * s[-1] - 3 * s[-2] + s[-3];
            break;
        case 4:
            s[0] += 4 * s[-1] - 6 * s[-2] + 4 * s[-3] - s[-4];
            break;
        default:
            break;
        }
    }
}

static void restore_lpc(int32_t *samples, uint32_t block_size, const int32_t *coefficients,
                        uint32_t order, uint32_t shift)
{
    for (uint32_t index = order; index < block_size; index++) {
        int64_t sum = 0;
        for (uint32_t tap = 0; tap < order; tap++) {
            sum += (int64_t)coefficients[tap] * samples[index - 1 - tap];
        }
        int64_t predicted = sum >= 0 ? sum >> shift : -((-sum - 1) >> shift) - 1;
        samples[index] += (int32_t)predicted;
    }
}

static bool read_warmup(FlacDecoder *decoder, int32_t *samples, uint32_t order, uint32_t width)
{
    for (uint32_t index = 0; index < order; index++) {
        samples[index] = flac_bits_read_signed(&decoder->bits, width);
    }
    return !decoder->bits.failed;
}

static bool read_lpc(FlacDecoder *decoder, int32_t *samples, uint32_t block_size, uint32_t order,
                     uint32_t width)
{
    int32_t coefficients[MAX_LPC_ORDER];
    if (!read_warmup(decoder, samples, order, width)) {
        return false;
    }
    uint32_t precision = read(decoder, 4) + 1;
    int32_t shift = flac_bits_read_signed(&decoder->bits, 5);
    if (precision == 16 || shift < 0) {
        return false;
    }
    for (uint32_t tap = 0; tap < order; tap++) {
        coefficients[tap] = flac_bits_read_signed(&decoder->bits, precision);
    }
    if (!read_residual(decoder, samples, block_size, order)) {
        return false;
    }
    restore_lpc(samples, block_size, coefficients, order, (uint32_t)shift);
    return true;
}

static bool read_subframe(FlacDecoder *decoder, int32_t *samples, uint32_t block_size,
                          uint32_t width)
{
    uint32_t order = 0;
    if (read(decoder, 1) != 0) {
        return false;
    }
    Subframe kind = subframe_kind(read(decoder, 6), &order);
    uint32_t wasted = 0;
    if (read(decoder, 1) != 0) {
        wasted = flac_bits_read_unary(&decoder->bits) + 1;
    }
    if (kind == SUBFRAME_RESERVED || wasted >= width || order > block_size) {
        return false;
    }
    width -= wasted;
    bool ok = true;
    switch (kind) {
    case SUBFRAME_CONSTANT: {
        int32_t value = flac_bits_read_signed(&decoder->bits, width);
        for (uint32_t index = 0; index < block_size; index++) {
            samples[index] = value;
        }
        break;
    }
    case SUBFRAME_VERBATIM:
        ok = read_warmup(decoder, samples, block_size, width);
        break;
    case SUBFRAME_FIXED:
        ok = read_warmup(decoder, samples, order, width) &&
             read_residual(decoder, samples, block_size, order);
        if (ok) {
            restore_fixed(samples, block_size, order);
        }
        break;
    case SUBFRAME_LPC:
        ok = read_lpc(decoder, samples, block_size, order, width);
        break;
    case SUBFRAME_RESERVED:
        return false;
    }
    if (wasted > 0) {
        for (uint32_t index = 0; index < block_size; index++) {
            samples[index] = (int32_t)((uint32_t)samples[index] << wasted);
        }
    }
    return ok && !decoder->bits.failed;
}

static uint32_t channel_width(const FrameHeader *header, int channel)
{
    bool side = (header->channel_assignment == CHANNEL_LEFT_SIDE && channel == 1) ||
                (header->channel_assignment == CHANNEL_SIDE_RIGHT && channel == 0) ||
                (header->channel_assignment == CHANNEL_MID_SIDE && channel == 1);
    return header->bits_per_sample + (side ? 1u : 0u);
}

static int32_t floor_half(int32_t value)
{
    return value >= 0 ? value / 2 : -((-value - 1) / 2) - 1;
}

static void decorrelate(FlacDecoder *decoder, const FrameHeader *header)
{
    int32_t *first = decoder->samples[0];
    int32_t *second = decoder->samples[1];
    for (uint32_t index = 0; index < header->block_size; index++) {
        switch (header->channel_assignment) {
        case CHANNEL_LEFT_SIDE:
            second[index] = first[index] - second[index];
            break;
        case CHANNEL_SIDE_RIGHT:
            first[index] += second[index];
            break;
        case CHANNEL_MID_SIDE: {
            int32_t side = second[index];
            int32_t mid = (int32_t)((uint32_t)first[index] << 1) | (side & 1);
            first[index] = floor_half(mid + side);
            second[index] = floor_half(mid - side);
            break;
        }
        default:
            break;
        }
    }
}

static bool frame_crc_matches(FlacDecoder *decoder)
{
    flac_bits_align(&decoder->bits);
    uint16_t expected = decoder->bits.crc16;
    return read(decoder, 16) == expected && !decoder->bits.failed;
}

FlacResult flac_decode_frame(FlacDecoder *decoder)
{
    decoder->block_samples = 0;
    bool known_total = decoder->info.total_samples > 0;
    if ((known_total && decoder->decoded_samples >= decoder->info.total_samples) ||
        flac_bits_offset(&decoder->bits) >= decoder->bits.input.size) {
        return FLAC_END;
    }
    FrameHeader header;
    if (!read_frame_header(decoder, &header)) {
        return FLAC_ERROR;
    }
    for (uint32_t channel = 0; channel < header.channels; channel++) {
        uint32_t width = channel_width(&header, (int)channel);
        if (!read_subframe(decoder, decoder->samples[channel], header.block_size, width)) {
            return FLAC_ERROR;
        }
    }
    if (!frame_crc_matches(decoder)) {
        return FLAC_ERROR;
    }
    decorrelate(decoder, &header);
    decoder->block_samples = header.block_size;
    decoder->decoded_samples += header.block_size;
    return FLAC_OK;
}

const int32_t *flac_channel(const FlacDecoder *decoder, int channel)
{
    return decoder->samples[channel];
}
