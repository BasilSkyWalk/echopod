#include "device/sd_card.h"

#define VOLTAGE_WINDOW 0x00FF8000u
#define HIGH_CAPACITY_REQUEST 0x40000000u
#define CARD_READY 0x80000000u
#define CARD_HIGH_CAPACITY 0x40000000u

enum {
    CMD_GO_IDLE = 0,
    CMD_ALL_SEND_CID = 2,
    CMD_SEND_RELATIVE_ADDRESS = 3,
    CMD_SELECT = 7,
    CMD_SEND_IF_COND = 8,
    CMD_SEND_CSD = 9,
    CMD_SET_BLOCK_LENGTH = 16,
    CMD_APP = 55,
    ACMD_SET_BUS_WIDTH = 6,
    ACMD_SEND_OP_COND = 41,
    ACMD_SET_CLEAR_CARD_DETECT = 42,
    IF_COND_PATTERN = 0x1AA,
    IF_COND_MASK = 0xFFF,
    READY_TRIES = 100,
    READY_RETRY_MS = 10,
    RCA_ATTEMPTS = 4,
    BLOCK_LENGTH = 512,
    SECTOR_SHIFT = 9,
    WIDE_BUS_ARGUMENT = 2,
    WIDE_BUS_BITS = 4
};

static bool send(const SdHost *host, SdCommand command, uint32_t response[4])
{
    return host->command(host->context, command, response);
}

static bool send_short(const SdHost *host, uint8_t index, uint32_t argument, SdResponse type,
                       uint32_t *response)
{
    uint32_t words[4] = {0, 0, 0, 0};
    bool ok = send(host, (SdCommand){index, argument, type, false}, words);
    *response = words[0];
    return ok;
}

static bool send_app(const SdHost *host, const SdCard *card, uint8_t index, uint32_t argument,
                     uint32_t *response)
{
    uint32_t status = 0;
    if (!send_short(host, CMD_APP, sd_card_rca_argument(card), SD_RESPONSE_SHORT, &status)) {
        return false;
    }
    SdResponse type = index == ACMD_SEND_OP_COND ? SD_RESPONSE_SHORT_NO_CRC : SD_RESPONSE_SHORT;
    return send_short(host, index, argument, type, response);
}

static bool interface_condition(const SdHost *host, bool *version_2)
{
    uint32_t echo = 0;
    *version_2 = send_short(host, CMD_SEND_IF_COND, IF_COND_PATTERN, SD_RESPONSE_SHORT, &echo);
    return !*version_2 || (echo & IF_COND_MASK) == IF_COND_PATTERN;
}

static bool wait_until_ready(const SdHost *host, SdCard *card, bool version_2)
{
    uint32_t argument = VOLTAGE_WINDOW | (version_2 ? HIGH_CAPACITY_REQUEST : 0);
    for (int attempt = 0; attempt < READY_TRIES && host->card_present(host->context); attempt++) {
        uint32_t ocr = 0;
        if (send_app(host, card, ACMD_SEND_OP_COND, argument, &ocr) && (ocr & CARD_READY) != 0) {
            card->high_capacity = version_2 && (ocr & CARD_HIGH_CAPACITY) != 0;
            return true;
        }
        host->delay_ms(host->context, READY_RETRY_MS);
    }
    return false;
}

static bool assign_address(const SdHost *host, SdCard *card)
{
    for (int attempt = 0; attempt < RCA_ATTEMPTS; attempt++) {
        uint32_t response = 0;
        if (send_short(host, CMD_SEND_RELATIVE_ADDRESS, 0, SD_RESPONSE_SHORT, &response) &&
            (response >> 16) != 0) {
            card->rca = (uint16_t)(response >> 16);
            return true;
        }
    }
    return false;
}

static uint32_t csd_bits(const uint32_t csd[4], int start, int size)
{
    uint32_t value = 0;
    for (int bit = size - 1; bit >= 0; bit--) {
        int position = start + bit;
        value = value << 1 | ((csd[position / 32] >> (position % 32)) & 1u);
    }
    return value;
}

static uint32_t csd_sectors(const uint32_t csd[4])
{
    if (csd_bits(csd, 126, 2) == 1) {
        return (csd_bits(csd, 48, 22) + 1) * 1024u;
    }
    uint32_t blocks = (csd_bits(csd, 62, 12) + 1) << (csd_bits(csd, 47, 3) + 2);
    uint32_t block_length = csd_bits(csd, 80, 4);
    return block_length >= SECTOR_SHIFT ? blocks << (block_length - SECTOR_SHIFT)
                                        : blocks >> (SECTOR_SHIFT - block_length);
}

static bool identify(const SdHost *host, SdCard *card)
{
    uint32_t words[4] = {0, 0, 0, 0};
    if (!send(host, (SdCommand){CMD_ALL_SEND_CID, 0, SD_RESPONSE_LONG, false}, words) ||
        !assign_address(host, card)) {
        return false;
    }
    SdCommand csd = {CMD_SEND_CSD, sd_card_rca_argument(card), SD_RESPONSE_LONG, false};
    if (!send(host, csd, words)) {
        return false;
    }
    card->sector_count = csd_sectors(words);
    return true;
}

static bool select_card(const SdHost *host, SdCard *card)
{
    uint32_t status = 0;
    host->set_clock(host->context, SD_CLOCK_TRANSFER);
    if (!send_short(host, CMD_SELECT, sd_card_rca_argument(card), SD_RESPONSE_SHORT_BUSY,
                    &status)) {
        return false;
    }
    return card->high_capacity ||
           send_short(host, CMD_SET_BLOCK_LENGTH, BLOCK_LENGTH, SD_RESPONSE_SHORT, &status);
}

static void widen_bus(const SdHost *host, SdCard *card)
{
    uint32_t status = 0;
    send_app(host, card, ACMD_SET_CLEAR_CARD_DETECT, 0, &status);
    card->bus_width = 1;
    if (send_app(host, card, ACMD_SET_BUS_WIDTH, WIDE_BUS_ARGUMENT, &status)) {
        host->set_bus_width(host->context, WIDE_BUS_BITS);
        card->bus_width = WIDE_BUS_BITS;
    }
}

static bool start_card(const SdHost *host, SdCard *card)
{
    bool version_2 = false;
    uint32_t none[4] = {0, 0, 0, 0};
    send(host, (SdCommand){CMD_GO_IDLE, 0, SD_RESPONSE_NONE, true}, none);
    host->delay_ms(host->context, 1);
    return interface_condition(host, &version_2) && wait_until_ready(host, card, version_2) &&
           identify(host, card) && select_card(host, card);
}

bool sd_card_init(SdCard *card, const SdHost *host)
{
    *card = (SdCard){.bus_width = 1};
    if (!host->card_present(host->context)) {
        return false;
    }
    if (!start_card(host, card)) {
        host->stop(host->context);
        return false;
    }
    widen_bus(host, card);
    return true;
}

uint32_t sd_card_address(const SdCard *card, uint32_t lba)
{
    return card->high_capacity ? lba : lba << SECTOR_SHIFT;
}

uint32_t sd_card_rca_argument(const SdCard *card)
{
    return (uint32_t)card->rca << 16;
}
