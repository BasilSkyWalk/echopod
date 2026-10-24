#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum SdResponse {
    SD_RESPONSE_NONE,
    SD_RESPONSE_SHORT,
    SD_RESPONSE_SHORT_NO_CRC,
    SD_RESPONSE_SHORT_BUSY,
    SD_RESPONSE_LONG
} SdResponse;

typedef enum SdClock {
    SD_CLOCK_IDENTIFY,
    SD_CLOCK_TRANSFER
} SdClock;

typedef struct SdCommand {
    uint8_t index;
    uint32_t argument;
    SdResponse response;
    bool initialise;
} SdCommand;

typedef struct SdHost {
    bool (*command)(void *context, SdCommand command, uint32_t response[4]);
    void (*set_clock)(void *context, SdClock clock);
    void (*set_bus_width)(void *context, uint8_t bits);
    void (*delay_ms)(void *context, uint32_t milliseconds);
    bool (*card_present)(void *context);
    void (*stop)(void *context);
    void *context;
} SdHost;

typedef struct SdCard {
    bool high_capacity;
    uint16_t rca;
    uint32_t sector_count;
    uint8_t bus_width;
} SdCard;

bool sd_card_init(SdCard *card, const SdHost *host);
uint32_t sd_card_address(const SdCard *card, uint32_t lba);
uint32_t sd_card_rca_argument(const SdCard *card);
