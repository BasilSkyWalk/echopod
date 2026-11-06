#include "overlay.h"

#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "device/image_crc.h"
#include "emmc.h"

#define CODE_BASE 0x03050000u

enum {
    SECTOR_BYTES = 512,
    SECTORS_PER_READ = 8
};

extern const uint8_t device_code_end[];

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static uint32_t running_code_crc(void)
{
    static uint32_t crc;
    static bool known;
    if (!known) {
        const uint8_t *code = (const uint8_t *)(uintptr_t)CODE_BASE;
        crc = image_crc_update(0, code, (uint32_t)((uintptr_t)device_code_end - CODE_BASE));
        known = true;
    }
    return crc;
}

static bool read_sectors(uint32_t first_sector, uint32_t sectors, uint8_t *target)
{
    for (uint32_t done = 0; done < sectors; done += SECTORS_PER_READ) {
        uint32_t count = sectors - done < SECTORS_PER_READ ? sectors - done : SECTORS_PER_READ;
        if (!emmc_read(NULL, first_sector + done, count, target + done * SECTOR_BYTES)) {
            return false;
        }
    }
    return true;
}

static uint32_t header_size(const BoardOverlay *overlay)
{
    const uint8_t *base = overlay->base;
    uint32_t size = le32(base + 4);
    uint32_t sectors = (size + SECTOR_BYTES - 1) / SECTOR_BYTES;
    if (le32(base) != overlay->magic || size < OVERLAY_HEADER_BYTES ||
        sectors * SECTOR_BYTES > overlay->capacity || le32(base + 12) != running_code_crc()) {
        return 0;
    }
    return size;
}

static bool body_intact(const BoardOverlay *overlay, uint32_t size)
{
    const uint8_t *base = overlay->base;
    return image_crc_update(0, base + OVERLAY_HEADER_BYTES, size - OVERLAY_HEADER_BYTES) ==
           le32(base + 8);
}

static uint32_t already_loaded(const BoardOverlay *overlay)
{
    uint32_t size = header_size(overlay);
    return size != 0 && body_intact(overlay, size) ? size : 0;
}

static uint32_t load_from(uint32_t slot, const BoardOverlay *overlay)
{
    uint32_t first_sector = slot + overlay->image_offset / SECTOR_BYTES;
    if (!emmc_read(NULL, first_sector, 1, overlay->base)) {
        return 0;
    }
    uint32_t size = header_size(overlay);
    uint32_t sectors = (size + SECTOR_BYTES - 1) / SECTOR_BYTES;
    if (size == 0 || !read_sectors(first_sector + 1, sectors - 1, overlay->base + SECTOR_BYTES) ||
        !body_intact(overlay, size)) {
        return 0;
    }
    return size;
}

bool board_overlay_load(const BoardOverlay *overlay)
{
    EmmcInfo info;
    uint32_t size = already_loaded(overlay);
    if (size == 0 && emmc_start(&info)) {
        size = load_from(info.layout.slot_1, overlay);
        size = size != 0 ? size : load_from(info.layout.slot_2, overlay);
    }
    if (size == 0) {
        return false;
    }
    for (uint32_t index = size; index < overlay->capacity; index++) {
        overlay->base[index] = 0;
    }
    __asm__ volatile("dsb\n\tisb" ::: "memory");
    return true;
}

static const void *load_window(uint32_t image_offset, uint32_t magic)
{
    uint8_t *base = (uint8_t *)(uintptr_t)OVERLAY_WINDOW_BASE;
    BoardOverlay overlay = {image_offset, magic, base, OVERLAY_WINDOW_BYTES};
    board_audio_shut_down();
    return board_overlay_load(&overlay) ? base + OVERLAY_HEADER_BYTES : NULL;
}

const void *hal_system_load_rescan_code(void)
{
    return load_window(RESCAN_OVERLAY_IMAGE_OFFSET, RESCAN_OVERLAY_MAGIC);
}

const void *board_usb_overlay_load(void)
{
    return load_window(USB_OVERLAY_IMAGE_OFFSET, USB_OVERLAY_MAGIC);
}
