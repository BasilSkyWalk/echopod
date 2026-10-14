#include "sd.h"

#include <stddef.h>

#include "board.h"
#include "cru.h"
#include "device/sd_card.h"
#include "gpio.h"
#include "mmc.h"
#include "registers.h"

#define CRU_SD_IDENTIFY 0x009F009Fu
#define CRU_SD_TRANSFER 0x009F0080u
#define GRF_SD_MUX_LOW 0x20u
#define GRF_SD_MUX_LOW_VALUE 0xFC005400u
#define GRF_SD_MUX_HIGH 0x24u
#define GRF_SD_MUX_HIGH_VALUE 0x003F0015u
#define GRF_DETECT_MUX 0x04u
#define GRF_DETECT_MUX_VALUE 0x00C00000u
#define GRF_DETECT_PULL 0x14u
#define GRF_DETECT_PULL_VALUE 0x00080008u
#define CRU_MMC_DIVIDERS 0x30u

enum {
    GATE_SD_BUS = 0x18,
    GATE_SD_CARD_CLOCK = 0x6B,
    RESET_SD = 0x12,
    CMD_READ_SINGLE_BLOCK = 17,
    POWER_SETTLE_MS = 1,
    DETECT_SETTLE_MS = 1
};

static const GpioPin CARD_DETECT = {0, 11};
static const MmcHost SD_HOST = {SD_BASE, CRU_SD_IDENTIFY, CRU_SD_TRANSFER};

static SdCard card;

static bool host_command(void *context, SdCommand command, uint32_t response[4])
{
    (void)context;
    return mmc_command(&SD_HOST, command, response);
}

static void host_set_clock(void *context, SdClock clock)
{
    (void)context;
    mmc_set_clock(&SD_HOST, clock);
}

static void host_set_bus_width(void *context, uint8_t bits)
{
    (void)context;
    mmc_set_bus_width(&SD_HOST, bits);
}

static void host_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    board_delay_ms(milliseconds);
}

static bool host_card_present(void *context)
{
    (void)context;
    return !gpio_read(CARD_DETECT);
}

static void host_stop(void *context)
{
    (void)context;
    cru_gate(GATE_SD_CARD_CLOCK, true);
    mmc_stop_clock(&SD_HOST);
    cru_gate(GATE_SD_CARD_CLOCK, false);
    cru_gate(GATE_SD_BUS, false);
}

static const SdHost HOST = {host_command, host_set_clock,    host_set_bus_width,
                            host_delay,   host_card_present, host_stop,
                            NULL};

static void start_card_detect(void)
{
    register_write(GRF_BASE + GRF_DETECT_MUX, GRF_DETECT_MUX_VALUE);
    gpio_input(CARD_DETECT);
    register_write(GRF_BASE + GRF_DETECT_PULL, GRF_DETECT_PULL_VALUE);
    board_delay_ms(DETECT_SETTLE_MS);
}

static void power_controller(void)
{
    cru_gate(GATE_SD_BUS, true);
    cru_gate(GATE_SD_CARD_CLOCK, true);
    cru_reset(RESET_SD, false);
    cru_write(CRU_MMC_DIVIDERS, CRU_SD_IDENTIFY);
    register_write(GRF_BASE + GRF_SD_MUX_LOW, GRF_SD_MUX_LOW_VALUE);
    register_write(GRF_BASE + GRF_SD_MUX_HIGH, GRF_SD_MUX_HIGH_VALUE);
    board_delay_ms(POWER_SETTLE_MS);
    mmc_reset_controller(&SD_HOST);
    mmc_set_clock(&SD_HOST, SD_CLOCK_IDENTIFY);
    board_delay_ms(POWER_SETTLE_MS);
}

static bool read_aligned(uint32_t lba, uint32_t *words)
{
    return mmc_read_block(&SD_HOST, CMD_READ_SINGLE_BLOCK, sd_card_address(&card, lba), words);
}

static bool read_sectors(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    (void)context;
    uint8_t *bytes = buffer;
    for (uint32_t index = 0; index < count; index++) {
        if (lba + index >= card.sector_count ||
            !read_aligned(lba + index, (uint32_t *)(void *)(bytes + index * 512u))) {
            return false;
        }
    }
    return true;
}

bool sd_start(BlockDevice *device)
{
    start_card_detect();
    if (!host_card_present(NULL)) {
        return false;
    }
    power_controller();
    if (!sd_card_init(&card, &HOST)) {
        return false;
    }
    *device = (BlockDevice){read_sectors, NULL};
    return true;
}
