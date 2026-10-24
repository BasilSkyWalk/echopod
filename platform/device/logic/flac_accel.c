#include "device/flac_accel.h"

#include <stddef.h>

#define CRU_GATES_3 0x4018008Cu
#define CRU_GATES_4 0x40180090u
#define CRU_RESETS_2 0x401800C8u
#define CRU_RESETS_3 0x401800CCu
#define ACCEL_GATE_ON 0x02000000u
#define DMAC_GATE_ON 0x08000000u
#define RESET_55_ASSERT 0x00800080u
#define RESET_55_RELEASE 0x00800000u
#define RESET_35_ASSERT 0x00080008u
#define RESET_35_RELEASE 0x00080000u
#define ACCEL_CONFIG (FLAC_ACCEL_BASE + 0x00u)
#define ACCEL_CONTROL (FLAC_ACCEL_BASE + 0x04u)
#define ACCEL_STATUS (FLAC_ACCEL_BASE + 0x08u)
#define ACCEL_JOB (FLAC_ACCEL_BASE + 0x0Cu)
#define ACCEL_MODE (FLAC_ACCEL_BASE + 0x10u)
#define ACCEL_LPC (FLAC_ACCEL_BASE + 0x2Cu)
#define ACCEL_INPUT (FLAC_ACCEL_BASE + 0x40u)
#define ACCEL_OUTPUT (FLAC_ACCEL_BASE + 0xA0u)
#define CONFIG_VALUE 0x01080108u
#define CONTROL_VALUE 0x00830083u
#define CONTROL_RUN 0x00008000u
#define STATUS_DONE 0x00008000u
#define MODE_VALUE 3u
#define JOB_FLAC 0xA0000000u
#define JOB_START 3u
#define CHANNEL_STRIDE 0x58u
#define CHANNEL_SAR 0x00u
#define CHANNEL_DAR 0x08u
#define CHANNEL_LLP 0x10u
#define CHANNEL_CTL_LOW 0x18u
#define CHANNEL_CTL_HIGH 0x1Cu
#define CHANNEL_CFG_LOW 0x40u
#define CHANNEL_CFG_HIGH 0x44u
#define DMAC_RAW_TFR (FLAC_ACCEL_DMAC + 0x2C0u)
#define DMAC_CLEAR_TFR (FLAC_ACCEL_DMAC + 0x338u)
#define DMAC_CLEAR_BLOCK (FLAC_ACCEL_DMAC + 0x340u)
#define DMAC_CLEAR_SOURCE (FLAC_ACCEL_DMAC + 0x348u)
#define DMAC_CLEAR_DESTINATION (FLAC_ACCEL_DMAC + 0x350u)
#define DMAC_CLEAR_ERROR (FLAC_ACCEL_DMAC + 0x358u)
#define DMAC_CONFIG (FLAC_ACCEL_DMAC + 0x398u)
#define DMAC_ENABLE (FLAC_ACCEL_DMAC + 0x3A0u)
#define INPUT_CTL_LOW 0x02109125u
#define OUTPUT_CTL_LOW 0x00A09425u
#define INPUT_CFG_LOW 0x00000800u
#define INPUT_CFG_HIGH 0x00000807u
#define OUTPUT_CFG_LOW 0x00000400u
#define OUTPUT_CFG_HIGH 0x00000007u
#define BOTH_CHANNELS 3u
#define INPUT_ON 0x202u
#define OUTPUT_ON 0x101u
#define BOTH_OFF 0x300u
#define CORE1_BUS_OFFSET 0x01000000u

enum {
    INPUT_CHANNEL = 1,
    OUTPUT_CHANNEL = 0,
    JOB_LENGTH_SHIFT = 15,
    ORDER_SHIFT = 16,
    SHIFT_SHIFT = 8,
    SETTLE_READS = 4000,
    POLL_LIMIT = 400000,
    WIDE_LIMIT = 32,
    SMALLEST_HARDWARE_ORDER = 3,
    TIMING_VECTOR = 9,
    VECTOR_COUNT = 17,
    EXPECTED_OFFSET = FLAC_ACCEL_TEST_BLOCK
};

typedef struct TestVector {
    uint16_t samples;
    uint8_t bits;
    uint8_t precision;
    uint8_t order;
    uint8_t shift;
} TestVector;

static const TestVector VECTORS[VECTOR_COUNT] = {
    {4096, 16, 12, 8, 11},  {4608, 16, 12, 12, 12}, {1152, 16, 13, 3, 12},  {192, 16, 12, 16, 9},
    {4608, 16, 14, 4, 0},   {4608, 12, 15, 32, 14}, {576, 20, 8, 8, 7},     {1001, 16, 12, 8, 11},
    {4096, 24, 15, 32, 15}, {4608, 24, 15, 12, 14}, {192, 24, 15, 8, 13},   {1152, 24, 14, 3, 13},
    {4096, 16, 15, 32, 15}, {4608, 24, 15, 32, 15}, {2304, 24, 12, 16, 11}, {3001, 24, 15, 24, 15},
    {4093, 24, 15, 32, 14},
};

static void write(const AccelBus *bus, uint32_t address, uint32_t value)
{
    bus->write(bus->context, address, value);
}

static uint32_t read(const AccelBus *bus, uint32_t address)
{
    return bus->read(bus->context, address);
}

static void settle(const AccelBus *bus)
{
    for (int index = 0; index < SETTLE_READS; index++) {
        (void)read(bus, ACCEL_STATUS);
    }
}

static void reset_and_init(const AccelBus *bus)
{
    write(bus, CRU_RESETS_3, RESET_55_ASSERT);
    write(bus, CRU_RESETS_2, RESET_35_ASSERT);
    settle(bus);
    write(bus, CRU_RESETS_3, RESET_55_RELEASE);
    write(bus, CRU_RESETS_2, RESET_35_RELEASE);
    write(bus, ACCEL_JOB, JOB_FLAC);
    write(bus, ACCEL_MODE, MODE_VALUE);
    write(bus, ACCEL_CONFIG, CONFIG_VALUE);
    write(bus, ACCEL_CONTROL, CONTROL_VALUE);
}

void flac_accel_start(const AccelBus *bus)
{
    write(bus, CRU_GATES_3, ACCEL_GATE_ON);
    write(bus, CRU_GATES_4, DMAC_GATE_ON);
    reset_and_init(bus);
}

static uint32_t channel(int number, uint32_t offset)
{
    return FLAC_ACCEL_DMAC + CHANNEL_STRIDE * (uint32_t)number + offset;
}

static void clear_channels(const AccelBus *bus)
{
    write(bus, DMAC_CLEAR_TFR, BOTH_CHANNELS);
    write(bus, DMAC_CLEAR_BLOCK, BOTH_CHANNELS);
    write(bus, DMAC_CLEAR_SOURCE, BOTH_CHANNELS);
    write(bus, DMAC_CLEAR_DESTINATION, BOTH_CHANNELS);
    write(bus, DMAC_CLEAR_ERROR, BOTH_CHANNELS);
}

static void start_dma(const AccelBus *bus, uint32_t address, uint32_t length)
{
    clear_channels(bus);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_SAR), address);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_DAR), ACCEL_INPUT);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_LLP), 0);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_CTL_LOW), INPUT_CTL_LOW);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_CTL_HIGH), length);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_CFG_LOW), INPUT_CFG_LOW);
    write(bus, channel(INPUT_CHANNEL, CHANNEL_CFG_HIGH), INPUT_CFG_HIGH);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_SAR), ACCEL_OUTPUT);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_DAR), address);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_LLP), 0);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_CTL_LOW), OUTPUT_CTL_LOW);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_CTL_HIGH), length);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_CFG_LOW), OUTPUT_CFG_LOW);
    write(bus, channel(OUTPUT_CHANNEL, CHANNEL_CFG_HIGH), OUTPUT_CFG_HIGH);
    write(bus, DMAC_CONFIG, 1);
    write(bus, DMAC_ENABLE, INPUT_ON);
    write(bus, DMAC_ENABLE, OUTPUT_ON);
}

static bool wait_for(const AccelBus *bus, uint32_t address, uint32_t bit)
{
    for (int poll = 0; poll < POLL_LIMIT; poll++) {
        if ((read(bus, address) & bit) != 0) {
            return true;
        }
    }
    return false;
}

static bool run_job(const AccelBus *bus, uint32_t address, uint32_t length,
                    const int32_t *coefficients, int order, int shift)
{
    uint32_t job = JOB_FLAC + (length << JOB_LENGTH_SHIFT);
    write(bus, ACCEL_JOB, job);
    write(bus, ACCEL_CONTROL, read(bus, ACCEL_CONTROL) | CONTROL_RUN);
    write(bus, ACCEL_LPC, (uint32_t)order << ORDER_SHIFT | (uint32_t)shift << SHIFT_SHIFT);
    for (int word = 0; word < order; word++) {
        write(bus, FLAC_ACCEL_COEFFICIENTS + 4u * (uint32_t)word,
              (uint32_t)coefficients[order - 1 - word]);
    }
    write(bus, ACCEL_JOB, job + JOB_START);
    start_dma(bus, address, length);
    bool done = wait_for(bus, DMAC_RAW_TFR, 1u) && wait_for(bus, ACCEL_STATUS, STATUS_DONE);
    write(bus, ACCEL_CONTROL, read(bus, ACCEL_CONTROL) & ~CONTROL_RUN);
    write(bus, DMAC_CLEAR_TFR, BOTH_CHANNELS);
    if (!done) {
        write(bus, DMAC_ENABLE, BOTH_OFF);
        reset_and_init(bus);
    }
    return done;
}

bool flac_accel_restore(const AccelBus *bus, uint32_t address, uint32_t count,
                        const int32_t *coefficients, int order, int shift)
{
    uint32_t bus_address = address < CORE1_BUS_OFFSET ? address + CORE1_BUS_OFFSET : address;
    uint32_t restored = 0;
    while (restored < count) {
        uint32_t first = restored == 0 ? 0 : restored - (uint32_t)order;
        uint32_t length = count - first < FLAC_ACCEL_MAX_JOB ? count - first : FLAC_ACCEL_MAX_JOB;
        if (!run_job(bus, bus_address + 4u * first, length, coefficients, order, shift)) {
            return false;
        }
        restored = first + length;
    }
    return true;
}

static int64_t prediction(const int32_t *samples, uint32_t index, const int32_t *coefficients,
                          int order)
{
    int64_t sum = 0;
    for (int tap = 0; tap < order; tap++) {
        sum += (int64_t)coefficients[tap] * samples[index - 1 - (uint32_t)tap];
    }
    return sum;
}

void flac_lpc_restore(int32_t *samples, uint32_t count, const int32_t *coefficients, int order,
                      int shift)
{
    for (uint32_t index = (uint32_t)order; index < count; index++) {
        samples[index] += (int32_t)(prediction(samples, index, coefficients, order) >> shift);
    }
}

static int ceiling_log2(int value)
{
    int bits = 0;
    while ((1 << bits) < value) {
        bits++;
    }
    return bits;
}

bool flac_lpc_is_wide(int bits, int precision, int order)
{
    return bits + precision + ceiling_log2(order) > WIDE_LIMIT;
}

static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static int32_t random_signed(uint32_t *state, int bits)
{
    uint32_t span = 1u << (bits - 1);
    return (int32_t)(next_random(state) % (2u * span)) - (int32_t)span;
}

static void prepare(const TestVector *vector, uint32_t seed, int32_t *buffer, int32_t *expected,
                    int32_t *coefficients)
{
    uint32_t state = seed;
    for (int tap = 0; tap < vector->order; tap++) {
        coefficients[tap] = random_signed(&state, vector->precision);
    }
    for (uint32_t index = 0; index < vector->samples; index++) {
        expected[index] = random_signed(&state, vector->bits);
    }
    for (uint32_t index = 0; index < vector->samples; index++) {
        buffer[index] = expected[index];
        if (index >= vector->order) {
            int64_t sum = prediction(expected, index, coefficients, vector->order);
            buffer[index] = expected[index] - (int32_t)(sum >> vector->shift);
        }
    }
}

static uint32_t first_difference(const int32_t *left, const int32_t *right, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        if (left[index] != right[index]) {
            return index;
        }
    }
    return count;
}

AccelBlock flac_accel_timing_block(int32_t *scratch)
{
    const TestVector *vector = &VECTORS[TIMING_VECTOR];
    AccelBlock block = {.samples = vector->samples, .order = vector->order, .shift = vector->shift};
    prepare(vector, 0x9E3779B9u + TIMING_VECTOR, scratch, scratch + EXPECTED_OFFSET,
            block.coefficients);
    return block;
}

AccelTest flac_accel_self_test(const AccelBus *bus, int32_t *scratch, uint32_t scratch_address)
{
    AccelTest test = {.narrow_ok = true, .wide_ok = true};
    int32_t coefficients[FLAC_ACCEL_MAX_ORDER];
    int32_t *expected = scratch + EXPECTED_OFFSET;
    flac_accel_start(bus);
    for (int index = 0; index < VECTOR_COUNT; index++) {
        const TestVector *vector = &VECTORS[index];
        bool wide = flac_lpc_is_wide(vector->bits, vector->precision, vector->order);
        if (wide ? !test.wide_ok : !test.narrow_ok) {
            continue;
        }
        prepare(vector, 0x9E3779B9u + (uint32_t)index, scratch, expected, coefficients);
        if (!flac_accel_restore(bus, scratch_address, vector->samples, coefficients, vector->order,
                                vector->shift)) {
            return (AccelTest){.timed_out = true, .failed_vector = (uint8_t)(index + 1)};
        }
        uint32_t difference = first_difference(scratch, expected, vector->samples);
        if (difference == vector->samples) {
            continue;
        }
        if (wide) {
            test.wide_ok = false;
        } else {
            test.narrow_ok = false;
        }
        if (test.failed_vector == 0) {
            test.failed_vector = (uint8_t)(index + 1);
            test.failed_sample = difference;
        }
    }
    return test;
}

static bool restore_with_accelerator(void *context, int32_t *samples, FlacLpcJob job)
{
    AccelHook *hook = context;
    if (job.order < SMALLEST_HARDWARE_ORDER) {
        return false;
    }
    bool wide = flac_lpc_is_wide((int)job.width, (int)job.precision, (int)job.order);
    if (wide ? !hook->wide : !hook->narrow) {
        return false;
    }
    uint32_t address = hook->window == NULL
                           ? (uint32_t)(uintptr_t)samples
                           : hook->window_address + 4u * (uint32_t)(samples - hook->window);
    if (!flac_accel_restore(hook->bus, address, job.block_size, job.coefficients, (int)job.order,
                            (int)job.shift)) {
        hook->narrow = false;
        hook->wide = false;
        hook->timeouts++;
        return false;
    }
    hook->restored++;
    return true;
}

FlacLpcHook flac_accel_hook(AccelHook *hook)
{
    return (FlacLpcHook){restore_with_accelerator, hook};
}
