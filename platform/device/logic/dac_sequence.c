#include "device/dac_sequence.h"

#include <stddef.h>

enum {
    MICROSECONDS_PER_MILLISECOND = 1000,
    CHIP_COUNT = 2,
    STATUS_MAP = 0x0F0000,
    STATUS_NOT_READY = 0x03,
    POWER_CONTROL_MAP = 0x020000,
    SERIAL_PORT_BIT = 0x40,
    HEADPHONE_BIT = 0x10,
    RELEASE_WAIT_US = 100,
    MONO_GROUPS = 4,
    MONO_ITEMS = 6
};

typedef struct Write {
    uint32_t map;
    uint8_t value;
} Write;

#define PCM_VOLUME_B 0x090001u
#define PCM_VOLUME_A 0x090002u
#define HEADPHONE_GAIN 0x080000u

enum {
    TOP_VOLUME_VALUE = 0x7A,
    VOLUME_STEP_NUMERATOR = 12,
    VOLUME_STEP_DENOMINATOR = 5,
    GAIN_HIGH_VALUE = 0x31,
    GAIN_LOW_VALUE = 0x10
};

static const uint8_t CHIPS[CHIP_COUNT] = {DAC_LEFT_ADDRESS, DAC_RIGHT_ADDRESS};
static const uint8_t CHANNEL_VALUES[CHIP_COUNT] = {0x07, 0x0F};

static const Write AFTER_STATUS[] = {
    {0x0F0010, 0xE7}, {0x020000, 0xF6}, {0x01000C, 0x04}, {0x040010, 0x01}, {0x040011, 0x00},
    {0x040012, 0x08}, {0x040013, 0x00}, {0x040014, 0x1F}, {0x040015, 0x00}, {0x040016, 0x3F},
    {0x040017, 0x00}, {0x040018, 0x0C}, {0x040019, 0x0A}, {0x050000, 0x00}, {0x050001, 0x00},
};

static const Write BEFORE_MONO[] = {
    {0x090000, 0x02},
    {PCM_VOLUME_B, DAC_VOLUME_MUTED},
    {PCM_VOLUME_A, DAC_VOLUME_MUTED},
    {0x090003, 0xEC},
    {0x090004, 0x00},
};

static const uint8_t MONO_OFFSETS[MONO_ITEMS] = {0x09, 0x0A, 0x0B, 0x0C, 0x0F, 0x10};
static const uint8_t MONO_VALUES[MONO_GROUPS][MONO_ITEMS] = {
    {0x2F, 0xB4, 0x25, 0x80, 0x55, 0xFA},
    {0x2F, 0xB4, 0x25, 0x80, 0x4E, 0xFD},
    {0x2F, 0xB4, 0x25, 0x80, 0x7C, 0xFD},
    {0x2F, 0xB4, 0x25, 0x80, 0x52, 0xFE},
};

static const Write AFTER_MONO[] = {
    {0x080000, 0x10}, {0x0D0000, 0x04}, {0x0D0001, 0x00}, {0x0F0010, 0x87}, {0x0F0011, 0x07},
    {0x010006, 0x04}, {0x01000D, 0x02}, {0x010010, 0x99}, {0x180000, 0x45}, {0x010010, 0x00},
};

static void wait_ms(const DacBus *bus, uint32_t milliseconds)
{
    bus->delay_us(bus->context, milliseconds * MICROSECONDS_PER_MILLISECOND);
}

static void set_pin(const DacBus *bus, DacPin pin, bool level)
{
    bus->set_pin(bus->context, pin, level);
}

static bool write_both(const DacBus *bus, uint32_t map, uint8_t value)
{
    for (int chip = 0; chip < CHIP_COUNT; chip++) {
        if (!bus->write(bus->context, CHIPS[chip], map, value)) {
            return false;
        }
    }
    return true;
}

static bool write_all(const DacBus *bus, const Write *writes, size_t count)
{
    for (size_t index = 0; index < count; index++) {
        if (!write_both(bus, writes[index].map, writes[index].value)) {
            return false;
        }
    }
    return true;
}

static bool write_channel_values(const DacBus *bus)
{
    for (int chip = 0; chip < CHIP_COUNT; chip++) {
        uint8_t value = CHANNEL_VALUES[chip];
        if (!bus->write(bus->context, CHIPS[chip], 0x05000A, value) ||
            !bus->write(bus->context, CHIPS[chip], 0x05000B, value)) {
            return false;
        }
    }
    return true;
}

static bool write_mono_table(const DacBus *bus)
{
    for (uint32_t group = 0; group < MONO_GROUPS; group++) {
        for (int item = 0; item < MONO_ITEMS; item++) {
            uint32_t map = 0x180000u + group * 0x10u + MONO_OFFSETS[item];
            if (!write_both(bus, map, MONO_VALUES[group][item])) {
                return false;
            }
        }
    }
    return true;
}

static bool release_power_bit(const DacBus *bus, uint8_t bit)
{
    for (int chip = 0; chip < CHIP_COUNT; chip++) {
        uint8_t value = 0;
        bus->delay_us(bus->context, RELEASE_WAIT_US);
        if (!bus->read(bus->context, CHIPS[chip], POWER_CONTROL_MAP, &value) ||
            !bus->write(bus->context, CHIPS[chip], POWER_CONTROL_MAP, (uint8_t)(value & ~bit))) {
            return false;
        }
    }
    return true;
}

static DacResult read_status(const DacBus *bus)
{
    int not_ready = 0;
    for (int chip = 0; chip < CHIP_COUNT; chip++) {
        uint8_t status = 0;
        if (!bus->read(bus->context, CHIPS[chip], STATUS_MAP, &status)) {
            return DAC_NO_REPLY;
        }
        not_ready += (status & STATUS_NOT_READY) == STATUS_NOT_READY ? 1 : 0;
    }
    return not_ready == CHIP_COUNT ? DAC_NOT_READY : DAC_READY;
}

void dac_power_pins(const DacBus *bus)
{
    set_pin(bus, DAC_PIN_MUTE, true);
    set_pin(bus, DAC_PIN_ENABLE_A, true);
    set_pin(bus, DAC_PIN_ENABLE_B, true);
    wait_ms(bus, 2);
    set_pin(bus, DAC_PIN_RESET, false);
    wait_ms(bus, 1);
    set_pin(bus, DAC_PIN_RESET, true);
    wait_ms(bus, 8);
    set_pin(bus, DAC_PIN_RESET, false);
    wait_ms(bus, 2);
}

bool dac_probe(const DacBus *bus)
{
    return write_both(bus, 0x000000, 0x00);
}

static bool start_up(const DacBus *bus)
{
    if (!write_both(bus, 0x0B0000, 0x1F)) {
        return false;
    }
    wait_ms(bus, 80);
    set_pin(bus, DAC_PIN_AMP, true);
    return write_both(bus, 0x020052, 0x02);
}

static bool set_up_blocks(const DacBus *bus)
{
    if (!write_all(bus, AFTER_STATUS, sizeof AFTER_STATUS / sizeof AFTER_STATUS[0]) ||
        !write_channel_values(bus) ||
        !write_all(bus, BEFORE_MONO, sizeof BEFORE_MONO / sizeof BEFORE_MONO[0]) ||
        !write_mono_table(bus) || !write_both(bus, 0x0B0000, 0x1F)) {
        return false;
    }
    wait_ms(bus, 80);
    return write_all(bus, AFTER_MONO, sizeof AFTER_MONO / sizeof AFTER_MONO[0]);
}

static bool release_outputs(const DacBus *bus, uint8_t rate_byte)
{
    if (!release_power_bit(bus, SERIAL_PORT_BIT) || !write_both(bus, 0x010010, 0x99) ||
        !write_both(bus, 0x080032, 0x20) || !release_power_bit(bus, HEADPHONE_BIT)) {
        return false;
    }
    wait_ms(bus, 10);
    if (!write_both(bus, 0x080032, 0x00) || !write_both(bus, 0x010010, 0x00) ||
        !write_both(bus, 0x01000B, rate_byte)) {
        return false;
    }
    wait_ms(bus, 10);
    return true;
}

DacResult dac_configure(const DacBus *bus, uint8_t rate_byte)
{
    if (!start_up(bus)) {
        return DAC_NO_REPLY;
    }
    DacResult status = read_status(bus);
    if (status != DAC_READY) {
        return status;
    }
    return set_up_blocks(bus) && release_outputs(bus, rate_byte) ? DAC_READY : DAC_NO_REPLY;
}

void dac_set_volume(const DacBus *bus, uint8_t volume)
{
    write_both(bus, PCM_VOLUME_B, volume);
    write_both(bus, PCM_VOLUME_A, volume);
}

void dac_set_gain(const DacBus *bus, bool high)
{
    write_both(bus, HEADPHONE_GAIN, high ? GAIN_HIGH_VALUE : GAIN_LOW_VALUE);
}

uint8_t dac_volume_value(uint8_t level)
{
    if (level == 0) {
        return DAC_VOLUME_MUTED;
    }
    uint32_t capped = level < DAC_VOLUME_LEVELS ? level : DAC_VOLUME_LEVELS;
    return (uint8_t)(TOP_VOLUME_VALUE - VOLUME_STEP_NUMERATOR * capped / VOLUME_STEP_DENOMINATOR);
}

void dac_output_on(const DacBus *bus, uint8_t volume)
{
    set_pin(bus, DAC_PIN_ENABLE_A, true);
    set_pin(bus, DAC_PIN_MUTE, false);
    set_pin(bus, DAC_PIN_AMP, true);
    write_channel_values(bus);
    dac_set_volume(bus, volume);
    wait_ms(bus, 10);
}

void dac_output_off(const DacBus *bus)
{
    set_pin(bus, DAC_PIN_MUTE, true);
    set_pin(bus, DAC_PIN_ENABLE_A, true);
    dac_set_volume(bus, DAC_VOLUME_MUTED);
    write_both(bus, 0x020000, 0x10);
    wait_ms(bus, 5);
    write_both(bus, 0x0B0000, 0x00);
    wait_ms(bus, 10);
    set_pin(bus, DAC_PIN_AMP, false);
    set_pin(bus, DAC_PIN_RESET, true);
    set_pin(bus, DAC_PIN_ENABLE_B, false);
}

void dac_set_muted(const DacBus *bus, bool muted, uint8_t volume)
{
    if (muted) {
        set_pin(bus, DAC_PIN_MUTE, true);
        dac_set_volume(bus, DAC_VOLUME_MUTED);
        return;
    }
    dac_set_volume(bus, volume);
    set_pin(bus, DAC_PIN_MUTE, false);
}
