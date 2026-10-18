#include "emmc.h"

#include <stddef.h>

#include "board.h"
#include "cru.h"
#include "mmc.h"
#include "registers.h"

#define CRU_EMMC_IDENTIFY 0x9F009F00u
#define CRU_EMMC_TRANSFER 0x9F008000u
#define CRU_MMC_DIVIDERS 0x30u
#define GRF_EMMC_MUX_0 0x00u
#define GRF_EMMC_MUX_0_VALUE 0xFFFF5555u
#define GRF_EMMC_MUX_1 0x04u
#define GRF_EMMC_MUX_1_VALUE 0x003F0015u
#define GRF_EMMC_MUX_2 0x24u
#define GRF_EMMC_MUX_2_VALUE 0x00C00040u
#define OCR_ARGUMENT 0x40FF8000u
#define OCR_READY (1u << 31)
#define OCR_SECTOR_MODE (1u << 30)
#define RCA_ARGUMENT 0x00020000u
#define SWITCH_BUS_WIDTH_8 0x03B70200u
#define STATUS_READY_FOR_DATA (1u << 8)
#define STATUS_STATE_SHIFT 9
#define STATUS_STATE_MASK 0xFu
#define STATE_TRANSFER 4u

enum {
    GATE_EMMC_BUS = 0x19,
    GATE_EMMC_CARD_CLOCK = 0x6C,
    RESET_EMMC = 0x11,
    CMD_GO_IDLE = 0,
    CMD_SEND_OP_COND = 1,
    CMD_ALL_SEND_CID = 2,
    CMD_SET_RELATIVE_ADDRESS = 3,
    CMD_SWITCH = 6,
    CMD_SELECT = 7,
    CMD_SEND_EXT_CSD = 8,
    CMD_SEND_CSD = 9,
    CMD_SEND_STATUS = 13,
    CMD_SET_BLOCK_LENGTH = 16,
    CMD_READ_SINGLE_BLOCK = 17,
    CMD_WRITE_BLOCK = 24,
    OCR_TRIES = 8000,
    STATUS_TRIES = 1000,
    EXT_CSD_SECTOR_COUNT = 212,
    MUX_SETTLE_US = 10,
    IDLE_SETTLE_US = 100,
    BLOCK_LENGTH = 512
};

static uint32_t emmc_fifo_depth;
static const MmcHost EMMC_HOST = {EMMC_BASE, CRU_EMMC_IDENTIFY, CRU_EMMC_TRANSFER,
                                  &emmc_fifo_depth};
static EmmcInfo emmc;
static bool started;

static bool command(uint8_t index, uint32_t argument, SdResponse response, uint32_t words[4])
{
    return mmc_command(&EMMC_HOST, (SdCommand){index, argument, response, false}, words);
}

static void power_controller(void)
{
    cru_gate(GATE_EMMC_BUS, true);
    cru_gate(GATE_EMMC_CARD_CLOCK, true);
    cru_reset(RESET_EMMC, false);
    cru_write(CRU_MMC_DIVIDERS, CRU_EMMC_IDENTIFY);
    register_write(GRF_BASE + GRF_EMMC_MUX_0, GRF_EMMC_MUX_0_VALUE);
    register_write(GRF_BASE + GRF_EMMC_MUX_1, GRF_EMMC_MUX_1_VALUE);
    register_write(GRF_BASE + GRF_EMMC_MUX_2, GRF_EMMC_MUX_2_VALUE);
    board_delay_us(MUX_SETTLE_US);
    mmc_reset_controller(&EMMC_HOST);
    mmc_set_clock(&EMMC_HOST, SD_CLOCK_IDENTIFY);
    mmc_power_cycle(&EMMC_HOST);
}

static bool wait_for_ocr(void)
{
    uint32_t words[4] = {0, 0, 0, 0};
    for (int attempt = 0; attempt < OCR_TRIES; attempt++) {
        if (command(CMD_SEND_OP_COND, OCR_ARGUMENT, SD_RESPONSE_SHORT_NO_CRC, words) &&
            (words[0] & OCR_READY) != 0) {
            return (words[0] & OCR_SECTOR_MODE) != 0;
        }
        board_delay_ms(1);
    }
    return false;
}

static uint32_t cid_serial(const uint32_t cid[4])
{
    return (cid[1] & 0xFFFFu) << 16 | cid[0] >> 16;
}

static bool identify(void)
{
    uint32_t words[4] = {0, 0, 0, 0};
    uint32_t none[4] = {0, 0, 0, 0};
    mmc_command(&EMMC_HOST, (SdCommand){CMD_GO_IDLE, 0, SD_RESPONSE_NONE, true}, none);
    board_delay_us(IDLE_SETTLE_US);
    if (!wait_for_ocr() || !command(CMD_ALL_SEND_CID, 0, SD_RESPONSE_LONG, words)) {
        return false;
    }
    emmc.serial = cid_serial(words);
    return command(CMD_SET_RELATIVE_ADDRESS, RCA_ARGUMENT, SD_RESPONSE_SHORT, words) &&
           command(CMD_SEND_CSD, RCA_ARGUMENT, SD_RESPONSE_LONG, words);
}

static bool wait_for_transfer_state(void)
{
    uint32_t words[4] = {0, 0, 0, 0};
    for (int attempt = 0; attempt < STATUS_TRIES; attempt++) {
        if (command(CMD_SEND_STATUS, RCA_ARGUMENT, SD_RESPONSE_SHORT, words) &&
            (words[0] & STATUS_READY_FOR_DATA) != 0 &&
            ((words[0] >> STATUS_STATE_SHIFT) & STATUS_STATE_MASK) == STATE_TRANSFER) {
            return true;
        }
    }
    return false;
}

static bool read_sector_count(void)
{
    static uint32_t ext_csd[BLOCK_LENGTH / 4];
    if (!mmc_read_block(&EMMC_HOST, CMD_SEND_EXT_CSD, 0, ext_csd)) {
        return false;
    }
    emmc.sector_count = ext_csd[EXT_CSD_SECTOR_COUNT / 4];
    return emmc.sector_count != 0;
}

static bool select_and_widen(void)
{
    uint32_t words[4] = {0, 0, 0, 0};
    mmc_set_clock(&EMMC_HOST, SD_CLOCK_TRANSFER);
    if (!command(CMD_SELECT, RCA_ARGUMENT, SD_RESPONSE_SHORT_BUSY, words) ||
        !command(CMD_SET_BLOCK_LENGTH, BLOCK_LENGTH, SD_RESPONSE_SHORT, words) ||
        !read_sector_count() ||
        !command(CMD_SWITCH, SWITCH_BUS_WIDTH_8, SD_RESPONSE_SHORT_BUSY, words) ||
        !wait_for_transfer_state()) {
        return false;
    }
    mmc_set_bus_width(&EMMC_HOST, 8);
    return true;
}

bool emmc_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    (void)context;
    uint32_t *words = buffer;
    for (uint32_t index = 0; index < count; index++) {
        if (!started || lba >= emmc.sector_count || index >= emmc.sector_count - lba ||
            !mmc_read_block(&EMMC_HOST, CMD_READ_SINGLE_BLOCK, lba + index,
                            words + index * (BLOCK_LENGTH / 4))) {
            return false;
        }
    }
    return true;
}

bool emmc_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    (void)context;
    const uint32_t *words = buffer;
    if (!started || !emmc_layout_allows_write(&emmc.layout, lba, count)) {
        return false;
    }
    for (uint32_t index = 0; index < count; index++) {
        if (!mmc_write_block(&EMMC_HOST, CMD_WRITE_BLOCK, lba + index,
                             words + index * (BLOCK_LENGTH / 4))) {
            return false;
        }
        if (!wait_for_transfer_state()) {
            mmc_note_failure(&EMMC_HOST, MMC_STEP_TRANSFER_STATE);
            return false;
        }
    }
    return true;
}

uint32_t emmc_fifo_depth_words(void)
{
    return emmc_fifo_depth;
}

BlockDevice emmc_device(void)
{
    return (BlockDevice){emmc_read, NULL, NULL};
}

static bool inside_volume(uint32_t lba, uint32_t count)
{
    return lba < emmc.layout.volume_sectors && count <= emmc.layout.volume_sectors - lba;
}

bool emmc_volume_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    return inside_volume(lba, count) &&
           emmc_read(context, emmc.layout.volume_start + lba, count, buffer);
}

bool emmc_volume_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    return inside_volume(lba, count) &&
           emmc_write(context, emmc.layout.volume_start + lba, count, buffer);
}

bool emmc_start(EmmcInfo *info)
{
    if (!started) {
        power_controller();
        started = identify() && select_and_widen();
        BlockDevice device = emmc_device();
        started = started && emmc_layout_read(&emmc.layout, &device, emmc.sector_count);
    }
    *info = emmc;
    return started;
}
