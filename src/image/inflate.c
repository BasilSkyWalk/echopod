#include "image/inflate.h"

#include <stddef.h>

enum {
    WINDOW_MASK = INFLATE_WINDOW - 1,
    FLUSH_THRESHOLD = INFLATE_WINDOW / 2,
    MAX_PADDING_BYTES = 4,
    BITS_PER_BYTE = 8,
    END_OF_BLOCK = 256,
    FIRST_LENGTH = 257,
    LENGTH_SYMBOLS = 29,
    DISTANCE_SYMBOLS = 30,
    CODE_LENGTH_CODES = 19,
    FIXED_LITERALS = 288,
    FIXED_DISTANCES = 30,
    REPEAT_PREVIOUS = 16,
    REPEAT_ZERO_SHORT = 17,
    REPEAT_ZERO_LONG = 18,
    FAST_LENGTH_SHIFT = 9,
    FAST_SYMBOL_MASK = (1 << FAST_LENGTH_SHIFT) - 1,
    STORED = 0,
    FIXED = 1,
    DYNAMIC = 2
};

static const uint16_t LENGTH_BASE[LENGTH_SYMBOLS] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                                     15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                                     67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t LENGTH_EXTRA[LENGTH_SYMBOLS] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                     2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t DISTANCE_BASE[DISTANCE_SYMBOLS] = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t DISTANCE_EXTRA[DISTANCE_SYMBOLS] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                                         4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                                         9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const uint8_t CODE_LENGTH_ORDER[CODE_LENGTH_CODES] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                             11, 4,  12, 3, 13, 2, 14, 1, 15};

bool inflate_init(Inflate *inflate, InflateInput input, InflateOutput output, ImageArena *arena)
{
    inflate->input = input;
    inflate->output = output;
    inflate->bits = 0;
    inflate->bit_count = 0;
    inflate->padding_bits = 0;
    inflate->position = 0;
    inflate->flushed = 0;
    inflate->failed = false;
    inflate->stopped = false;
    inflate->window = image_arena_take(arena, INFLATE_WINDOW);
    return inflate->window != NULL;
}

static bool fill(Inflate *inflate, uint32_t needed)
{
    while (inflate->bit_count < needed) {
        uint8_t byte = 0;
        if (!inflate->input.next(inflate->input.context, &byte)) {
            if (inflate->padding_bits >= MAX_PADDING_BYTES * BITS_PER_BYTE) {
                inflate->failed = true;
                return false;
            }
            inflate->padding_bits += BITS_PER_BYTE;
        }
        inflate->bits |= (uint32_t)byte << inflate->bit_count;
        inflate->bit_count += BITS_PER_BYTE;
    }
    return true;
}

static void consume(Inflate *inflate, uint32_t count)
{
    inflate->bits >>= count;
    inflate->bit_count -= count;
    if (inflate->bit_count < inflate->padding_bits) {
        inflate->failed = true;
    }
}

static uint32_t take_bits(Inflate *inflate, uint32_t count)
{
    if (count == 0 || !fill(inflate, count)) {
        return 0;
    }
    uint32_t value = inflate->bits & ((1u << count) - 1u);
    consume(inflate, count);
    return value;
}

static uint32_t reversed(uint32_t code, uint32_t length)
{
    uint32_t result = 0;
    for (uint32_t bit = 0; bit < length; bit++) {
        result = result << 1 | (code >> bit & 1u);
    }
    return result;
}

static void fill_fast(InflateCode *code, const uint8_t *lengths, uint32_t count)
{
    uint32_t next_code[INFLATE_MAX_BITS + 1];
    uint32_t value = 0;
    for (uint32_t length = 1; length <= INFLATE_MAX_BITS; length++) {
        value = (value + code->counts[length - 1]) << 1;
        next_code[length] = value;
    }
    for (uint32_t entry = 0; entry < (1u << INFLATE_FAST_BITS); entry++) {
        code->fast[entry] = 0;
    }
    for (uint32_t symbol = 0; symbol < count; symbol++) {
        uint32_t length = lengths[symbol];
        if (length == 0) {
            continue;
        }
        uint32_t bits = reversed(next_code[length]++, length);
        if (length > INFLATE_FAST_BITS) {
            continue;
        }
        for (uint32_t entry = bits; entry < (1u << INFLATE_FAST_BITS); entry += 1u << length) {
            code->fast[entry] = (uint16_t)(length << FAST_LENGTH_SHIFT | symbol);
        }
    }
}

static bool build_code(InflateCode *code, const uint8_t *lengths, uint32_t count)
{
    uint16_t offsets[INFLATE_MAX_BITS + 2];
    for (uint32_t length = 0; length <= INFLATE_MAX_BITS; length++) {
        code->counts[length] = 0;
    }
    for (uint32_t symbol = 0; symbol < count; symbol++) {
        code->counts[lengths[symbol]]++;
    }
    code->counts[0] = 0;
    int32_t left = 1;
    for (uint32_t length = 1; length <= INFLATE_MAX_BITS; length++) {
        left = left * 2 - code->counts[length];
        if (left < 0) {
            return false;
        }
    }
    offsets[1] = 0;
    for (uint32_t length = 1; length <= INFLATE_MAX_BITS; length++) {
        offsets[length + 1] = (uint16_t)(offsets[length] + code->counts[length]);
    }
    for (uint32_t symbol = 0; symbol < count; symbol++) {
        if (lengths[symbol] != 0) {
            code->symbols[offsets[lengths[symbol]]++] = (uint16_t)symbol;
        }
    }
    fill_fast(code, lengths, count);
    return true;
}

static int32_t decode_slow(Inflate *inflate, const InflateCode *code)
{
    int32_t value = 0;
    int32_t first = 0;
    int32_t index = 0;
    for (uint32_t length = 1; length <= INFLATE_MAX_BITS; length++) {
        value |= (int32_t)take_bits(inflate, 1);
        int32_t count = code->counts[length];
        if (value - first < count) {
            return code->symbols[index + value - first];
        }
        index += count;
        first = (first + count) << 1;
        value <<= 1;
    }
    inflate->failed = true;
    return -1;
}

static int32_t decode(Inflate *inflate, const InflateCode *code)
{
    if (!fill(inflate, INFLATE_FAST_BITS)) {
        return -1;
    }
    uint16_t entry = code->fast[inflate->bits & ((1u << INFLATE_FAST_BITS) - 1u)];
    if (entry == 0) {
        return decode_slow(inflate, code);
    }
    consume(inflate, entry >> FAST_LENGTH_SHIFT);
    return entry & FAST_SYMBOL_MASK;
}

static void write_segment(Inflate *inflate, uint32_t from, uint32_t to)
{
    if (inflate->stopped || from == to) {
        return;
    }
    if (!inflate->output.write(inflate->output.context, inflate->window + from, to - from)) {
        inflate->stopped = true;
    }
}

static void flush(Inflate *inflate)
{
    uint32_t from = inflate->flushed & WINDOW_MASK;
    uint32_t pending = inflate->position - inflate->flushed;
    uint32_t first = INFLATE_WINDOW - from < pending ? INFLATE_WINDOW - from : pending;
    write_segment(inflate, from, from + first);
    write_segment(inflate, 0, pending - first);
    inflate->flushed = inflate->position;
}

static void emit(Inflate *inflate, uint8_t byte)
{
    inflate->window[inflate->position++ & WINDOW_MASK] = byte;
    if (inflate->position - inflate->flushed >= FLUSH_THRESHOLD) {
        flush(inflate);
    }
}

static bool copy_match(Inflate *inflate, uint32_t length, uint32_t distance)
{
    uint32_t available = inflate->position < INFLATE_WINDOW ? inflate->position : INFLATE_WINDOW;
    if (distance == 0 || distance > available) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        emit(inflate, inflate->window[(inflate->position - distance) & WINDOW_MASK]);
    }
    return true;
}

static bool decode_match(Inflate *inflate, int32_t symbol)
{
    uint32_t length_index = (uint32_t)symbol - FIRST_LENGTH;
    if (length_index >= LENGTH_SYMBOLS) {
        return false;
    }
    uint32_t length = LENGTH_BASE[length_index] + take_bits(inflate, LENGTH_EXTRA[length_index]);
    int32_t distance_symbol = decode(inflate, &inflate->distances);
    if (distance_symbol < 0 || distance_symbol >= DISTANCE_SYMBOLS) {
        return false;
    }
    uint32_t distance =
        DISTANCE_BASE[distance_symbol] + take_bits(inflate, DISTANCE_EXTRA[distance_symbol]);
    return !inflate->failed && copy_match(inflate, length, distance);
}

static bool decode_block(Inflate *inflate)
{
    while (!inflate->failed && !inflate->stopped) {
        int32_t symbol = decode(inflate, &inflate->lengths);
        if (symbol < 0) {
            return false;
        }
        if (symbol == END_OF_BLOCK) {
            return true;
        }
        if (symbol < END_OF_BLOCK) {
            emit(inflate, (uint8_t)symbol);
        } else if (!decode_match(inflate, symbol)) {
            return false;
        }
    }
    return !inflate->failed;
}

static bool stored_block(Inflate *inflate)
{
    consume(inflate, inflate->bit_count % BITS_PER_BYTE);
    uint32_t length = take_bits(inflate, 16);
    uint32_t inverse = take_bits(inflate, 16);
    if (inflate->failed || (length ^ 0xFFFFu) != inverse) {
        return false;
    }
    for (uint32_t index = 0; index < length && !inflate->failed && !inflate->stopped; index++) {
        emit(inflate, (uint8_t)take_bits(inflate, BITS_PER_BYTE));
    }
    return !inflate->failed;
}

static bool fixed_codes(Inflate *inflate)
{
    uint8_t lengths[FIXED_LITERALS];
    for (uint32_t symbol = 0; symbol < FIXED_LITERALS; symbol++) {
        lengths[symbol] = symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
    }
    if (!build_code(&inflate->lengths, lengths, FIXED_LITERALS)) {
        return false;
    }
    for (uint32_t symbol = 0; symbol < FIXED_DISTANCES; symbol++) {
        lengths[symbol] = 5;
    }
    return build_code(&inflate->distances, lengths, FIXED_DISTANCES);
}

static bool read_code_lengths(Inflate *inflate, uint8_t *lengths, uint32_t total)
{
    uint32_t count = 0;
    while (count < total && !inflate->failed) {
        int32_t symbol = decode(inflate, &inflate->distances);
        uint32_t repeat = 0;
        uint8_t value = 0;
        if (symbol < 0) {
            return false;
        } else if (symbol < REPEAT_PREVIOUS) {
            lengths[count++] = (uint8_t)symbol;
            continue;
        } else if (symbol == REPEAT_PREVIOUS) {
            if (count == 0) {
                return false;
            }
            value = lengths[count - 1];
            repeat = 3 + take_bits(inflate, 2);
        } else {
            repeat = symbol == REPEAT_ZERO_SHORT ? 3 + take_bits(inflate, 3)
                                                 : 11 + take_bits(inflate, 7);
        }
        if (count + repeat > total) {
            return false;
        }
        while (repeat-- > 0) {
            lengths[count++] = value;
        }
    }
    return !inflate->failed;
}

static bool dynamic_codes(Inflate *inflate)
{
    uint8_t lengths[INFLATE_LENGTH_CODES + INFLATE_DISTANCE_CODES];
    uint32_t literal_count = take_bits(inflate, 5) + 257;
    uint32_t distance_count = take_bits(inflate, 5) + 1;
    uint32_t length_codes = take_bits(inflate, 4) + 4;
    if (literal_count > 286 || distance_count > DISTANCE_SYMBOLS) {
        return false;
    }
    for (uint32_t index = 0; index < CODE_LENGTH_CODES; index++) {
        lengths[CODE_LENGTH_ORDER[index]] =
            index < length_codes ? (uint8_t)take_bits(inflate, 3) : 0;
    }
    if (inflate->failed || !build_code(&inflate->distances, lengths, CODE_LENGTH_CODES) ||
        !read_code_lengths(inflate, lengths, literal_count + distance_count) ||
        lengths[END_OF_BLOCK] == 0) {
        return false;
    }
    return build_code(&inflate->lengths, lengths, literal_count) &&
           build_code(&inflate->distances, lengths + literal_count, distance_count);
}

static bool run_block(Inflate *inflate, uint32_t type)
{
    if (type == STORED) {
        return stored_block(inflate);
    }
    if (type == FIXED) {
        return fixed_codes(inflate) && decode_block(inflate);
    }
    return type == DYNAMIC && dynamic_codes(inflate) && decode_block(inflate);
}

InflateResult inflate_run(Inflate *inflate)
{
    bool last = false;
    while (!last && !inflate->stopped) {
        last = take_bits(inflate, 1) != 0;
        uint32_t type = take_bits(inflate, 2);
        if (inflate->failed || !run_block(inflate, type)) {
            return inflate->stopped ? INFLATE_STOPPED : INFLATE_ERROR;
        }
    }
    flush(inflate);
    return inflate->stopped ? INFLATE_STOPPED : INFLATE_DONE;
}
