#include "flac_writer.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum {
    FIXED_ORDER = 2,
    LEFT_SIDE = 8,
    BLOCK_CODE_16_BIT = 7,
    FIXED_TYPE = 8,
    RICE_METHOD_5_BIT = 1,
    LARGEST_PARAMETER = 30,
    LARGEST_PARTITION_ORDER = 8
};

typedef struct BitWriter {
    uint8_t *bytes;
    uint32_t capacity;
    uint32_t length;
    uint32_t pending;
    uint32_t pending_bits;
} BitWriter;

static void put_byte(BitWriter *writer, uint8_t value)
{
    if (writer->length == writer->capacity) {
        writer->capacity = writer->capacity * 2 + 4096;
        writer->bytes = realloc(writer->bytes, writer->capacity);
    }
    writer->bytes[writer->length++] = value;
}

static void put_bits(BitWriter *writer, uint32_t value, uint32_t count)
{
    for (uint32_t bit = count; bit-- > 0;) {
        writer->pending = writer->pending << 1 | (value >> bit & 1u);
        if (++writer->pending_bits == 8) {
            put_byte(writer, (uint8_t)writer->pending);
            writer->pending = 0;
            writer->pending_bits = 0;
        }
    }
}

static void put_signed(BitWriter *writer, int32_t value, uint32_t count)
{
    put_bits(writer, (uint32_t)value & (count == 32 ? 0xFFFFFFFFu : (1u << count) - 1u), count);
}

static void align(BitWriter *writer)
{
    while (writer->pending_bits != 0) {
        put_bits(writer, 0, 1);
    }
}

static uint8_t crc8(const uint8_t *bytes, uint32_t length)
{
    uint32_t crc = 0;
    for (uint32_t index = 0; index < length; index++) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80u) != 0 ? (crc << 1) ^ 0x07u : crc << 1;
        }
    }
    return (uint8_t)crc;
}

static uint16_t crc16(const uint8_t *bytes, uint32_t length)
{
    uint32_t crc = 0;
    for (uint32_t index = 0; index < length; index++) {
        crc ^= (uint32_t)bytes[index] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) != 0 ? (crc << 1) ^ 0x8005u : crc << 1;
        }
    }
    return (uint16_t)crc;
}

static void put_coded_number(BitWriter *writer, uint32_t number)
{
    if (number < 0x80u) {
        put_bits(writer, number, 8);
        return;
    }
    uint32_t extra = 1;
    while (extra < 5 && number >= 1u << (6 - extra + 6 * extra)) {
        extra++;
    }
    uint32_t lead = (0xFF00u >> (extra + 1)) & 0xFFu;
    put_bits(writer, lead | number >> (6 * extra), 8);
    for (uint32_t index = extra; index-- > 0;) {
        put_bits(writer, 0x80u | (number >> (6 * index) & 0x3Fu), 8);
    }
}

static uint32_t zigzag(int64_t value)
{
    return value >= 0 ? (uint32_t)(value * 2) : (uint32_t)(-value * 2 - 1);
}

static uint32_t parameter_for(const int64_t *residual, uint32_t count)
{
    uint64_t sum = 0;
    for (uint32_t index = 0; index < count; index++) {
        sum += zigzag(residual[index]);
    }
    uint32_t parameter = 0;
    while (parameter < LARGEST_PARAMETER && ((uint64_t)count << (parameter + 1)) < sum) {
        parameter++;
    }
    return parameter;
}

static void put_rice(BitWriter *writer, int64_t value, uint32_t parameter)
{
    uint32_t folded = zigzag(value);
    for (uint32_t quotient = folded >> parameter; quotient > 0; quotient--) {
        put_bits(writer, 0, 1);
    }
    put_bits(writer, 1, 1);
    put_bits(writer, folded & ((1u << parameter) - 1u), parameter);
}

static uint32_t partition_order_for(uint32_t block_size)
{
    uint32_t order = 0;
    while (order < LARGEST_PARTITION_ORDER &&
           (block_size >> (order + 1)) << (order + 1) == block_size &&
           (block_size >> (order + 1)) > FIXED_ORDER) {
        order++;
    }
    return order;
}

static void put_subframe(BitWriter *writer, const int64_t *samples, uint32_t block_size,
                         uint32_t width)
{
    put_bits(writer, 0, 1);
    put_bits(writer, FIXED_TYPE + FIXED_ORDER, 6);
    put_bits(writer, 0, 1);
    for (uint32_t index = 0; index < FIXED_ORDER; index++) {
        put_signed(writer, (int32_t)samples[index], width);
    }
    int64_t *residual = malloc(sizeof(int64_t) * block_size);
    for (uint32_t index = FIXED_ORDER; index < block_size; index++) {
        residual[index] = samples[index] - 2 * samples[index - 1] + samples[index - 2];
    }
    uint32_t order = partition_order_for(block_size);
    uint32_t per_partition = block_size >> order;
    put_bits(writer, RICE_METHOD_5_BIT, 2);
    put_bits(writer, order, 4);
    for (uint32_t partition = 0; partition < 1u << order; partition++) {
        uint32_t first = partition == 0 ? FIXED_ORDER : partition * per_partition;
        uint32_t end = (partition + 1) * per_partition;
        uint32_t parameter = parameter_for(residual + first, end - first);
        put_bits(writer, parameter, 5);
        for (uint32_t index = first; index < end; index++) {
            put_rice(writer, residual[index], parameter);
        }
    }
    free(residual);
}

static int32_t sample_value(FlacWriterSpec spec, uint32_t frame, uint32_t channel, uint32_t *noise)
{
    int64_t peak = ((int64_t)1 << (spec.bits - 1)) - 1;
    uint32_t period = 97 + 61 * channel;
    int64_t phase = (int64_t)(frame % period) * 4 * peak / period;
    int64_t triangle = phase < 2 * peak ? phase - peak : 3 * peak - phase;
    *noise = *noise * 1103515245u + 12345u;
    int64_t jitter = (int64_t)(*noise >> 8 & 0xFFFFu) - 0x8000;
    int64_t value = triangle * 3 / 5 + jitter * peak / 0x80000;
    return (int32_t)(value > peak ? peak : value < -peak - 1 ? -peak - 1 : value);
}

static void put_streaminfo(BitWriter *writer, FlacWriterSpec spec)
{
    put_bits(writer, 0x664C6143u, 32);
    put_bits(writer, 1, 1);
    put_bits(writer, 0, 7);
    put_bits(writer, 34, 24);
    put_bits(writer, spec.block_size, 16);
    put_bits(writer, spec.block_size, 16);
    put_bits(writer, 0, 24);
    put_bits(writer, 0, 24);
    put_bits(writer, spec.sample_rate, 20);
    put_bits(writer, spec.channels - 1, 3);
    put_bits(writer, spec.bits - 1, 5);
    put_bits(writer, 0, 4);
    put_bits(writer, spec.frames, 32);
    for (int index = 0; index < 16; index++) {
        put_bits(writer, 0, 8);
    }
}

static void put_frame(BitWriter *writer, FlacWriterSpec spec, const int32_t *samples,
                      uint32_t number, uint32_t block_size)
{
    uint32_t start = writer->length;
    bool stereo = spec.channels == 2;
    put_bits(writer, 0x3FFE, 14);
    put_bits(writer, 0, 2);
    put_bits(writer, BLOCK_CODE_16_BIT, 4);
    put_bits(writer, 0, 4);
    put_bits(writer, stereo ? LEFT_SIDE : spec.channels - 1, 4);
    put_bits(writer, 0, 4);
    put_coded_number(writer, number);
    put_bits(writer, block_size - 1, 16);
    put_bits(writer, crc8(writer->bytes + start, writer->length - start), 8);
    int64_t *channel = malloc(sizeof(int64_t) * block_size);
    uint32_t first = number * spec.block_size;
    for (uint32_t index = 0; index < spec.channels; index++) {
        for (uint32_t sample = 0; sample < block_size; sample++) {
            const int32_t *row = samples + (size_t)(first + sample) * spec.channels;
            channel[sample] = index == 1 && stereo ? (int64_t)row[0] - row[1] : row[index];
        }
        put_subframe(writer, channel, block_size, spec.bits + (index == 1 && stereo ? 1u : 0u));
    }
    free(channel);
    align(writer);
    put_bits(writer, crc16(writer->bytes + start, writer->length - start), 16);
}

FlacWriterOutput flac_writer_make(FlacWriterSpec spec)
{
    FlacWriterOutput output = {0};
    output.sample_bytes = spec.frames * spec.channels * (uint32_t)sizeof(int32_t);
    output.samples = malloc(output.sample_bytes);
    uint32_t noise = spec.seed;
    for (uint32_t frame = 0; frame < spec.frames; frame++) {
        for (uint32_t channel = 0; channel < spec.channels; channel++) {
            output.samples[frame * spec.channels + channel] =
                sample_value(spec, frame, channel, &noise);
        }
    }
    BitWriter writer = {0};
    put_streaminfo(&writer, spec);
    for (uint32_t number = 0; number * spec.block_size < spec.frames; number++) {
        uint32_t left = spec.frames - number * spec.block_size;
        put_frame(&writer, spec, output.samples, number,
                  left < spec.block_size ? left : spec.block_size);
    }
    output.bytes = writer.bytes;
    output.size = writer.length;
    return output;
}

void flac_writer_free(FlacWriterOutput *output)
{
    free(output->bytes);
    free(output->samples);
    *output = (FlacWriterOutput){0};
}
