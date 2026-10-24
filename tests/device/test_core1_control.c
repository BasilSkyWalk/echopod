#include <stdbool.h>
#include <stddef.h>

#include "check.h"
#include "device/core1_control.h"

enum {
    OP_CAPACITY = 4096,
    MEMORY_WORDS = 64
};

#define PMU_POWER 0x5002000Cu
#define PMU_STATUS 0x50020010u
#define CRU_CLOCK_SELECT 0x40180014u
#define GRF_SPEED 0x500100C4u
#define MAILBOX_A2B_STATUS 0x40110004u
#define MAILBOX_B2A_ENABLE 0x40110028u
#define MAILBOX_B2A_STATUS 0x4011002Cu
#define CORE1_RUN_RESET 0x401800CCu

typedef struct Op {
    bool write;
    uint32_t address;
    uint32_t value;
} Op;

typedef struct FakeBus {
    Op ops[OP_CAPACITY];
    int count;
    uint32_t now_ms;
    uint32_t pmu_power;
    bool pmu_stuck;
    bool core1_running;
    bool core1_replies;
    uint32_t reply_command;
    uint32_t ready_word;
    uint32_t code[MEMORY_WORDS];
    uint32_t shared[MEMORY_WORDS];
} FakeBus;

static FakeBus fake;

static void record(bool write, uint32_t address, uint32_t value)
{
    if (fake.count < OP_CAPACITY) {
        fake.ops[fake.count++] = (Op){write, address, value};
    }
}

static uint32_t fake_read(void *context, uint32_t address)
{
    (void)context;
    record(false, address, 0);
    fake.now_ms++;
    if (address == PMU_POWER) {
        return fake.pmu_power;
    }
    if (address == PMU_STATUS) {
        return fake.pmu_stuck ? (fake.pmu_power ^ 2u) : fake.pmu_power;
    }
    if (address == MAILBOX_B2A_STATUS) {
        return fake.core1_running && fake.core1_replies ? 1u : 0u;
    }
    if (address == CORE1_MAILBOX_B2A_COMMAND) {
        return fake.reply_command;
    }
    if (address == CORE1_SHARED_READY) {
        return fake.ready_word;
    }
    return 0;
}

static void fake_write(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    record(true, address, value);
    if (address == PMU_POWER) {
        fake.pmu_power = value;
    }
    if (address == CORE1_RUN_RESET) {
        fake.core1_running = value == 0x00040000u;
    }
    if (address >= CORE1_CODE_BASE && address < CORE1_CODE_BASE + 4u * MEMORY_WORDS) {
        fake.code[(address - CORE1_CODE_BASE) / 4u] = value;
    }
    if (address >= CORE1_SHARED_BASE && address < CORE1_SHARED_BASE + 4u * MEMORY_WORDS) {
        fake.shared[(address - CORE1_SHARED_BASE) / 4u] = value;
    }
}

static void fake_delay_ms(void *context, uint32_t milliseconds)
{
    (void)context;
    fake.now_ms += milliseconds;
}

static uint32_t fake_now_ms(void *context)
{
    (void)context;
    return fake.now_ms;
}

static const Core1Bus BUS = {fake_read, fake_write, fake_delay_ms, fake_now_ms, NULL};

static void reset_fake(void)
{
    fake = (FakeBus){.pmu_power = 2u,
                     .core1_replies = true,
                     .reply_command = CORE1_READY_COMMAND,
                     .ready_word = CORE1_READY_MAGIC};
}

static int index_of_write(uint32_t address, uint32_t value, int from)
{
    for (int index = from; index < fake.count; index++) {
        if (fake.ops[index].write && fake.ops[index].address == address &&
            fake.ops[index].value == value) {
            return index;
        }
    }
    return -1;
}

static bool writes_in_order(const Op *expected, int count)
{
    int from = 0;
    for (int index = 0; index < count; index++) {
        int found = index_of_write(expected[index].address, expected[index].value, from);
        if (found < 0) {
            return false;
        }
        from = found + 1;
    }
    return true;
}

static void the_divider_keeps_core_1_at_or_under_100_mhz(void)
{
    CHECK_EQUAL(1u, core1_divider_for(96));
    CHECK_EQUAL(1u, core1_divider_for(100));
    CHECK_EQUAL(2u, core1_divider_for(200));
    CHECK_EQUAL(4u, core1_divider_for(400));
}

static void power_up_follows_the_recorded_sequence(void)
{
    reset_fake();
    CHECK_EQUAL(CORE1_OK, core1_power_up(&BUS, 200, 2));
    CHECK_EQUAL(0u, fake.pmu_power & 2u);
    static const Op expected[] = {
        {true, PMU_POWER, 0},
        {true, 0x401800A4u, 0x00040000u},
        {true, 0x40180090u, 0x170F0000u},
        {true, 0x4018008Cu, 0x01FF0000u},
        {true, CORE1_RUN_RESET, 0x00040004u},
        {true, GRF_SPEED, 0xC0000000u},
        {true, CRU_CLOCK_SELECT, 0xF7000000u | 1u << 12},
        {true, 0x40180080u, 0x00040000u},
        {true, 0x401800C4u, 0x40000000u},
        {true, CORE1_RUN_RESET, 0x00400000u},
        {true, 0x401800C8u, 0x00070000u},
        {true, MAILBOX_B2A_STATUS, 0xFu},
        {true, MAILBOX_B2A_ENABLE, 0xFu},
    };
    CHECK(writes_in_order(expected, (int)(sizeof expected / sizeof expected[0])));
}

static void power_up_uses_the_fast_bus_bits_at_200_mhz_and_above(void)
{
    reset_fake();
    CHECK_EQUAL(CORE1_OK, core1_power_up(&BUS, 400, 2));
    CHECK(index_of_write(GRF_SPEED, 0xC000C000u, 0) >= 0);
    CHECK(index_of_write(CRU_CLOCK_SELECT, 0xF7000000u | 1u << 12, 0) >= 0);
}

static void power_up_gives_up_when_the_domain_stays_off(void)
{
    reset_fake();
    fake.pmu_stuck = true;
    CHECK_EQUAL(CORE1_NO_POWER, core1_power_up(&BUS, 96, 1));
    CHECK(index_of_write(CORE1_RUN_RESET, 0x00040004u, 0) < 0);
}

static const uint8_t IMAGE[] = {0x00, 0x40, 0x02, 0x01, 0x41, 0x00, 0x00, 0x00, 0xAA, 0xBB};

static void start_copies_the_image_while_core_1_is_held(void)
{
    reset_fake();
    CHECK_EQUAL(CORE1_OK, core1_start(&BUS, IMAGE, sizeof IMAGE));
    CHECK_EQUAL(0x01024000u, fake.code[0]);
    CHECK_EQUAL(0x00000041u, fake.code[1]);
    CHECK_EQUAL(0x0000BBAAu, fake.code[2]);
    int hold = index_of_write(CORE1_RUN_RESET, 0x00040004u, 0);
    int copy = index_of_write(CORE1_CODE_BASE, 0x01024000u, 0);
    int clear = index_of_write(CORE1_SHARED_BASE, 0, 0);
    int status_b2a = index_of_write(MAILBOX_B2A_STATUS, 0xFu, 0);
    int status_a2b = index_of_write(MAILBOX_A2B_STATUS, 0xFu, 0);
    int release = index_of_write(CORE1_RUN_RESET, 0x00040000u, 0);
    CHECK(hold >= 0 && hold < copy && copy < clear && clear < status_b2a);
    CHECK(status_b2a < status_a2b && status_a2b < release);
    CHECK(index_of_write(CORE1_SHARED_BASE + CORE1_SHARED_BYTES - 4u, 0, 0) > 0);
    CHECK(index_of_write(MAILBOX_B2A_STATUS, 1u, release) > release);
}

static void start_reports_no_reply_after_200_ms(void)
{
    reset_fake();
    fake.core1_replies = false;
    CHECK_EQUAL(CORE1_NO_REPLY, core1_start(&BUS, IMAGE, sizeof IMAGE));
    CHECK(fake.now_ms >= 200u);
}

static void start_checks_the_command_and_the_ready_word(void)
{
    reset_fake();
    fake.reply_command = 3;
    CHECK_EQUAL(CORE1_BAD_REPLY, core1_start(&BUS, IMAGE, sizeof IMAGE));
    reset_fake();
    fake.ready_word = 0x12345678u;
    CHECK_EQUAL(CORE1_BAD_REPLY, core1_start(&BUS, IMAGE, sizeof IMAGE));
}

static void load_leaves_core_1_held_until_run(void)
{
    reset_fake();
    CHECK_EQUAL(CORE1_OK, core1_load(&BUS, IMAGE, sizeof IMAGE));
    CHECK(!fake.core1_running);
    CHECK(index_of_write(CORE1_RUN_RESET, 0x00040000u, 0) < 0);
    CHECK_EQUAL(CORE1_OK, core1_run(&BUS, 200));
    CHECK(fake.core1_running);
}

static void a_program_placed_in_core_1_ram_runs_after_hold_and_prepare(void)
{
    reset_fake();
    core1_hold(&BUS);
    int held = index_of_write(CORE1_RUN_RESET, 0x00040004u, 0);
    CHECK(held >= 0);
    core1_prepare(&BUS);
    CHECK(index_of_write(CORE1_SHARED_BASE, 0, held) > held);
    CHECK(!fake.core1_running);
    CHECK_EQUAL(CORE1_OK, core1_run(&BUS, 200));
    CHECK(fake.core1_running);
}

static void run_waits_as_long_as_asked(void)
{
    reset_fake();
    fake.core1_replies = false;
    CHECK_EQUAL(CORE1_OK, core1_load(&BUS, IMAGE, sizeof IMAGE));
    uint32_t before = fake.now_ms;
    CHECK_EQUAL(CORE1_NO_REPLY, core1_run(&BUS, 3000));
    CHECK(fake.now_ms - before >= 3000u);
}

static void an_image_larger_than_core_1_code_ram_is_refused(void)
{
    reset_fake();
    CHECK_EQUAL(CORE1_TOO_BIG, core1_load(&BUS, IMAGE, CORE1_CODE_BYTES + 4u));
    CHECK_EQUAL(0, fake.count);
}

static void stop_holds_core_1_and_powers_the_domain_down(void)
{
    reset_fake();
    fake.pmu_power = 0;
    CHECK_EQUAL(CORE1_OK, core1_stop(&BUS));
    CHECK_EQUAL(2u, fake.pmu_power & 2u);
    static const Op expected[] = {
        {true, CORE1_RUN_RESET, 0x00040004u},
        {true, 0x401800C8u, 0x00070007u},
        {true, 0x401800C4u, 0x40004000u},
        {true, 0x4018008Cu, 0x03FF03FFu},
        {true, 0x40180090u, 0x1F0F1F0Fu},
        {true, 0x40180080u, 0x00040004u},
        {true, 0x401800A4u, 0x00040004u},
        {true, GRF_SPEED, 0xC0000000u},
        {true, PMU_POWER, 2u},
    };
    CHECK(writes_in_order(expected, (int)(sizeof expected / sizeof expected[0])));
}

static void stop_reports_a_domain_that_stays_on(void)
{
    reset_fake();
    fake.pmu_power = 0;
    fake.pmu_stuck = true;
    CHECK_EQUAL(CORE1_NO_STOP, core1_stop(&BUS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_divider_keeps_core_1_at_or_under_100_mhz),
        TEST_CASE(power_up_follows_the_recorded_sequence),
        TEST_CASE(power_up_uses_the_fast_bus_bits_at_200_mhz_and_above),
        TEST_CASE(power_up_gives_up_when_the_domain_stays_off),
        TEST_CASE(start_copies_the_image_while_core_1_is_held),
        TEST_CASE(start_reports_no_reply_after_200_ms),
        TEST_CASE(start_checks_the_command_and_the_ready_word),
        TEST_CASE(load_leaves_core_1_held_until_run),
        TEST_CASE(a_program_placed_in_core_1_ram_runs_after_hold_and_prepare),
        TEST_CASE(run_waits_as_long_as_asked),
        TEST_CASE(an_image_larger_than_core_1_code_ram_is_refused),
        TEST_CASE(stop_holds_core_1_and_powers_the_domain_down),
        TEST_CASE(stop_reports_a_domain_that_stays_on),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
