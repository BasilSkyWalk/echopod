#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "device/image_crc.h"
#include "emmc.h"
#include "hal/system.h"

#define RESIDENT_IMAGE_OFFSET 0x238200u
#define RESIDENT_MAGIC 0x53455245u

enum {
    SECTOR_BYTES = 512,
    HEADER_BYTES = 16,
    HEADER_WORDS = HEADER_BYTES / 4
};

extern uint32_t device_resident_start[];
extern uint32_t device_resident_end[];

static void clear_tables(uint32_t words)
{
    for (uint32_t index = 0; index < words; index++) {
        device_resident_start[index] = 0;
    }
}

bool board_resident_load(void)
{
    EmmcInfo info;
    uint32_t bytes = (uint32_t)((uintptr_t)device_resident_end - (uintptr_t)device_resident_start);
    uint32_t sectors = (bytes + SECTOR_BYTES - 1) / SECTOR_BYTES;
    uint32_t *table = device_resident_start;
    if (hal_system_restart_pending() || !emmc_start(&info) ||
        !emmc_read(NULL, info.layout.slot_1 + RESIDENT_IMAGE_OFFSET / SECTOR_BYTES, sectors,
                   table)) {
        clear_tables(bytes / 4);
        return false;
    }
    const uint8_t *payload = (const uint8_t *)(table + HEADER_WORDS);
    if (table[0] != RESIDENT_MAGIC || table[1] != bytes ||
        image_crc_update(0, payload, bytes - HEADER_BYTES) != table[2]) {
        clear_tables(bytes / 4);
        return false;
    }
    return true;
}
