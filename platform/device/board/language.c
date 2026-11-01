#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "emmc.h"
#include "gfx/font_pack.h"
#include "hal/language.h"

enum {
    SECTOR_BYTES = 512,
    LANGUAGE_RAM_BYTES = 0x3000
};

static uint32_t pack[LANGUAGE_RAM_BYTES / 4] __attribute__((section(".language_ram")));

bool hal_language_pack(uint32_t language, const uint8_t **bytes, uint32_t *size)
{
    EmmcInfo info;
    uint32_t offset;
    uint32_t length;
    if (!board_glyphs_entry(FONT_PACK_FACES + language, &offset, &length) || length == 0 ||
        length > LANGUAGE_RAM_BYTES || offset % SECTOR_BYTES != 0 || !emmc_start(&info)) {
        return false;
    }
    uint32_t sectors = (length + SECTOR_BYTES - 1) / SECTOR_BYTES;
    if (!emmc_read(NULL, info.layout.slot_1 + offset / SECTOR_BYTES, sectors, pack)) {
        return false;
    }
    *bytes = (const uint8_t *)pack;
    *size = length;
    return true;
}
