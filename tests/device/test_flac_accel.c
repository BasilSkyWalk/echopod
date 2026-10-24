#include <string.h>

#include "check.h"
#include "device/flac_accel.h"

enum {
    MEMORY_BASE = 0x01030000,
    MEMORY_WORDS = FLAC_ACCEL_TEST_WORDS,
    MAX_WRITES = 4096,
    CRU_BASE = 0x40180000
};

typedef struct Write {
    uint32_t address;
    uint32_t value;
} Write;

typedef struct FakeAccel {
    int32_t memory[MEMORY_WORDS];
    int32_t coefficients[FLAC_ACCEL_MAX_ORDER];
    uint32_t control;
    uint32_t job;
    uint32_t parameters;
    uint32_t input_address;
    uint32_t output_address;
    uint32_t input_length;
    uint32_t output_length;
    bool done;
    bool narrow_only;
    bool never_finishes;
    int jobs;
    Write writes[MAX_WRITES];
    int count;
} FakeAccel;

static FakeAccel fake;

static int32_t *memory_at(uint32_t address)
{
    return &fake.memory[(address - MEMORY_BASE) / 4u];
}

static void run_job(void)
{
    int order = (int)(fake.parameters >> 16 & 0xFFu);
    int shift = (int)(fake.parameters >> 8 & 0xFFu);
    uint32_t length = fake.job >> 15 & 0x3FFFu;
    int32_t *samples = memory_at(fake.input_address);
    for (uint32_t index = (uint32_t)order; index < length; index++) {
        int64_t sum = 0;
        for (int tap = 0; tap < order; tap++) {
            int64_t term =
                (int64_t)fake.coefficients[order - 1 - tap] * samples[index - 1 - (uint32_t)tap];
            sum += term;
        }
        if (fake.narrow_only) {
            sum = (int32_t)(uint32_t)sum;
        }
        samples[index] += (int32_t)(sum >> shift);
    }
    fake.jobs++;
    fake.done = true;
}

static uint32_t fake_read(void *context, uint32_t address)
{
    (void)context;
    if (address == FLAC_ACCEL_BASE + 0x08u) {
        return fake.done ? 0x8000u : 0u;
    }
    if (address == FLAC_ACCEL_BASE + 0x04u) {
        return fake.control;
    }
    if (address == FLAC_ACCEL_DMAC + 0x2C0u) {
        return fake.done ? 1u : 0u;
    }
    return 0;
}

static void fake_write(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    if (fake.count < MAX_WRITES) {
        fake.writes[fake.count++] = (Write){address, value};
    }
    if (address >= FLAC_ACCEL_COEFFICIENTS &&
        address < FLAC_ACCEL_COEFFICIENTS + 4u * FLAC_ACCEL_MAX_ORDER) {
        fake.coefficients[(address - FLAC_ACCEL_COEFFICIENTS) / 4u] = (int32_t)value;
    } else if (address == FLAC_ACCEL_BASE + 0x04u) {
        fake.control = value;
    } else if (address == FLAC_ACCEL_BASE + 0x0Cu) {
        fake.job = value;
        fake.done = false;
    } else if (address == FLAC_ACCEL_BASE + 0x2Cu) {
        fake.parameters = value;
    } else if (address == FLAC_ACCEL_DMAC + 0x58u) {
        fake.input_address = value;
    } else if (address == FLAC_ACCEL_DMAC + 0x08u) {
        fake.output_address = value;
    } else if (address == FLAC_ACCEL_DMAC + 0x58u + 0x1Cu) {
        fake.input_length = value;
    } else if (address == FLAC_ACCEL_DMAC + 0x1Cu) {
        fake.output_length = value;
    } else if (address == FLAC_ACCEL_DMAC + 0x3A0u && value == 0x101u && !fake.never_finishes) {
        run_job();
    }
}

static const AccelBus BUS = {fake_read, fake_write, NULL};

static void reset_fake(void)
{
    memset(&fake, 0, sizeof fake);
}

static int index_of(uint32_t address, uint32_t value, int from)
{
    for (int index = from; index < fake.count; index++) {
        if (fake.writes[index].address == address && fake.writes[index].value == value) {
            return index;
        }
    }
    return -1;
}

static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static void fill_signal(int32_t *samples, uint32_t count, int bits, uint32_t seed)
{
    uint32_t state = seed;
    uint32_t span = 1u << (bits - 1);
    for (uint32_t index = 0; index < count; index++) {
        samples[index] = (int32_t)(next_random(&state) % (2u * span)) - (int32_t)span;
    }
}

static void starting_turns_on_the_clocks_pulses_the_resets_and_writes_the_init_values(void)
{
    reset_fake();
    flac_accel_start(&BUS);
    int gate = index_of(CRU_BASE + 0x8Cu, 0x02000000u, 0);
    CHECK(gate >= 0);
    CHECK(index_of(CRU_BASE + 0x90u, 0x08000000u, 0) >= 0);
    int asserted = index_of(CRU_BASE + 0xCCu, 0x00800080u, 0);
    CHECK(asserted > gate);
    CHECK(index_of(CRU_BASE + 0xC8u, 0x00080008u, 0) > gate);
    int released = index_of(CRU_BASE + 0xCCu, 0x00800000u, asserted);
    CHECK(released > asserted);
    CHECK(index_of(CRU_BASE + 0xC8u, 0x00080000u, asserted) > asserted);
    CHECK(index_of(FLAC_ACCEL_BASE + 0x0Cu, 0xA0000000u, released) > released);
    CHECK(index_of(FLAC_ACCEL_BASE + 0x10u, 3u, released) > released);
    CHECK(index_of(FLAC_ACCEL_BASE + 0x00u, 0x01080108u, released) > released);
    CHECK(index_of(FLAC_ACCEL_BASE + 0x04u, 0x00830083u, released) > released);
}

static void a_job_follows_the_stock_sequence(void)
{
    reset_fake();
    static const int32_t COEFFICIENTS[3] = {100, -50, 25};
    fill_signal(fake.memory, 1000, 16, 1);
    CHECK(flac_accel_restore(&BUS, MEMORY_BASE, 1000, COEFFICIENTS, 3, 7));
    int job = index_of(FLAC_ACCEL_BASE + 0x0Cu, 0xA0000000u + (1000u << 15), 0);
    CHECK(job >= 0);
    int parameters = index_of(FLAC_ACCEL_BASE + 0x2Cu, 0x00030700u, job);
    CHECK(parameters > job);
    CHECK(index_of(FLAC_ACCEL_COEFFICIENTS, 25u, parameters) > parameters);
    CHECK(index_of(FLAC_ACCEL_COEFFICIENTS + 8u, 100u, parameters) > parameters);
    int start = index_of(FLAC_ACCEL_BASE + 0x0Cu, 0xA0000003u + (1000u << 15), parameters);
    CHECK(start > parameters);
    CHECK(index_of(FLAC_ACCEL_DMAC + 0x58u + 0x18u, 0x02109125u, start) > start);
    CHECK(index_of(FLAC_ACCEL_DMAC + 0x18u, 0x00A09425u, start) > start);
    CHECK(index_of(FLAC_ACCEL_DMAC + 0x58u + 0x08u, FLAC_ACCEL_BASE + 0x40u, start) > start);
    CHECK(index_of(FLAC_ACCEL_DMAC + 0x00u, FLAC_ACCEL_BASE + 0xA0u, start) > start);
    int input_on = index_of(FLAC_ACCEL_DMAC + 0x3A0u, 0x202u, start);
    CHECK(input_on > start);
    CHECK(index_of(FLAC_ACCEL_DMAC + 0x3A0u, 0x101u, input_on) > input_on);
    CHECK_EQUAL(MEMORY_BASE, fake.input_address);
    CHECK_EQUAL(MEMORY_BASE, fake.output_address);
    CHECK_EQUAL(1000u, fake.input_length);
    CHECK_EQUAL(0u, fake.control & 0x8000u);
}

static void the_hardware_result_matches_the_software_restore(void)
{
    reset_fake();
    static const int32_t COEFFICIENTS[8] = {1200, -800, 400, -200, 100, -50, 25, -12};
    static int32_t expected[4608];
    fill_signal(fake.memory, 4608, 16, 7);
    memcpy(expected, fake.memory, sizeof expected);
    flac_lpc_restore(expected, 4608, COEFFICIENTS, 8, 11);
    CHECK(flac_accel_restore(&BUS, MEMORY_BASE, 4608, COEFFICIENTS, 8, 11));
    CHECK(memcmp(expected, fake.memory, sizeof expected) == 0);
}

static void a_block_longer_than_one_dma_job_is_split_with_warm_up_overlap(void)
{
    reset_fake();
    static const int32_t COEFFICIENTS[12] = {9, -8, 7, -6, 5, -4, 3, -2, 1, -1, 2, -3};
    fill_signal(fake.memory, 4608, 24, 3);
    static int32_t expected[4608];
    memcpy(expected, fake.memory, sizeof expected);
    flac_lpc_restore(expected, 4608, COEFFICIENTS, 12, 4);
    CHECK(flac_accel_restore(&BUS, MEMORY_BASE, 4608, COEFFICIENTS, 12, 4));
    CHECK_EQUAL(2, fake.jobs);
    CHECK(memcmp(expected, fake.memory, sizeof expected) == 0);
}

static void a_job_that_never_finishes_times_out_and_resets(void)
{
    reset_fake();
    fake.never_finishes = true;
    static const int32_t COEFFICIENTS[3] = {1, 2, 3};
    CHECK(!flac_accel_restore(&BUS, MEMORY_BASE, 100, COEFFICIENTS, 3, 1));
    int off = index_of(FLAC_ACCEL_DMAC + 0x3A0u, 0x300u, 0);
    CHECK(off >= 0);
    CHECK(index_of(CRU_BASE + 0xCCu, 0x00800080u, off) > off);
    CHECK(index_of(FLAC_ACCEL_BASE + 0x04u, 0x00830083u, off) > off);
}

static void the_software_restore_is_the_flac_formula(void)
{
    int32_t samples[6] = {10, 20, 1, -2, 3, 0};
    static const int32_t COEFFICIENTS[2] = {2, -1};
    flac_lpc_restore(samples, 6, COEFFICIENTS, 2, 0);
    CHECK_EQUAL(10, samples[0]);
    CHECK_EQUAL(20, samples[1]);
    CHECK_EQUAL(31, samples[2]);
    CHECK_EQUAL(40, samples[3]);
    CHECK_EQUAL(52, samples[4]);
    CHECK_EQUAL(64, samples[5]);
}

static void wide_means_the_sum_can_pass_32_bits(void)
{
    CHECK(!flac_lpc_is_wide(16, 12, 8));
    CHECK(flac_lpc_is_wide(24, 15, 12));
    CHECK(!flac_lpc_is_wide(16, 15, 2));
    CHECK(flac_lpc_is_wide(16, 15, 3));
}

static void a_good_accelerator_passes_both_classes(void)
{
    reset_fake();
    static int32_t scratch[MEMORY_WORDS];
    (void)scratch;
    AccelTest test = flac_accel_self_test(&BUS, fake.memory, MEMORY_BASE);
    CHECK(test.narrow_ok);
    CHECK(test.wide_ok);
    CHECK(!test.timed_out);
}

static void a_32_bit_accelerator_fails_only_the_wide_class(void)
{
    reset_fake();
    fake.narrow_only = true;
    AccelTest test = flac_accel_self_test(&BUS, fake.memory, MEMORY_BASE);
    CHECK(test.narrow_ok);
    CHECK(!test.wide_ok);
    CHECK(test.failed_vector > 0);
}

static void a_dead_accelerator_fails_both_and_says_it_timed_out(void)
{
    reset_fake();
    fake.never_finishes = true;
    AccelTest test = flac_accel_self_test(&BUS, fake.memory, MEMORY_BASE);
    CHECK(!test.narrow_ok);
    CHECK(!test.wide_ok);
    CHECK(test.timed_out);
}

static FlacLpcJob job_of(const int32_t *coefficients, uint32_t order, uint32_t width,
                         uint32_t precision)
{
    return (FlacLpcJob){.coefficients = coefficients,
                        .block_size = 600,
                        .order = order,
                        .shift = 9,
                        .width = width,
                        .precision = precision};
}

static void the_hook_takes_only_classes_that_passed(void)
{
    reset_fake();
    static const int32_t COEFFICIENTS[12] = {300, -200, 100, -50, 25, -12, 6, -3, 2, -1, 1, 1};
    AccelHook hook = {
        .bus = &BUS, .window = fake.memory, .window_address = MEMORY_BASE, .narrow = true};
    FlacLpcHook lpc = flac_accel_hook(&hook);
    CHECK(lpc.restore(lpc.context, fake.memory, job_of(COEFFICIENTS, 8, 16, 12)));
    CHECK_EQUAL(1, fake.jobs);
    CHECK(!lpc.restore(lpc.context, fake.memory, job_of(COEFFICIENTS, 12, 24, 15)));
    CHECK(!lpc.restore(lpc.context, fake.memory, job_of(COEFFICIENTS, 2, 16, 12)));
    CHECK_EQUAL(1, fake.jobs);
    CHECK_EQUAL(1u, hook.restored);
}

static void a_timeout_turns_the_hook_off(void)
{
    reset_fake();
    fake.never_finishes = true;
    static const int32_t COEFFICIENTS[4] = {4, -3, 2, -1};
    AccelHook hook = {.bus = &BUS,
                      .window = fake.memory,
                      .window_address = MEMORY_BASE,
                      .narrow = true,
                      .wide = true};
    FlacLpcHook lpc = flac_accel_hook(&hook);
    CHECK(!lpc.restore(lpc.context, fake.memory, job_of(COEFFICIENTS, 4, 16, 12)));
    CHECK(!hook.narrow);
    CHECK(!hook.wide);
    CHECK_EQUAL(1u, hook.timeouts);
}

static void the_timing_block_is_a_full_24_bit_order_12_block(void)
{
    reset_fake();
    AccelBlock block = flac_accel_timing_block(fake.memory);
    CHECK_EQUAL(4608u, block.samples);
    CHECK_EQUAL(12, block.order);
    CHECK_EQUAL(14, block.shift);
    CHECK(flac_accel_restore(&BUS, MEMORY_BASE, block.samples, block.coefficients, block.order,
                             block.shift));
    CHECK(memcmp(fake.memory, fake.memory + FLAC_ACCEL_TEST_BLOCK, 4608 * sizeof(int32_t)) == 0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(starting_turns_on_the_clocks_pulses_the_resets_and_writes_the_init_values),
        TEST_CASE(a_job_follows_the_stock_sequence),
        TEST_CASE(the_hardware_result_matches_the_software_restore),
        TEST_CASE(a_block_longer_than_one_dma_job_is_split_with_warm_up_overlap),
        TEST_CASE(a_job_that_never_finishes_times_out_and_resets),
        TEST_CASE(the_software_restore_is_the_flac_formula),
        TEST_CASE(wide_means_the_sum_can_pass_32_bits),
        TEST_CASE(a_good_accelerator_passes_both_classes),
        TEST_CASE(a_32_bit_accelerator_fails_only_the_wide_class),
        TEST_CASE(a_dead_accelerator_fails_both_and_says_it_timed_out),
        TEST_CASE(the_hook_takes_only_classes_that_passed),
        TEST_CASE(a_timeout_turns_the_hook_off),
        TEST_CASE(the_timing_block_is_a_full_24_bit_order_12_block),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
