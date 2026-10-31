#include "codec/alac.h"

#include <stddef.h>

enum {
    ELEMENT_SINGLE = 0,
    ELEMENT_PAIR = 1,
    ELEMENT_LFE = 3,
    ELEMENT_DATA = 4,
    ELEMENT_FILL = 6,
    ELEMENT_END = 7,
    ELEMENT_TAG_BITS = 3,
    INSTANCE_BITS = 4,
    UNUSED_BITS = 12,
    HEADER_BITS = 4,
    PARTIAL_FLAG = 8,
    ESCAPE_FLAG = 1,
    SHIFT_MASK = 3,
    BYTE_BITS = 8,
    WORD_BITS = 32,
    PEEK_BITS = 25,
    SPLIT_BITS = 16,
    PEEK_BYTES = 4,
    COEFFICIENT_BITS = 16,
    ORDER_MASK = 31,
    SHORTCUT_ORDER = 31,
    UNROLLED_ORDERS = 8,
    DEN_SHIFT_MASK = 15,
    PB_FACTOR_SHIFT = 5,
    MODE_SHIFT = 4,
    QB_SHIFT = 9,
    QB = 1 << QB_SHIFT,
    MMUL_SHIFT = 2,
    MDEN_SHIFT = QB_SHIFT - MMUL_SHIFT - 1,
    MOFF = 1 << (MDEN_SHIFT - 2),
    BIT_OFF = 24,
    MAX_PREFIX = 9,
    RUN_ESCAPE_BITS = 16,
    MEAN_CLAMP = 0xFFFF,
    LONG_RUN = 65535,
    LARGE_DATA_COUNT = 255,
    LARGE_FILL_COUNT = 15,
    MAX_KB = 31,
    MAX_MIX_BITS = 31
};

typedef struct Prediction {
    uint32_t mode;
    uint32_t den_shift;
    uint32_t pb_factor;
    uint32_t order;
    int16_t coefficients[ALAC_MAX_COEFFICIENTS];
} Prediction;

static uint32_t big_endian(const uint8_t *bytes, int count)
{
    uint32_t value = 0;
    for (int index = 0; index < count; index++) {
        value = value << BYTE_BITS | bytes[index];
    }
    return value;
}

bool alac_parse_config(const uint8_t *bytes, uint32_t length, AlacConfig *config)
{
    if (length < ALAC_CONFIG_BYTES || bytes[4] != 0) {
        return false;
    }
    *config = (AlacConfig){
        .frame_length = big_endian(bytes, 4),
        .bit_depth = bytes[5],
        .pb = bytes[6],
        .mb = bytes[7],
        .kb = bytes[8],
        .channels = bytes[9],
        .max_run = (uint16_t)big_endian(bytes + 10, 2),
        .sample_rate = big_endian(bytes + 20, 4),
    };
    bool depth_ok = config->bit_depth == 16 || config->bit_depth == 20 || config->bit_depth == 24;
    return depth_ok && config->channels >= 1 && config->channels <= FLAC_MAX_CHANNELS &&
           config->frame_length > 0 && config->frame_length <= FLAC_MAX_BLOCK &&
           config->sample_rate > 0 && config->kb <= MAX_KB;
}

void alac_start(AlacDecoder *decoder, AlacConfig config)
{
    decoder->config = config;
    decoder->block_samples = 0;
}

const int32_t *alac_channel(const AlacDecoder *decoder, int channel)
{
    return decoder->samples[channel];
}

static void reader_fill(AlacReader *reader)
{
    uint32_t used = reader->bit / BYTE_BITS;
    used = used < reader->length ? used : reader->length;
    uint32_t kept = reader->length - used;
    for (uint32_t index = 0; index < kept; index++) {
        reader->buffer[index] = reader->buffer[used + index];
    }
    reader->base += used;
    reader->bit -= used * BYTE_BITS;
    reader->length = kept;
    uint32_t next = reader->base + kept;
    uint32_t available = reader->end > next ? reader->end - next : 0;
    uint32_t room = ALAC_READER_BYTES - kept;
    uint32_t wanted = available < room ? available : room;
    if (wanted > 0) {
        if (reader->input.read(reader->input.context, next, reader->buffer + kept, wanted)) {
            reader->length += wanted;
        } else {
            reader->failed = true;
        }
    }
    for (uint32_t index = 0; index < ALAC_READER_PADDING; index++) {
        reader->buffer[reader->length + index] = 0;
    }
}

static void reader_load(AlacReader *reader, uint32_t offset)
{
    reader->base = offset;
    reader->length = 0;
    reader->bit = 0;
    reader_fill(reader);
}

static inline uint32_t reader_peek(AlacReader *reader)
{
    uint32_t at = reader->bit / BYTE_BITS;
    if (at + PEEK_BYTES > reader->length && reader->base + reader->length < reader->end) {
        reader_fill(reader);
        at = reader->bit / BYTE_BITS;
    }
    if (at > reader->length) {
        reader->failed = true;
        return 0;
    }
    uint32_t word = 0;
    __builtin_memcpy(&word, reader->buffer + at, sizeof word);
    return __builtin_bswap32(word) << (reader->bit % BYTE_BITS);
}

static inline uint32_t reader_read(AlacReader *reader, uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    uint32_t value = reader_peek(reader) >> (WORD_BITS - count);
    reader->bit += count;
    return value;
}

static uint32_t reader_read_wide(AlacReader *reader, uint32_t count)
{
    if (count <= PEEK_BITS) {
        return reader_read(reader, count);
    }
    uint32_t high = reader_read(reader, count - SPLIT_BITS);
    return high << SPLIT_BITS | reader_read(reader, SPLIT_BITS);
}

static int32_t reader_read_signed(AlacReader *reader, uint32_t count)
{
    uint32_t value = reader_read_wide(reader, count);
    uint32_t sign = 1u << (count - 1);
    return (int32_t)(value ^ sign) - (int32_t)sign;
}

static uint64_t reader_position(const AlacReader *reader)
{
    return (uint64_t)reader->base * BYTE_BITS + reader->bit;
}

static void reader_seek(AlacReader *reader, uint64_t position)
{
    uint64_t start = (uint64_t)reader->base * BYTE_BITS;
    uint64_t limit = (uint64_t)(reader->base + reader->length) * BYTE_BITS;
    if (position >= start && position < limit) {
        reader->bit = (uint32_t)(position - start);
        return;
    }
    reader_load(reader, (uint32_t)(position / BYTE_BITS));
    reader->bit = (uint32_t)(position % BYTE_BITS);
}

static bool reader_failed(const AlacReader *reader)
{
    return reader->failed || reader_position(reader) > (uint64_t)reader->end * BYTE_BITS;
}

static uint32_t leading_zeros(uint32_t value)
{
    return value == 0 ? WORD_BITS : (uint32_t)__builtin_clz(value);
}

static uint32_t read_slow_tail(AlacReader *reader, uint32_t base, uint32_t k)
{
    uint32_t high = reader_read_wide(reader, k - 1);
    if (high == 0) {
        return base;
    }
    uint32_t value = high << 1 | reader_read(reader, 1);
    return base + value - 1;
}

static inline uint32_t read_value(AlacReader *reader, uint32_t m, uint32_t k, uint32_t escape_bits)
{
    uint32_t stream = reader_peek(reader);
    uint32_t prefix = leading_zeros(~stream);
    if (prefix >= MAX_PREFIX) {
        reader->bit += MAX_PREFIX;
        return reader_read_wide(reader, escape_bits);
    }
    uint32_t base = prefix * m;
    if (prefix + 1 + k > PEEK_BITS) {
        reader->bit += prefix + 1;
        return read_slow_tail(reader, base, k);
    }
    uint32_t tail = (stream << (prefix + 1)) >> (WORD_BITS - k);
    if (tail >= 2) {
        reader->bit += prefix + 1 + k;
        return base + tail - 1;
    }
    reader->bit += prefix + k;
    return base;
}

static bool read_residuals(AlacReader *reader, const AlacConfig *config, uint32_t pb, int32_t *out,
                           uint32_t count, uint32_t escape_bits)
{
    uint32_t mb = config->mb;
    uint32_t kb = config->kb;
    uint32_t wb = (1u << kb) - 1;
    uint32_t zero_mode = 0;
    uint32_t done = 0;
    while (done < count && !reader->failed) {
        uint32_t k = WORD_BITS - 1 - leading_zeros((mb >> QB_SHIFT) + 3);
        k = k < kb ? k : kb;
        uint32_t n = read_value(reader, (1u << k) - 1, k, escape_bits);
        uint32_t coded = n + zero_mode;
        int32_t magnitude = (int32_t)((coded + 1) >> 1);
        out[done++] = (coded & 1u) != 0 ? -magnitude : magnitude;
        mb = pb * coded + mb - ((pb * mb) >> QB_SHIFT);
        if (n > MEAN_CLAMP) {
            mb = MEAN_CLAMP;
        }
        zero_mode = 0;
        if ((mb << MMUL_SHIFT) < QB && done < count) {
            zero_mode = 1;
            uint32_t run_k = leading_zeros(mb) - BIT_OFF + ((mb + MOFF) >> MDEN_SHIFT);
            uint32_t run_m = ((1u << run_k) - 1) & wb;
            uint32_t run = read_value(reader, run_m, run_k, RUN_ESCAPE_BITS);
            if (run > count - done) {
                return false;
            }
            for (uint32_t index = 0; index < run; index++) {
                out[done++] = 0;
            }
            if (run >= LONG_RUN) {
                zero_mode = 0;
            }
            mb = 0;
        }
    }
    return !reader_failed(reader);
}

static inline int32_t sign_of(int32_t value)
{
    return (value > 0) - (value < 0);
}

static inline int32_t extend(uint32_t value, uint32_t shift)
{
    return (int32_t)(value << shift) >> shift;
}

static inline __attribute__((always_inline)) void restore_fixed(int32_t *samples, uint32_t count,
                                                                int16_t *coefficients,
                                                                const uint32_t order,
                                                                uint32_t shift, uint32_t den_shift)
{
    int32_t taps[UNROLLED_ORDERS];
    int32_t gaps[UNROLLED_ORDERS];
    uint32_t half = den_shift > 0 ? 1u << (den_shift - 1) : 0;
    for (uint32_t tap = 0; tap < order; tap++) {
        taps[tap] = coefficients[tap];
    }
    for (uint32_t index = order + 1; index < count; index++) {
        const int32_t *previous = samples + index - 1;
        int32_t top = samples[index - order - 1];
        uint32_t sum = 0;
        for (uint32_t tap = 0; tap < order; tap++) {
            gaps[tap] = top - previous[-(int32_t)tap];
            sum -= (uint32_t)taps[tap] * (uint32_t)gaps[tap];
        }
        int32_t residual = samples[index];
        int32_t predicted = (int32_t)(sum + half) >> den_shift;
        samples[index] = extend((uint32_t)residual + (uint32_t)top + (uint32_t)predicted, shift);
        if (residual > 0) {
            for (int32_t tap = (int32_t)order - 1; tap >= 0; tap--) {
                int32_t step = sign_of(gaps[tap]);
                taps[tap] = (int16_t)(taps[tap] - step);
                uint32_t change =
                    (order - (uint32_t)tap) * (uint32_t)((step * gaps[tap]) >> den_shift);
                residual = (int32_t)((uint32_t)residual - change);
                if (residual <= 0) {
                    break;
                }
            }
        } else if (residual < 0) {
            for (int32_t tap = (int32_t)order - 1; tap >= 0; tap--) {
                int32_t step = -sign_of(gaps[tap]);
                taps[tap] = (int16_t)(taps[tap] - step);
                uint32_t change =
                    (order - (uint32_t)tap) * (uint32_t)((step * gaps[tap]) >> den_shift);
                residual = (int32_t)((uint32_t)residual - change);
                if (residual >= 0) {
                    break;
                }
            }
        }
    }
    for (uint32_t tap = 0; tap < order; tap++) {
        coefficients[tap] = (int16_t)taps[tap];
    }
}

static void adapt(int16_t *coefficients, const int32_t *previous, int32_t top, uint32_t order,
                  int32_t residual, uint32_t den_shift)
{
    int32_t sign = sign_of(residual);
    int32_t remaining = residual;
    for (int32_t tap = (int32_t)order - 1; tap >= 0 && sign != 0; tap--) {
        int32_t difference = top - previous[-tap];
        int32_t step = sign_of(difference) * sign;
        coefficients[tap] = (int16_t)(coefficients[tap] - step);
        uint32_t weight = order - (uint32_t)tap;
        uint32_t change = weight * (uint32_t)((step * difference) >> den_shift);
        remaining = (int32_t)((uint32_t)remaining - change);
        if ((sign > 0 && remaining <= 0) || (sign < 0 && remaining >= 0)) {
            return;
        }
    }
}

static void restore_general(int32_t *samples, uint32_t count, int16_t *coefficients, uint32_t order,
                            uint32_t shift, uint32_t den_shift)
{
    uint32_t half = den_shift > 0 ? 1u << (den_shift - 1) : 0;
    for (uint32_t index = order + 1; index < count; index++) {
        const int32_t *previous = samples + index - 1;
        int32_t top = samples[index - order - 1];
        uint32_t sum = 0;
        for (uint32_t tap = 0; tap < order; tap++) {
            sum += (uint32_t)(int32_t)coefficients[tap] * (uint32_t)(previous[-(int32_t)tap] - top);
        }
        int32_t residual = samples[index];
        int32_t predicted = (int32_t)(sum + half) >> den_shift;
        samples[index] = extend((uint32_t)residual + (uint32_t)top + (uint32_t)predicted, shift);
        adapt(coefficients, previous, top, order, residual, den_shift);
    }
}

static void restore(int32_t *samples, uint32_t count, int16_t *coefficients, uint32_t order,
                    uint32_t chan_bits, uint32_t den_shift)
{
    uint32_t shift = WORD_BITS - chan_bits;
    if (order == 0 || count < 2) {
        return;
    }
    if (order == SHORTCUT_ORDER) {
        for (uint32_t index = 1; index < count; index++) {
            samples[index] = extend((uint32_t)samples[index] + (uint32_t)samples[index - 1], shift);
        }
        return;
    }
    for (uint32_t index = 1; index <= order && index < count; index++) {
        samples[index] = extend((uint32_t)samples[index] + (uint32_t)samples[index - 1], shift);
    }
    switch (order) {
    case 1:
        restore_fixed(samples, count, coefficients, 1, shift, den_shift);
        break;
    case 2:
        restore_fixed(samples, count, coefficients, 2, shift, den_shift);
        break;
    case 3:
        restore_fixed(samples, count, coefficients, 3, shift, den_shift);
        break;
    case 4:
        restore_fixed(samples, count, coefficients, 4, shift, den_shift);
        break;
    case 5:
        restore_fixed(samples, count, coefficients, 5, shift, den_shift);
        break;
    case 6:
        restore_fixed(samples, count, coefficients, 6, shift, den_shift);
        break;
    case 7:
        restore_fixed(samples, count, coefficients, 7, shift, den_shift);
        break;
    case 8:
        restore_fixed(samples, count, coefficients, 8, shift, den_shift);
        break;
    default:
        restore_general(samples, count, coefficients, order, shift, den_shift);
        break;
    }
}

static void read_prediction(AlacReader *reader, Prediction *prediction)
{
    uint32_t header = reader_read(reader, BYTE_BITS);
    prediction->mode = header >> MODE_SHIFT;
    prediction->den_shift = header & DEN_SHIFT_MASK;
    header = reader_read(reader, BYTE_BITS);
    prediction->pb_factor = header >> PB_FACTOR_SHIFT;
    prediction->order = header & ORDER_MASK;
    for (uint32_t tap = 0; tap < prediction->order; tap++) {
        prediction->coefficients[tap] = (int16_t)reader_read(reader, COEFFICIENT_BITS);
    }
}

static bool decode_channel(AlacDecoder *decoder, Prediction *prediction, int32_t *samples,
                           uint32_t count, uint32_t chan_bits)
{
    uint32_t pb = decoder->config.pb * prediction->pb_factor / 4;
    if (!read_residuals(&decoder->reader, &decoder->config, pb, samples, count, chan_bits)) {
        return false;
    }
    if (prediction->mode != 0) {
        restore(samples, count, NULL, SHORTCUT_ORDER, chan_bits, 0);
    }
    restore(samples, count, prediction->coefficients, prediction->order, chan_bits,
            prediction->den_shift);
    return true;
}

static void read_verbatim(AlacReader *reader, int32_t **channels, uint32_t count, uint32_t samples,
                          uint32_t chan_bits)
{
    for (uint32_t index = 0; index < samples; index++) {
        for (uint32_t channel = 0; channel < count; channel++) {
            channels[channel][index] = reader_read_signed(reader, chan_bits);
        }
    }
}

static void unmix(int32_t *left, int32_t *right, uint32_t samples, uint32_t mix_bits,
                  int32_t mix_res)
{
    for (uint32_t index = 0; index < samples; index++) {
        int32_t u = left[index];
        int32_t v = right[index];
        int32_t weighted = (int32_t)((uint32_t)mix_res * (uint32_t)v) >> mix_bits;
        int32_t l = (int32_t)((uint32_t)u + (uint32_t)v - (uint32_t)weighted);
        left[index] = l;
        right[index] = (int32_t)((uint32_t)l - (uint32_t)v);
    }
}

static void merge_shifted(AlacReader *reader, int32_t **channels, uint32_t count, uint32_t samples,
                          uint32_t shift)
{
    for (uint32_t index = 0; index < samples; index++) {
        for (uint32_t channel = 0; channel < count; channel++) {
            uint32_t low = reader_read(reader, shift);
            channels[channel][index] = (int32_t)((uint32_t)channels[channel][index] << shift | low);
        }
    }
}

static AlacResult decode_element(AlacDecoder *decoder, uint32_t first, uint32_t count)
{
    AlacReader *reader = &decoder->reader;
    const AlacConfig *config = &decoder->config;
    reader_read(reader, INSTANCE_BITS);
    if (reader_read(reader, UNUSED_BITS) != 0) {
        return ALAC_ERROR;
    }
    uint32_t header = reader_read(reader, HEADER_BITS);
    uint32_t bytes_shifted = header >> 1 & SHIFT_MASK;
    uint32_t samples = config->frame_length;
    if ((header & PARTIAL_FLAG) != 0) {
        samples = reader_read_wide(reader, WORD_BITS);
    }
    bool sizes_ok = samples > 0 && samples <= config->frame_length &&
                    (decoder->block_samples == 0 || samples == decoder->block_samples);
    if (bytes_shifted == SHIFT_MASK || !sizes_ok) {
        return ALAC_ERROR;
    }
    decoder->block_samples = samples;
    int32_t *channels[FLAC_MAX_CHANNELS] = {decoder->samples[first], NULL};
    if (count == 2) {
        channels[1] = decoder->samples[first + 1];
    }
    if ((header & ESCAPE_FLAG) != 0) {
        read_verbatim(reader, channels, count, samples, config->bit_depth);
        return reader_failed(reader) ? ALAC_ERROR : ALAC_OK;
    }
    uint32_t shift = bytes_shifted * BYTE_BITS;
    uint32_t chan_bits = config->bit_depth - shift + count - 1;
    uint32_t mix_bits = reader_read(reader, BYTE_BITS);
    int32_t mix_res = (int8_t)reader_read(reader, BYTE_BITS);
    Prediction predictions[FLAC_MAX_CHANNELS];
    for (uint32_t channel = 0; channel < count; channel++) {
        read_prediction(reader, &predictions[channel]);
    }
    if (mix_bits > MAX_MIX_BITS || config->kb == 0 || reader_failed(reader)) {
        return ALAC_ERROR;
    }
    uint64_t shifted_at = reader_position(reader);
    if (shift > 0) {
        reader_seek(reader, shifted_at + (uint64_t)shift * count * samples);
    }
    for (uint32_t channel = 0; channel < count; channel++) {
        if (!decode_channel(decoder, &predictions[channel], channels[channel], samples,
                            chan_bits)) {
            return ALAC_ERROR;
        }
    }
    if (count == 2 && mix_res != 0) {
        unmix(channels[0], channels[1], samples, mix_bits, mix_res);
    }
    if (shift > 0) {
        uint64_t end = reader_position(reader);
        reader_seek(reader, shifted_at);
        merge_shifted(reader, channels, count, samples, shift);
        reader_seek(reader, end);
    }
    return reader_failed(reader) ? ALAC_ERROR : ALAC_OK;
}

static void skip_bytes(AlacReader *reader, uint32_t count)
{
    reader_seek(reader, reader_position(reader) + (uint64_t)count * BYTE_BITS);
}

static void skip_data(AlacReader *reader)
{
    reader_read(reader, INSTANCE_BITS);
    bool aligned = reader_read(reader, 1) != 0;
    uint32_t count = reader_read(reader, BYTE_BITS);
    if (count == LARGE_DATA_COUNT) {
        count += reader_read(reader, BYTE_BITS);
    }
    if (aligned) {
        uint64_t position = reader_position(reader);
        reader_seek(reader, (position + BYTE_BITS - 1) / BYTE_BITS * BYTE_BITS);
    }
    skip_bytes(reader, count);
}

static void skip_fill(AlacReader *reader)
{
    uint32_t count = reader_read(reader, HEADER_BITS);
    if (count == LARGE_FILL_COUNT) {
        count += reader_read(reader, BYTE_BITS) - 1;
    }
    skip_bytes(reader, count);
}

AlacResult alac_decode_frame(AlacDecoder *decoder, FlacInput input, uint32_t offset,
                             uint32_t length)
{
    if (offset > input.size || length > input.size - offset) {
        return ALAC_ERROR;
    }
    AlacReader *reader = &decoder->reader;
    reader->input = input;
    reader->end = offset + length;
    reader->failed = false;
    reader_load(reader, offset);
    decoder->block_samples = 0;
    uint32_t filled = 0;
    uint32_t channels = decoder->config.channels;
    while (filled < channels) {
        uint32_t tag = reader_read(reader, ELEMENT_TAG_BITS);
        if (reader_failed(reader)) {
            return ALAC_ERROR;
        }
        uint32_t width = tag == ELEMENT_PAIR ? 2 : 1;
        AlacResult result = ALAC_OK;
        switch (tag) {
        case ELEMENT_SINGLE:
        case ELEMENT_LFE:
        case ELEMENT_PAIR:
            if (filled + width > channels) {
                return ALAC_ERROR;
            }
            result = decode_element(decoder, filled, width);
            filled += width;
            break;
        case ELEMENT_DATA:
            skip_data(reader);
            break;
        case ELEMENT_FILL:
            skip_fill(reader);
            break;
        case ELEMENT_END:
            return filled > 0 ? ALAC_OK : ALAC_ERROR;
        default:
            return ALAC_UNSUPPORTED;
        }
        if (result != ALAC_OK) {
            return result;
        }
    }
    return reader_failed(reader) ? ALAC_ERROR : ALAC_OK;
}
