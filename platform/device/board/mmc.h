#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/sd_card.h"

typedef enum MmcStep {
    MMC_STEP_NONE,
    MMC_STEP_BUSY_BEFORE,
    MMC_STEP_COMMAND_START,
    MMC_STEP_FIFO,
    MMC_STEP_DATA_WAIT,
    MMC_STEP_DATA_ERROR,
    MMC_STEP_BUSY_AFTER,
    MMC_STEP_TRANSFER_STATE,
    MMC_STEP_STOP
} MmcStep;

typedef struct MmcFailure {
    MmcStep step;
    uint32_t interrupts;
    uint32_t status;
    uint32_t count;
} MmcFailure;

typedef struct MmcHost {
    uint32_t base;
    uint32_t cru_identify;
    uint32_t cru_transfer;
    uint32_t *fifo_depth;
} MmcHost;

void mmc_reset_controller(const MmcHost *host);
void mmc_power_cycle(const MmcHost *host);
bool mmc_command(const MmcHost *host, SdCommand command, uint32_t response[4]);
bool mmc_read_block(const MmcHost *host, uint8_t index, uint32_t address, uint32_t *words);
bool mmc_read_blocks(const MmcHost *host, uint32_t address, uint32_t count, uint32_t stop_argument,
                     uint32_t *words);
bool mmc_write_block(const MmcHost *host, uint8_t index, uint32_t address, const uint32_t *words);
void mmc_set_clock(const MmcHost *host, SdClock clock);
void mmc_set_bus_width(const MmcHost *host, uint8_t bits);
void mmc_stop_clock(const MmcHost *host);
void mmc_note_failure(const MmcHost *host, MmcStep step);
MmcFailure mmc_last_failure(void);
