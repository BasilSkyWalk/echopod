#include "jpeg_internal.h"

enum {
    BYTE_FF = 0xFF,
    RESTART_FIRST = 0xD0,
    RESTART_LAST = 0xD7,
    FILL_LIMIT = 24,
    BITS_PER_BYTE = 8,
    FAST_LENGTH_SHIFT = 8,
    FAST_SYMBOL_MASK = 0xFF,
    MAX_VALUE_BITS = 16,
    PADDING_LIMIT = 8
};

const uint8_t JPEG_ZIGZAG[JPEG_BLOCK] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

static void fill_fast(JpegHuffman *table, uint32_t code, uint32_t length, uint8_t symbol)
{
    uint32_t first = code << (JPEG_FAST_BITS - length);
    uint32_t count = 1u << (JPEG_FAST_BITS - length);
    for (uint32_t entry = first; entry < first + count; entry++) {
        table->fast[entry] = (uint16_t)(length << FAST_LENGTH_SHIFT | symbol);
    }
}

bool jpeg_build_huffman(JpegHuffman *table, const uint8_t *counts, const uint8_t *values,
                        uint32_t total)
{
    uint32_t code = 0;
    uint32_t index = 0;
    for (uint32_t entry = 0; entry < (1u << JPEG_FAST_BITS); entry++) {
        table->fast[entry] = 0;
    }
    for (uint32_t value = 0; value < total; value++) {
        table->values[value] = values[value];
    }
    for (uint32_t length = 1; length <= JPEG_MAX_CODE_BITS; length++) {
        table->offset[length] = (int32_t)index - (int32_t)code;
        for (uint32_t count = 0; count < counts[length - 1]; count++) {
            if (code >= (1u << length)) {
                return false;
            }
            if (length <= JPEG_FAST_BITS) {
                fill_fast(table, code, length, values[index]);
            }
            code++;
            index++;
        }
        table->max_code[length] = counts[length - 1] != 0 ? (int32_t)code - 1 : -1;
        code <<= 1;
    }
    table->max_code[JPEG_MAX_CODE_BITS + 1] = INT32_MAX;
    table->present = true;
    return true;
}

void jpeg_bits_reset(JpegBits *bits, ImageReader *reader)
{
    *bits = (JpegBits){.reader = reader, .buffer = 0, .count = 0, .padding = 0, .marker = false};
}

static uint32_t next_byte(JpegBits *bits)
{
    ImageReader *reader = bits->reader;
    while (!bits->marker) {
        if (!image_reader_available(reader, 1)) {
            bits->marker = true;
            break;
        }
        uint8_t byte = image_reader_peek(reader, 0);
        if (byte != BYTE_FF) {
            reader->cursor++;
            return byte;
        }
        uint8_t following = image_reader_peek(reader, 1);
        if (following == 0x00 && image_reader_available(reader, 2)) {
            reader->cursor += 2;
            return BYTE_FF;
        }
        if (following == BYTE_FF) {
            reader->cursor++;
            continue;
        }
        bits->marker = true;
    }
    if (++bits->padding > PADDING_LIMIT) {
        bits->failed = true;
    }
    return 0;
}

static void fill(JpegBits *bits)
{
    while (bits->count <= FILL_LIMIT) {
        bits->buffer |= next_byte(bits) << (FILL_LIMIT - bits->count);
        bits->count += BITS_PER_BYTE;
    }
}

static void consume(JpegBits *bits, uint32_t count)
{
    bits->buffer <<= count;
    bits->count -= (int32_t)count;
}

uint32_t jpeg_read_bits(JpegBits *bits, uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    fill(bits);
    uint32_t value = bits->buffer >> (32 - count);
    consume(bits, count);
    return value;
}

int32_t jpeg_decode_symbol(JpegBits *bits, const JpegHuffman *table)
{
    fill(bits);
    uint16_t entry = table->fast[bits->buffer >> (32 - JPEG_FAST_BITS)];
    if (entry != 0) {
        consume(bits, entry >> FAST_LENGTH_SHIFT);
        return entry & FAST_SYMBOL_MASK;
    }
    for (uint32_t length = JPEG_FAST_BITS + 1; length <= JPEG_MAX_CODE_BITS; length++) {
        int32_t code = (int32_t)(bits->buffer >> (32 - length));
        if (code <= table->max_code[length]) {
            int32_t index = code + table->offset[length];
            consume(bits, length);
            return index >= 0 && index < 256 ? table->values[index] : -1;
        }
    }
    bits->failed = true;
    return -1;
}

int32_t jpeg_receive_extend(JpegBits *bits, uint32_t size)
{
    if (size == 0) {
        return 0;
    }
    if (size > MAX_VALUE_BITS) {
        bits->failed = true;
        return 0;
    }
    int32_t value = (int32_t)jpeg_read_bits(bits, size);
    return value < (1 << (size - 1)) ? value - (1 << size) + 1 : value;
}

void jpeg_skip_entropy(ImageReader *reader)
{
    while (image_reader_available(reader, 2)) {
        uint8_t byte = image_reader_peek(reader, 0);
        uint8_t following = image_reader_peek(reader, 1);
        if (byte != BYTE_FF) {
            reader->cursor++;
        } else if (following == 0x00 || (following >= RESTART_FIRST && following <= RESTART_LAST)) {
            reader->cursor += 2;
        } else if (following == BYTE_FF) {
            reader->cursor++;
        } else {
            return;
        }
    }
}

void jpeg_restart(JpegBits *bits)
{
    ImageReader *reader = bits->reader;
    bits->buffer = 0;
    bits->count = 0;
    bits->padding = 0;
    bits->marker = false;
    while (image_reader_available(reader, 2)) {
        uint8_t byte = image_reader_peek(reader, 0);
        uint8_t following = image_reader_peek(reader, 1);
        if (byte == BYTE_FF && following >= RESTART_FIRST && following <= RESTART_LAST) {
            reader->cursor += 2;
            return;
        }
        if (byte == BYTE_FF && following != 0x00 && following != BYTE_FF) {
            return;
        }
        reader->cursor++;
    }
}
