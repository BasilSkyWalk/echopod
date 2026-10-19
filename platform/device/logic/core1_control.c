#include "device/core1_control.h"

#include <stdbool.h>

#define PMU_POWER 0x5002000Cu
#define PMU_STATUS 0x50020010u
#define PMU_CORE1_DOMAIN 0x2u
#define CRU_GATES_00 0x40180080u
#define CRU_GATES_30 0x4018008Cu
#define CRU_GATES_40 0x40180090u
#define CRU_GATES_90 0x401800A4u
#define CRU_RESETS_10 0x401800C4u
#define CRU_RESETS_20 0x401800C8u
#define CRU_RESETS_30 0x401800CCu
#define CRU_CLOCK_SELECT 0x40180014u
#define GRF_SPEED 0x500100C4u
#define MAILBOX_A2B_STATUS 0x40110004u
#define MAILBOX_B2A_ENABLE 0x40110028u
#define MAILBOX_B2A_STATUS 0x4011002Cu
#define MAILBOX_ALL_CHANNELS 0xFu
#define MAILBOX_CHANNEL_0 0x1u
#define CORE1_HOLD 0x00040004u
#define CORE1_RUN 0x00040000u
#define CORE1_CLOCK_SELECT 0xF7000000u
#define BUS_SPEED_SLOW 0xC0000000u
#define BUS_SPEED_FAST 0xC000C000u

enum {
    MAX_CORE1_MHZ = 100,
    FAST_BUS_MHZ = 200,
    CORE1_DIVIDER_SHIFT = 12,
    MAX_DIVIDER = 8,
    STEP_DELAY_MS = 5,
    POWER_TIMEOUT_MS = 1000,
    REPLY_TIMEOUT_MS = 200,
    WORD_BYTES = 4,
    BITS_PER_BYTE = 8
};

typedef struct Write {
    uint32_t address;
    uint32_t value;
} Write;

uint32_t core1_divider_for(uint32_t pll_mhz)
{
    uint32_t divider = 1;
    while (divider < MAX_DIVIDER && pll_mhz / divider > MAX_CORE1_MHZ) {
        divider++;
    }
    return divider;
}

static void write_then_wait(const Core1Bus *bus, uint32_t address, uint32_t value)
{
    bus->write(bus->context, address, value);
    bus->delay_ms(bus->context, STEP_DELAY_MS);
}

static void write_all(const Core1Bus *bus, const Write *writes, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        write_then_wait(bus, writes[index].address, writes[index].value);
    }
}

static bool wait_for_domain(const Core1Bus *bus, uint32_t wanted)
{
    uint32_t start = bus->now_ms(bus->context);
    while ((bus->read(bus->context, PMU_STATUS) & PMU_CORE1_DOMAIN) != wanted) {
        if (bus->now_ms(bus->context) - start >= POWER_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

static void set_domain_power(const Core1Bus *bus, bool off)
{
    uint32_t power = bus->read(bus->context, PMU_POWER) & ~PMU_CORE1_DOMAIN;
    bus->write(bus->context, PMU_POWER, off ? power | PMU_CORE1_DOMAIN : power);
}

Core1Result core1_power_up(const Core1Bus *bus, uint32_t pll_mhz, uint32_t divider)
{
    set_domain_power(bus, false);
    if (!wait_for_domain(bus, 0)) {
        return CORE1_NO_POWER;
    }
    bool fast = divider != 0 && pll_mhz / divider >= FAST_BUS_MHZ;
    const Write writes[] = {
        {CRU_GATES_90, 0x00040000u},
        {CRU_GATES_40, 0x170F0000u},
        {CRU_GATES_30, 0x01FF0000u},
        {CRU_RESETS_30, CORE1_HOLD},
        {GRF_SPEED, fast ? BUS_SPEED_FAST : BUS_SPEED_SLOW},
        {CRU_CLOCK_SELECT, CORE1_CLOCK_SELECT | (divider - 1u) << CORE1_DIVIDER_SHIFT},
        {CRU_GATES_00, 0x00040000u},
        {CRU_RESETS_10, 0x40000000u},
        {CRU_RESETS_30, 0x00400000u},
        {CRU_RESETS_20, 0x00070000u},
        {MAILBOX_B2A_STATUS, MAILBOX_ALL_CHANNELS},
        {MAILBOX_B2A_ENABLE, MAILBOX_ALL_CHANNELS},
    };
    write_all(bus, writes, sizeof writes / sizeof writes[0]);
    return CORE1_OK;
}

static uint32_t image_word(const uint8_t *image, uint32_t length, uint32_t offset)
{
    uint32_t word = 0;
    for (uint32_t byte = 0; byte < WORD_BYTES && offset + byte < length; byte++) {
        word |= (uint32_t)image[offset + byte] << (BITS_PER_BYTE * byte);
    }
    return word;
}

static void copy_image(const Core1Bus *bus, const uint8_t *image, uint32_t length)
{
    for (uint32_t offset = 0; offset < length; offset += WORD_BYTES) {
        bus->write(bus->context, CORE1_CODE_BASE + offset, image_word(image, length, offset));
    }
}

static void clear_shared(const Core1Bus *bus)
{
    for (uint32_t offset = 0; offset < CORE1_SHARED_BYTES; offset += WORD_BYTES) {
        bus->write(bus->context, CORE1_SHARED_BASE + offset, 0);
    }
}

static bool wait_for_reply(const Core1Bus *bus, uint32_t timeout_ms)
{
    uint32_t start = bus->now_ms(bus->context);
    while ((bus->read(bus->context, MAILBOX_B2A_STATUS) & MAILBOX_CHANNEL_0) == 0) {
        if (bus->now_ms(bus->context) - start >= timeout_ms) {
            return false;
        }
    }
    bus->write(bus->context, MAILBOX_B2A_STATUS, MAILBOX_CHANNEL_0);
    return true;
}

void core1_hold(const Core1Bus *bus)
{
    bus->write(bus->context, CRU_RESETS_30, CORE1_HOLD);
}

void core1_prepare(const Core1Bus *bus)
{
    clear_shared(bus);
    bus->write(bus->context, MAILBOX_B2A_STATUS, MAILBOX_ALL_CHANNELS);
    bus->write(bus->context, MAILBOX_A2B_STATUS, MAILBOX_ALL_CHANNELS);
}

Core1Result core1_load(const Core1Bus *bus, const uint8_t *image, uint32_t length)
{
    if (length > CORE1_CODE_BYTES) {
        return CORE1_TOO_BIG;
    }
    core1_hold(bus);
    copy_image(bus, image, length);
    core1_prepare(bus);
    return CORE1_OK;
}

Core1Result core1_run(const Core1Bus *bus, uint32_t timeout_ms)
{
    bus->write(bus->context, CRU_RESETS_30, CORE1_RUN);
    if (!wait_for_reply(bus, timeout_ms)) {
        return CORE1_NO_REPLY;
    }
    bool ready = bus->read(bus->context, CORE1_MAILBOX_B2A_COMMAND) == CORE1_READY_COMMAND &&
                 bus->read(bus->context, CORE1_SHARED_READY) == CORE1_READY_MAGIC;
    return ready ? CORE1_OK : CORE1_BAD_REPLY;
}

Core1Result core1_start(const Core1Bus *bus, const uint8_t *image, uint32_t length)
{
    Core1Result loaded = core1_load(bus, image, length);
    return loaded == CORE1_OK ? core1_run(bus, REPLY_TIMEOUT_MS) : loaded;
}

Core1Result core1_stop(const Core1Bus *bus)
{
    const Write writes[] = {
        {CRU_RESETS_30, CORE1_HOLD}, {CRU_RESETS_20, 0x00070007u}, {CRU_RESETS_10, 0x40004000u},
        {CRU_GATES_30, 0x03FF03FFu}, {CRU_GATES_40, 0x1F0F1F0Fu},  {CRU_GATES_00, 0x00040004u},
        {CRU_GATES_90, 0x00040004u}, {GRF_SPEED, BUS_SPEED_SLOW},
    };
    write_all(bus, writes, sizeof writes / sizeof writes[0]);
    set_domain_power(bus, true);
    return wait_for_domain(bus, PMU_CORE1_DOMAIN) ? CORE1_OK : CORE1_NO_STOP;
}
