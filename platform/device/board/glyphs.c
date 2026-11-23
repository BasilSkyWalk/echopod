#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "emmc.h"
#include "gfx/font_pack.h"
#include "hal/system.h"
#include "settings/settings.h"

#define GLYPH_TABLE_OFFSET 0x2C0000u
#define GLYPH_TABLE_MAGIC 0x54504745u
#define NO_SECTOR 0xFFFFFFFFu

enum {
    SECTOR_BYTES = 512,
    SECTOR_WORDS = SECTOR_BYTES / 4,
    GLYPH_RAM_BYTES = 0x8800,
    CACHE_WORDS = (GLYPH_RAM_BYTES - 2 * SECTOR_BYTES) / 4,
    MAX_ENTRIES = (SECTOR_BYTES - 8) / 8,
    FIRST_THEME_ENTRY = FONT_PACK_FACES + LANGUAGE_COUNT,
    BUILTIN_THEMES = THEME_COUNT
};

static uint32_t table[SECTOR_WORDS] __attribute__((section(".glyph_ram")));
static uint32_t sector[SECTOR_WORDS] __attribute__((section(".glyph_ram")));
static uint32_t cache[CACHE_WORDS] __attribute__((section(".glyph_ram")));
static uint32_t sector_lba;
static uint32_t slot_1;
static uint32_t face_count;
static uint32_t entry_count;

static bool read_sector(uint32_t lba)
{
    if (lba == sector_lba) {
        return true;
    }
    sector_lba = NO_SECTOR;
    if (!emmc_read(NULL, lba, 1, sector)) {
        return false;
    }
    sector_lba = lba;
    return true;
}

static bool read_entry(uint32_t entry, uint32_t offset, void *buffer, uint32_t length)
{
    if (hal_system_restart_pending() || entry >= entry_count) {
        return false;
    }
    uint32_t start = table[2 + 2 * entry];
    uint32_t size = table[3 + 2 * entry];
    if (offset > size || length > size - offset) {
        return false;
    }
    uint32_t at = start + offset;
    uint8_t *out = buffer;
    while (length > 0) {
        uint32_t within = at % SECTOR_BYTES;
        uint32_t chunk = SECTOR_BYTES - within < length ? SECTOR_BYTES - within : length;
        if (!read_sector(slot_1 + at / SECTOR_BYTES)) {
            return false;
        }
        const uint8_t *bytes = (const uint8_t *)sector + within;
        for (uint32_t index = 0; index < chunk; index++) {
            out[index] = bytes[index];
        }
        out += chunk;
        at += chunk;
        length -= chunk;
    }
    return true;
}

static bool read_face(void *context, uint32_t face, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    return face < face_count && read_entry(face, offset, buffer, length);
}

uint32_t hal_system_builtin_theme_size(uint32_t theme)
{
    uint32_t entry = FIRST_THEME_ENTRY + theme;
    return theme < BUILTIN_THEMES && entry < entry_count ? table[3 + 2 * entry] : 0;
}

bool hal_system_builtin_theme_read(uint32_t theme, uint32_t offset, void *target, uint32_t length)
{
    return theme < BUILTIN_THEMES && read_entry(FIRST_THEME_ENTRY + theme, offset, target, length);
}

bool board_glyphs_entry(uint32_t entry, uint32_t *offset, uint32_t *size)
{
    if (entry >= entry_count) {
        return false;
    }
    *offset = table[2 + 2 * entry];
    *size = table[3 + 2 * entry];
    return true;
}

void board_glyphs_start(void)
{
    EmmcInfo info;
    font_pack_detach();
    face_count = 0;
    entry_count = 0;
    sector_lba = NO_SECTOR;
    board_resident_load();
    if (!emmc_start(&info) ||
        !emmc_read(NULL, info.layout.slot_1 + GLYPH_TABLE_OFFSET / SECTOR_BYTES, 1, table) ||
        table[0] != GLYPH_TABLE_MAGIC) {
        return;
    }
    slot_1 = info.layout.slot_1;
    entry_count = table[1] < MAX_ENTRIES ? table[1] : MAX_ENTRIES;
    face_count = table[1] < FONT_PACK_FACES ? table[1] : FONT_PACK_FACES;
    font_pack_attach((FontPackReader){read_face, NULL}, cache, sizeof cache);
}
