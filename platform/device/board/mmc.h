#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/sd_card.h"

typedef struct MmcHost {
    uint32_t base;
    uint32_t cru_identify;
    uint32_t cru_transfer;
} MmcHost;

void mmc_reset_controller(const MmcHost *host);
bool mmc_command(const MmcHost *host, SdCommand command, uint32_t response[4]);
bool mmc_read_block(const MmcHost *host, uint8_t index, uint32_t address, uint32_t *words);
void mmc_set_clock(const MmcHost *host, SdClock clock);
void mmc_set_bus_width(const MmcHost *host, uint8_t bits);
void mmc_stop_clock(const MmcHost *host);
