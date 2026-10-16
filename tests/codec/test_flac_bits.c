#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "flac_bits.h"

enum {
    STREAM_CAPACITY = 8 * FLAC_BUFFER_BYTES,
    VALUE_COUNT = 6000
};

typedef struct Writer {
    uint8_t bytes[STREAM_CAPACITY];
    uint32_t bit_count;
} Writer;

static Writer writer;
static FlacBits bits;
static int32_t values[VALUE_COUNT];
static int32_t decoded[VALUE_COUNT];
static uint32_t random_state;

static void put_bit(uint32_t bit)
{
    if (bit != 0) {
        writer.bytes[writer.bit_count / 8] |= (uint8_t)(0x80u >> (writer.bit_count % 8));
    }
    writer.bit_count++;
}

static void put_bits(uint32_t value, uint32_t count)
{
    while (count > 0) {
        count--;
        put_bit((value >> count) & 1u);
    }
}

static void put_rice(int32_t value, uint32_t parameter)
{
    uint32_t folded = value >= 0 ? (uint32_t)value << 1 : ((uint32_t)(-(value + 1)) << 1) | 1u;
    uint32_t quotient = folded >> parameter;
    for (uint32_t zero = 0; zero < quotient; zero++) {
        put_bit(0);
    }
    put_bit(1);
    put_bits(folded, parameter);
}

static uint32_t next_random(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state >> 8;
}

static bool read_stream(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if (offset > STREAM_CAPACITY || length > STREAM_CAPACITY - offset) {
        return false;
    }
    memcpy(buffer, writer.bytes + offset, length);
    return true;
}

static void start_writing(void)
{
    memset(&writer, 0, sizeof writer);
    random_state = 12345;
}

static void start_reading(void)
{
    uint32_t size = (writer.bit_count + 7) / 8;
    flac_bits_start(&bits, (FlacInput){.read = read_stream, .context = NULL, .size = size}, 0);
}

static void write_values(uint32_t count, uint32_t parameter, int32_t magnitude)
{
    for (uint32_t index = 0; index < count; index++) {
        values[index] = (int32_t)(next_random() % (uint32_t)(2 * magnitude + 1)) - magnitude;
        put_rice(values[index], parameter);
    }
}

static uint16_t reference_crc16(const uint8_t *bytes, uint32_t length)
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

static uint8_t reference_crc8(const uint8_t *bytes, uint32_t length)
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

static bool decoded_match(uint32_t count)
{
    return memcmp(values, decoded, count * sizeof values[0]) == 0;
}

static void rice_values_decode_across_buffer_refills(void)
{
    start_writing();
    write_values(VALUE_COUNT, 9, 4000);
    start_reading();
    CHECK(flac_bits_read_rice(&bits, decoded, VALUE_COUNT, 9));
    CHECK(decoded_match(VALUE_COUNT));
    CHECK((writer.bit_count + 7) / 8 > FLAC_BUFFER_BYTES);
}

static void a_zero_parameter_reads_only_the_unary_part(void)
{
    start_writing();
    write_values(500, 0, 20);
    start_reading();
    CHECK(flac_bits_read_rice(&bits, decoded, 500, 0));
    CHECK(decoded_match(500));
}

static void long_unary_runs_span_several_bytes(void)
{
    start_writing();
    values[0] = 200;
    values[1] = -150;
    values[2] = 3;
    put_rice(values[0], 1);
    put_rice(values[1], 1);
    put_rice(values[2], 1);
    start_reading();
    CHECK(flac_bits_read_rice(&bits, decoded, 3, 1));
    CHECK(decoded_match(3));
}

static void parameters_above_twenty_four_bits_decode(void)
{
    start_writing();
    write_values(300, 27, 100000000);
    start_reading();
    CHECK(flac_bits_read_rice(&bits, decoded, 300, 27));
    CHECK(decoded_match(300));
}

static void reads_carry_on_after_a_rice_run(void)
{
    start_writing();
    put_bits(5, 3);
    write_values(100, 4, 60);
    put_bits(0xABCDE, 20);
    start_reading();
    CHECK_EQUAL(5u, flac_bits_read(&bits, 3));
    CHECK(flac_bits_read_rice(&bits, decoded, 100, 4));
    CHECK(decoded_match(100));
    CHECK_EQUAL(0xABCDEu, flac_bits_read(&bits, 20));
}

static void the_checksums_cover_exactly_the_bytes_read(void)
{
    start_writing();
    put_bits(0xFFF8, 16);
    write_values(VALUE_COUNT, 9, 4000);
    while (writer.bit_count % 8 != 0) {
        put_bit(0);
    }
    uint32_t length = writer.bit_count / 8;
    put_bits(0x1234, 16);
    start_reading();
    flac_bits_reset_crc(&bits);
    CHECK_EQUAL(0xFFF8u, flac_bits_read(&bits, 16));
    CHECK_EQUAL(reference_crc8(writer.bytes, 2), flac_bits_crc8(&bits));
    CHECK(flac_bits_read_rice(&bits, decoded, VALUE_COUNT, 9));
    flac_bits_align(&bits);
    CHECK_EQUAL(reference_crc16(writer.bytes, length), flac_bits_crc16(&bits));
    CHECK_EQUAL(0x1234u, flac_bits_read(&bits, 16));
}

static void a_reset_starts_the_checksums_at_the_current_byte(void)
{
    start_writing();
    put_bits(0x77, 8);
    put_bits(0x1234, 16);
    start_reading();
    CHECK_EQUAL(0x77u, flac_bits_read(&bits, 8));
    flac_bits_reset_crc(&bits);
    CHECK_EQUAL(0x1234u, flac_bits_read(&bits, 16));
    CHECK_EQUAL(reference_crc16(writer.bytes + 1, 2), flac_bits_crc16(&bits));
}

static void running_out_of_input_fails(void)
{
    start_writing();
    write_values(10, 3, 30);
    start_reading();
    CHECK(!flac_bits_read_rice(&bits, decoded, 400, 3));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(rice_values_decode_across_buffer_refills),
        TEST_CASE(a_zero_parameter_reads_only_the_unary_part),
        TEST_CASE(long_unary_runs_span_several_bytes),
        TEST_CASE(parameters_above_twenty_four_bits_decode),
        TEST_CASE(reads_carry_on_after_a_rice_run),
        TEST_CASE(the_checksums_cover_exactly_the_bytes_read),
        TEST_CASE(a_reset_starts_the_checksums_at_the_current_byte),
        TEST_CASE(running_out_of_input_fails),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
