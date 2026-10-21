#include "mmc.h"

#include "board.h"
#include "cru.h"
#include "hal/clock.h"
#include "registers.h"

#define MMC_CTRL 0x00u
#define MMC_PWREN 0x04u
#define MMC_CLKDIV 0x08u
#define MMC_CLKSRC 0x0Cu
#define MMC_CLKENA 0x10u
#define MMC_TMOUT 0x14u
#define MMC_CTYPE 0x18u
#define MMC_BLKSIZ 0x1Cu
#define MMC_BYTCNT 0x20u
#define MMC_INTMASK 0x24u
#define MMC_CMDARG 0x28u
#define MMC_CMD 0x2Cu
#define MMC_RESP0 0x30u
#define MMC_RINTSTS 0x44u
#define MMC_STATUS 0x48u
#define MMC_FIFOTH 0x4Cu
#define MMC_FIFO 0x200u

#define CMD_START (1u << 31)
#define CMD_BASE 0xA0002000u
#define CMD_UPDATE_CLOCK 0x80202000u
#define CMD_INITIALISE (1u << 15)
#define CMD_STOP_ABORT (1u << 14)
#define CMD_DATA_READ 0x340u
#define CMD_WRITE (1u << 10)
#define CMD_CHECK_CRC (1u << 8)
#define CMD_LONG (1u << 7)
#define CMD_RESPONSE (1u << 6)

#define INT_RESPONSE_ERROR (1u << 1)
#define INT_COMMAND_DONE (1u << 2)
#define INT_DATA_OVER (1u << 3)
#define INT_RESPONSE_CRC (1u << 6)
#define INT_RESPONSE_TIMEOUT (1u << 8)
#define INT_HARDWARE_LOCKED (1u << 12)
#define INT_DATA_ABORT 0xAF82u
#define INT_DATA_FINISHED 0xAE88u
#define INT_DATA_FAILED 0xAE80u
#define INT_ALL 0xFFFFFFFFu

#define STATUS_FIFO_EMPTY (1u << 2)
#define STATUS_CARD_BUSY (1u << 9)
#define STATUS_FIFO_COUNT_SHIFT 17
#define STATUS_FIFO_COUNT_MASK 0x1FFFu

#define CTRL_RESET_ALL 0x7u
#define CTRL_FIFO_RESET 0x2u
#define FIFOTH_VALUE 0x300F0010u
#define TMOUT_VALUE 0xFFFFFF40u
#define CLKENA_ON 0x00010001u
#define CTYPE_4_BIT 0x1u
#define CTYPE_8_BIT 0x10000u
#define FIFOTH_RX_WATERMARK_SHIFT 16
#define FIFOTH_RX_WATERMARK_MASK 0xFFFu

#define CRU_MMC_DIVIDERS 0x30u

enum {
    BLOCK_WORDS = 128,
    BLOCK_BYTES = 512,
    CMD_READ_SINGLE_BLOCK_INDEX = 17,
    CMD_READ_MULTIPLE_BLOCK = 18,
    CMD_STOP_TRANSMISSION = 12,
    COMMAND_TIMEOUT_MS = 100,
    BUSY_TIMEOUT_MS = 1000,
    DATA_TIMEOUT_MS = 500,
    IDENTIFY_DIVIDER_REGISTER = 1,
    FALLBACK_FIFO_DEPTH = 16,
    MINIMUM_FIFO_DEPTH = 8,
    MAXIMUM_FIFO_DEPTH = 1024,
    POWER_OFF_US = 10,
    POWER_ON_US = 600
};

static MmcFailure last_failure;

static uint32_t read_register(const MmcHost *host, uint32_t offset)
{
    return register_read(host->base + offset);
}

void mmc_note_failure(const MmcHost *host, MmcStep step)
{
    last_failure = (MmcFailure){step, read_register(host, MMC_RINTSTS),
                                read_register(host, MMC_STATUS), last_failure.count + 1};
}

MmcFailure mmc_last_failure(void)
{
    return last_failure;
}

static void write_register(const MmcHost *host, uint32_t offset, uint32_t value)
{
    register_write(host->base + offset, value);
}

static bool wait_clear(const MmcHost *host, uint32_t offset, uint32_t bits, uint32_t timeout_ms)
{
    uint32_t start = hal_clock_milliseconds();
    while ((read_register(host, offset) & bits) != 0) {
        if (hal_clock_milliseconds() - start > timeout_ms) {
            return false;
        }
    }
    return true;
}

static bool wait_any(const MmcHost *host, uint32_t offset, uint32_t bits, uint32_t timeout_ms)
{
    uint32_t start = hal_clock_milliseconds();
    while ((read_register(host, offset) & bits) == 0) {
        if (hal_clock_milliseconds() - start > timeout_ms) {
            return false;
        }
    }
    return true;
}

static bool update_clock(const MmcHost *host)
{
    write_register(host, MMC_CMD, CMD_UPDATE_CLOCK);
    return wait_clear(host, MMC_CMD, CMD_START, COMMAND_TIMEOUT_MS);
}

static uint32_t fifo_depth(const MmcHost *host)
{
    uint32_t watermark = read_register(host, MMC_FIFOTH) >> FIFOTH_RX_WATERMARK_SHIFT;
    uint32_t depth = (watermark & FIFOTH_RX_WATERMARK_MASK) + 1;
    return depth < MINIMUM_FIFO_DEPTH || depth > MAXIMUM_FIFO_DEPTH ? FALLBACK_FIFO_DEPTH : depth;
}

void mmc_reset_controller(const MmcHost *host)
{
    write_register(host, MMC_CTRL, CTRL_RESET_ALL);
    wait_clear(host, MMC_CTRL, CTRL_RESET_ALL, COMMAND_TIMEOUT_MS);
    *host->fifo_depth = fifo_depth(host);
    write_register(host, MMC_FIFOTH, FIFOTH_VALUE);
    write_register(host, MMC_CTYPE, 0);
    write_register(host, MMC_CLKSRC, 0);
    write_register(host, MMC_TMOUT, TMOUT_VALUE);
    write_register(host, MMC_RINTSTS, INT_ALL);
    write_register(host, MMC_INTMASK, 0);
    write_register(host, MMC_PWREN, 1);
}

void mmc_set_clock(const MmcHost *host, SdClock clock)
{
    bool identify = clock == SD_CLOCK_IDENTIFY;
    write_register(host, MMC_CLKENA, 0);
    update_clock(host);
    cru_write(CRU_MMC_DIVIDERS, identify ? host->cru_identify : host->cru_transfer);
    write_register(host, MMC_CLKDIV, identify ? IDENTIFY_DIVIDER_REGISTER : 0);
    update_clock(host);
    write_register(host, MMC_CLKENA, CLKENA_ON);
    update_clock(host);
}

void mmc_power_cycle(const MmcHost *host)
{
    write_register(host, MMC_PWREN, 0);
    board_delay_us(POWER_OFF_US);
    write_register(host, MMC_PWREN, 1);
    board_delay_us(POWER_ON_US);
}

void mmc_set_bus_width(const MmcHost *host, uint8_t bits)
{
    uint32_t type = bits == 8 ? CTYPE_8_BIT : bits == 4 ? CTYPE_4_BIT : 0;
    write_register(host, MMC_CTYPE, type);
}

void mmc_stop_clock(const MmcHost *host)
{
    write_register(host, MMC_CLKENA, 0);
    update_clock(host);
}

static uint32_t command_flags(SdCommand command)
{
    uint32_t flags = command.initialise ? CMD_INITIALISE : 0;
    if (command.response != SD_RESPONSE_NONE) {
        flags |= CMD_RESPONSE;
    }
    if (command.response == SD_RESPONSE_LONG) {
        flags |= CMD_LONG;
    }
    if (command.response != SD_RESPONSE_NONE && command.response != SD_RESPONSE_SHORT_NO_CRC) {
        flags |= CMD_CHECK_CRC;
    }
    return flags;
}

static bool command_failed(SdCommand command, uint32_t status)
{
    uint32_t errors = INT_RESPONSE_ERROR | INT_RESPONSE_TIMEOUT | INT_HARDWARE_LOCKED;
    if ((command_flags(command) & CMD_CHECK_CRC) != 0) {
        errors |= INT_RESPONSE_CRC;
    }
    return (status & errors) != 0;
}

static void read_response(const MmcHost *host, uint32_t response[4])
{
    for (uint32_t index = 0; index < 4; index++) {
        response[index] = read_register(host, MMC_RESP0 + 4u * index);
    }
}

bool mmc_command(const MmcHost *host, SdCommand command, uint32_t response[4])
{
    if (!wait_clear(host, MMC_STATUS, STATUS_CARD_BUSY, BUSY_TIMEOUT_MS)) {
        return false;
    }
    write_register(host, MMC_RINTSTS, INT_ALL);
    write_register(host, MMC_CMDARG, command.argument);
    write_register(host, MMC_CMD, CMD_BASE | command_flags(command) | command.index);
    if (!wait_clear(host, MMC_CMD, CMD_START, COMMAND_TIMEOUT_MS) ||
        !wait_any(host, MMC_RINTSTS, INT_COMMAND_DONE, COMMAND_TIMEOUT_MS) ||
        command_failed(command, read_register(host, MMC_RINTSTS))) {
        return false;
    }
    read_response(host, response);
    return command.response != SD_RESPONSE_SHORT_BUSY ||
           wait_clear(host, MMC_STATUS, STATUS_CARD_BUSY, BUSY_TIMEOUT_MS);
}

static bool prepare_data(const MmcHost *host)
{
    if (!wait_clear(host, MMC_STATUS, STATUS_CARD_BUSY, BUSY_TIMEOUT_MS)) {
        return false;
    }
    if ((read_register(host, MMC_STATUS) & STATUS_FIFO_EMPTY) == 0) {
        write_register(host, MMC_CTRL, read_register(host, MMC_CTRL) | CTRL_FIFO_RESET);
        return wait_clear(host, MMC_CTRL, CTRL_FIFO_RESET, COMMAND_TIMEOUT_MS);
    }
    return true;
}

static bool drain_fifo(const MmcHost *host, uint32_t *words, uint32_t total)
{
    uint32_t received = 0;
    uint32_t start = hal_clock_milliseconds();
    while (received < total) {
        if ((read_register(host, MMC_RINTSTS) & INT_DATA_ABORT) != 0 ||
            hal_clock_milliseconds() - start > DATA_TIMEOUT_MS) {
            return false;
        }
        uint32_t status = read_register(host, MMC_STATUS);
        uint32_t count = (status >> STATUS_FIFO_COUNT_SHIFT) & STATUS_FIFO_COUNT_MASK;
        while (count-- > 0 && received < total) {
            words[received++] = read_register(host, MMC_FIFO);
        }
    }
    return true;
}

static bool failed(const MmcHost *host, MmcStep step);

static bool read_data(const MmcHost *host, uint8_t index, uint32_t address, uint32_t count,
                      uint32_t *words)
{
    if (!prepare_data(host)) {
        return failed(host, MMC_STEP_BUSY_BEFORE);
    }
    write_register(host, MMC_BLKSIZ, BLOCK_BYTES);
    write_register(host, MMC_BYTCNT, BLOCK_BYTES * count);
    write_register(host, MMC_RINTSTS, INT_ALL);
    write_register(host, MMC_CMDARG, address);
    write_register(host, MMC_CMD, CMD_BASE | CMD_DATA_READ | index);
    if (!wait_clear(host, MMC_CMD, CMD_START, COMMAND_TIMEOUT_MS)) {
        return failed(host, MMC_STEP_COMMAND_START);
    }
    if (!drain_fifo(host, words, BLOCK_WORDS * count)) {
        return failed(host, MMC_STEP_FIFO);
    }
    if (!wait_any(host, MMC_RINTSTS, INT_DATA_FINISHED, DATA_TIMEOUT_MS)) {
        return failed(host, MMC_STEP_DATA_WAIT);
    }
    if ((read_register(host, MMC_RINTSTS) & INT_DATA_FAILED) != 0) {
        return failed(host, MMC_STEP_DATA_ERROR);
    }
    write_register(host, MMC_RINTSTS, INT_ALL);
    return true;
}

bool mmc_read_block(const MmcHost *host, uint8_t index, uint32_t address, uint32_t *words)
{
    return read_data(host, index, address, 1, words);
}

static bool stop_transmission(const MmcHost *host, uint32_t argument)
{
    write_register(host, MMC_RINTSTS, INT_ALL);
    write_register(host, MMC_CMDARG, argument);
    write_register(host, MMC_CMD,
                   CMD_BASE | CMD_STOP_ABORT | CMD_RESPONSE | CMD_CHECK_CRC |
                       CMD_STOP_TRANSMISSION);
    SdCommand stop = {CMD_STOP_TRANSMISSION, argument, SD_RESPONSE_SHORT_BUSY, false};
    if (!wait_clear(host, MMC_CMD, CMD_START, COMMAND_TIMEOUT_MS) ||
        !wait_any(host, MMC_RINTSTS, INT_COMMAND_DONE, COMMAND_TIMEOUT_MS) ||
        command_failed(stop, read_register(host, MMC_RINTSTS))) {
        return failed(host, MMC_STEP_STOP);
    }
    if (!wait_clear(host, MMC_STATUS, STATUS_CARD_BUSY, BUSY_TIMEOUT_MS)) {
        return failed(host, MMC_STEP_BUSY_AFTER);
    }
    write_register(host, MMC_RINTSTS, INT_ALL);
    return true;
}

bool mmc_read_blocks(const MmcHost *host, uint32_t address, uint32_t count, uint32_t stop_argument,
                     uint32_t *words)
{
    if (count == 1) {
        return read_data(host, CMD_READ_SINGLE_BLOCK_INDEX, address, 1, words);
    }
    bool read = read_data(host, CMD_READ_MULTIPLE_BLOCK, address, count, words);
    return stop_transmission(host, stop_argument) && read;
}

static bool fill_fifo(const MmcHost *host, const uint32_t *words, uint32_t total)
{
    uint32_t sent = 0;
    uint32_t start = hal_clock_milliseconds();
    while (sent < total) {
        if ((read_register(host, MMC_RINTSTS) & INT_DATA_ABORT) != 0 ||
            hal_clock_milliseconds() - start > DATA_TIMEOUT_MS) {
            return false;
        }
        uint32_t status = read_register(host, MMC_STATUS);
        uint32_t count = (status >> STATUS_FIFO_COUNT_SHIFT) & STATUS_FIFO_COUNT_MASK;
        uint32_t depth = *host->fifo_depth;
        uint32_t space = count < depth ? depth - count : 0;
        if (space > 0) {
            start = hal_clock_milliseconds();
        }
        while (space-- > 0 && sent < total) {
            write_register(host, MMC_FIFO, words[sent++]);
        }
    }
    return true;
}

static bool failed(const MmcHost *host, MmcStep step)
{
    mmc_note_failure(host, step);
    write_register(host, MMC_RINTSTS, INT_ALL);
    return false;
}

bool mmc_stop(const MmcHost *host, uint32_t argument)
{
    return stop_transmission(host, argument);
}

bool mmc_write_blocks(const MmcHost *host, uint8_t index, uint32_t address, uint32_t count,
                      const uint32_t *words)
{
    if (!prepare_data(host)) {
        return failed(host, MMC_STEP_BUSY_BEFORE);
    }
    write_register(host, MMC_BLKSIZ, BLOCK_BYTES);
    write_register(host, MMC_BYTCNT, BLOCK_BYTES * count);
    write_register(host, MMC_RINTSTS, INT_ALL);
    write_register(host, MMC_CMDARG, address);
    write_register(host, MMC_CMD, CMD_BASE | CMD_DATA_READ | CMD_WRITE | index);
    if (!wait_clear(host, MMC_CMD, CMD_START, COMMAND_TIMEOUT_MS)) {
        return failed(host, MMC_STEP_COMMAND_START);
    }
    if (!fill_fifo(host, words, BLOCK_WORDS * count)) {
        return failed(host, MMC_STEP_FIFO);
    }
    if (!wait_any(host, MMC_RINTSTS, INT_DATA_FINISHED, DATA_TIMEOUT_MS)) {
        return failed(host, MMC_STEP_DATA_WAIT);
    }
    if ((read_register(host, MMC_RINTSTS) & INT_DATA_FAILED) != 0) {
        return failed(host, MMC_STEP_DATA_ERROR);
    }
    write_register(host, MMC_RINTSTS, INT_ALL);
    if (!wait_clear(host, MMC_STATUS, STATUS_CARD_BUSY, BUSY_TIMEOUT_MS)) {
        return failed(host, MMC_STEP_BUSY_AFTER);
    }
    return true;
}
