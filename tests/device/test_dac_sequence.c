#include <stdbool.h>
#include <stddef.h>

#include "check.h"
#include "device/dac_sequence.h"

enum {
    LEFT = 0x60,
    RIGHT = 0x66,
    OP_CAPACITY = 256
};

typedef enum OpKind {
    OP_WRITE,
    OP_READ,
    OP_PIN,
    OP_DELAY
} OpKind;

typedef struct Op {
    OpKind kind;
    uint8_t address;
    uint32_t map;
    uint32_t value;
} Op;

typedef struct FakeBus {
    Op ops[OP_CAPACITY];
    int count;
    uint8_t status[2];
    uint8_t power_control[2];
    bool silent;
} FakeBus;

static FakeBus fake;

static int chip_of(uint8_t address)
{
    return address == LEFT ? 0 : 1;
}

static void record(Op op)
{
    if (fake.count < OP_CAPACITY) {
        fake.ops[fake.count] = op;
    }
    fake.count++;
}

static bool fake_write(void *context, uint8_t address, uint32_t map, uint8_t value)
{
    (void)context;
    if (fake.silent) {
        return false;
    }
    record((Op){OP_WRITE, address, map, value});
    if (map == 0x020000) {
        fake.power_control[chip_of(address)] = value;
    }
    return true;
}

static bool fake_read(void *context, uint8_t address, uint32_t map, uint8_t *value)
{
    (void)context;
    if (fake.silent) {
        return false;
    }
    record((Op){OP_READ, address, map, 0});
    *value = map == 0x0F0000 ? fake.status[chip_of(address)] : fake.power_control[chip_of(address)];
    return true;
}

static void fake_pin(void *context, DacPin pin, bool level)
{
    (void)context;
    record((Op){OP_PIN, 0, (uint32_t)pin, level ? 1u : 0u});
}

static void fake_delay(void *context, uint32_t microseconds)
{
    (void)context;
    record((Op){OP_DELAY, 0, 0, microseconds});
}

static DacBus fresh_bus(void)
{
    fake = (FakeBus){.count = 0};
    return (DacBus){fake_write, fake_read, fake_pin, fake_delay, NULL};
}

static int cursor;

static bool next_is(Op expected)
{
    if (cursor >= fake.count) {
        return false;
    }
    Op actual = fake.ops[cursor++];
    return actual.kind == expected.kind && actual.address == expected.address &&
           actual.map == expected.map && actual.value == expected.value;
}

static bool next_pin(DacPin pin, bool level)
{
    return next_is((Op){OP_PIN, 0, (uint32_t)pin, level ? 1u : 0u});
}

static bool next_delay_ms(uint32_t milliseconds)
{
    return next_is((Op){OP_DELAY, 0, 0, milliseconds * 1000u});
}

static bool next_both(uint32_t map, uint8_t value)
{
    return next_is((Op){OP_WRITE, LEFT, map, value}) && next_is((Op){OP_WRITE, RIGHT, map, value});
}

static bool next_one(uint8_t address, uint32_t map, uint8_t value)
{
    return next_is((Op){OP_WRITE, address, map, value});
}

static bool next_release(uint8_t address, uint8_t before, uint8_t bit)
{
    return next_is((Op){OP_DELAY, 0, 0, 100}) && next_is((Op){OP_READ, address, 0x020000, 0}) &&
           next_one(address, 0x020000, (uint8_t)(before & ~bit));
}

static void the_power_pins_pulse_reset_as_in_the_facts(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    dac_power_pins(&bus);
    CHECK(next_pin(DAC_PIN_MUTE, true));
    CHECK(next_pin(DAC_PIN_ENABLE_A, true));
    CHECK(next_pin(DAC_PIN_ENABLE_B, true));
    CHECK(next_delay_ms(2));
    CHECK(next_pin(DAC_PIN_RESET, false));
    CHECK(next_delay_ms(1));
    CHECK(next_pin(DAC_PIN_RESET, true));
    CHECK(next_delay_ms(8));
    CHECK(next_pin(DAC_PIN_RESET, false));
    CHECK(next_delay_ms(2));
    CHECK_EQUAL(cursor, fake.count);
}

static void the_probe_writes_map_zero_to_both_chips(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    CHECK(dac_probe(&bus));
    CHECK(next_both(0x000000, 0x00));
    CHECK_EQUAL(cursor, fake.count);
}

static void a_silent_bus_fails_the_probe(void)
{
    DacBus bus = fresh_bus();
    fake.silent = true;
    CHECK(!dac_probe(&bus));
}

static bool next_mono_table(void)
{
    static const uint8_t OFFSETS[6] = {0x09, 0x0A, 0x0B, 0x0C, 0x0F, 0x10};
    static const uint8_t VALUES[4][6] = {
        {0x2F, 0xB4, 0x25, 0x80, 0x55, 0xFA},
        {0x2F, 0xB4, 0x25, 0x80, 0x4E, 0xFD},
        {0x2F, 0xB4, 0x25, 0x80, 0x7C, 0xFD},
        {0x2F, 0xB4, 0x25, 0x80, 0x52, 0xFE},
    };
    for (int group = 0; group < 4; group++) {
        for (int item = 0; item < 6; item++) {
            uint32_t map = 0x180000u + (uint32_t)group * 0x10u + OFFSETS[item];
            if (!next_both(map, VALUES[group][item])) {
                return false;
            }
        }
    }
    return true;
}

static void the_register_sequence_follows_the_facts(void)
{
    static const uint8_t BLOCK_4[10] = {0x01, 0x00, 0x08, 0x00, 0x1F, 0x00, 0x3F, 0x00, 0x0C, 0x0A};
    DacBus bus = fresh_bus();
    cursor = 0;
    CHECK_EQUAL(DAC_READY, dac_configure(&bus, 1));
    CHECK(next_both(0x0B0000, 0x1F));
    CHECK(next_delay_ms(80));
    CHECK(next_pin(DAC_PIN_AMP, true));
    CHECK(next_both(0x020052, 0x02));
    CHECK(next_is((Op){OP_READ, LEFT, 0x0F0000, 0}));
    CHECK(next_is((Op){OP_READ, RIGHT, 0x0F0000, 0}));
    CHECK(next_both(0x0F0010, 0xE7));
    CHECK(next_both(0x020000, 0xF6));
    CHECK(next_both(0x01000C, 0x04));
    for (uint32_t index = 0; index < 10; index++) {
        CHECK(next_both(0x040010 + index, BLOCK_4[index]));
    }
    CHECK(next_both(0x050000, 0x00));
    CHECK(next_both(0x050001, 0x00));
    CHECK(next_one(LEFT, 0x05000A, 0x07) && next_one(LEFT, 0x05000B, 0x07));
    CHECK(next_one(RIGHT, 0x05000A, 0x0F) && next_one(RIGHT, 0x05000B, 0x0F));
    CHECK(next_both(0x090000, 0x02));
    CHECK(next_both(0x090001, 0xFF));
    CHECK(next_both(0x090002, 0xFF));
    CHECK(next_both(0x090003, 0xEC));
    CHECK(next_both(0x090004, 0x00));
    CHECK(next_mono_table());
    CHECK(next_both(0x0B0000, 0x1F));
    CHECK(next_delay_ms(80));
    CHECK(next_both(0x080000, 0x10));
    CHECK(next_both(0x0D0000, 0x04));
    CHECK(next_both(0x0D0001, 0x00));
    CHECK(next_both(0x0F0010, 0x87));
    CHECK(next_both(0x0F0011, 0x07));
    CHECK(next_both(0x010006, 0x04));
    CHECK(next_both(0x01000D, 0x02));
    CHECK(next_both(0x010010, 0x99));
    CHECK(next_both(0x180000, 0x45));
    CHECK(next_both(0x010010, 0x00));
    CHECK(next_release(LEFT, 0xF6, 0x40));
    CHECK(next_release(RIGHT, 0xF6, 0x40));
    CHECK(next_both(0x010010, 0x99));
    CHECK(next_both(0x080032, 0x20));
    CHECK(next_release(LEFT, 0xB6, 0x10));
    CHECK(next_release(RIGHT, 0xB6, 0x10));
    CHECK(next_delay_ms(10));
    CHECK(next_both(0x080032, 0x00));
    CHECK(next_both(0x010010, 0x00));
    CHECK(next_both(0x01000B, 0x01));
    CHECK(next_delay_ms(10));
    CHECK_EQUAL(cursor, fake.count);
    CHECK_EQUAL(0xA6, fake.power_control[0]);
    CHECK_EQUAL(0xA6, fake.power_control[1]);
}

static void both_chips_reporting_not_ready_stops_the_sequence(void)
{
    DacBus bus = fresh_bus();
    fake.status[0] = 0x03;
    fake.status[1] = 0x03;
    CHECK_EQUAL(DAC_NOT_READY, dac_configure(&bus, 1));
    CHECK_EQUAL(OP_READ, fake.ops[fake.count - 1].kind);
}

static void one_chip_reporting_not_ready_carries_on(void)
{
    DacBus bus = fresh_bus();
    fake.status[0] = 0x03;
    CHECK_EQUAL(DAC_READY, dac_configure(&bus, 1));
}

static void a_silent_bus_reports_no_reply(void)
{
    DacBus bus = fresh_bus();
    fake.silent = true;
    CHECK_EQUAL(DAC_NO_REPLY, dac_configure(&bus, 1));
}

static void output_on_unmutes_and_restores_the_channel_values(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    dac_output_on(&bus, 0x5E);
    CHECK(next_pin(DAC_PIN_ENABLE_A, true));
    CHECK(next_pin(DAC_PIN_MUTE, false));
    CHECK(next_pin(DAC_PIN_AMP, true));
    CHECK(next_one(LEFT, 0x05000A, 0x07) && next_one(LEFT, 0x05000B, 0x07));
    CHECK(next_one(RIGHT, 0x05000A, 0x0F) && next_one(RIGHT, 0x05000B, 0x0F));
    CHECK(next_both(0x090001, 0x5E));
    CHECK(next_both(0x090002, 0x5E));
    CHECK(next_delay_ms(10));
    CHECK_EQUAL(cursor, fake.count);
}

static void output_off_mutes_and_powers_down(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    dac_output_off(&bus);
    CHECK(next_pin(DAC_PIN_MUTE, true));
    CHECK(next_pin(DAC_PIN_ENABLE_A, true));
    CHECK(next_both(0x090001, 0xFF));
    CHECK(next_both(0x090002, 0xFF));
    CHECK(next_both(0x020000, 0x10));
    CHECK(next_delay_ms(5));
    CHECK(next_both(0x0B0000, 0x00));
    CHECK(next_delay_ms(10));
    CHECK(next_pin(DAC_PIN_AMP, false));
    CHECK(next_pin(DAC_PIN_RESET, true));
    CHECK(next_pin(DAC_PIN_ENABLE_B, false));
    CHECK_EQUAL(cursor, fake.count);
}

static void muting_holds_the_pin_and_mutes_both_chips(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    dac_set_muted(&bus, true, 0x5E);
    CHECK(next_pin(DAC_PIN_MUTE, true));
    CHECK(next_both(0x090001, 0xFF));
    CHECK(next_both(0x090002, 0xFF));
    dac_set_muted(&bus, false, 0x5E);
    CHECK(next_both(0x090001, 0x5E));
    CHECK(next_both(0x090002, 0x5E));
    CHECK(next_pin(DAC_PIN_MUTE, false));
    CHECK_EQUAL(cursor, fake.count);
}

static void setting_the_volume_writes_both_pcm_volume_registers(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    dac_set_volume(&bus, 0x30);
    CHECK(next_both(0x090001, 0x30));
    CHECK(next_both(0x090002, 0x30));
    CHECK_EQUAL(cursor, fake.count);
}

static void the_gain_setting_writes_the_headphone_gain_register(void)
{
    DacBus bus = fresh_bus();
    cursor = 0;
    dac_set_gain(&bus, true);
    CHECK(next_both(0x080000, 0x31));
    dac_set_gain(&bus, false);
    CHECK(next_both(0x080000, 0x10));
    CHECK_EQUAL(cursor, fake.count);
}

static void volume_levels_map_onto_the_pcm_volume_scale(void)
{
    CHECK_EQUAL(0xFF, dac_volume_value(0));
    CHECK_EQUAL(0x78, dac_volume_value(1));
    CHECK_EQUAL(0x5E, dac_volume_value(12));
    CHECK_EQUAL(0x3E, dac_volume_value(25));
    CHECK_EQUAL(0x02, dac_volume_value(50));
    CHECK_EQUAL(0x02, dac_volume_value(80));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_power_pins_pulse_reset_as_in_the_facts),
        TEST_CASE(the_probe_writes_map_zero_to_both_chips),
        TEST_CASE(a_silent_bus_fails_the_probe),
        TEST_CASE(the_register_sequence_follows_the_facts),
        TEST_CASE(both_chips_reporting_not_ready_stops_the_sequence),
        TEST_CASE(one_chip_reporting_not_ready_carries_on),
        TEST_CASE(a_silent_bus_reports_no_reply),
        TEST_CASE(output_on_unmutes_and_restores_the_channel_values),
        TEST_CASE(output_off_mutes_and_powers_down),
        TEST_CASE(muting_holds_the_pin_and_mutes_both_chips),
        TEST_CASE(setting_the_volume_writes_both_pcm_volume_registers),
        TEST_CASE(the_gain_setting_writes_the_headphone_gain_register),
        TEST_CASE(volume_levels_map_onto_the_pcm_volume_scale),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
