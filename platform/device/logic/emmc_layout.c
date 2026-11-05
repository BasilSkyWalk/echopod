#include "device/emmc_layout.h"

#define IDB_MAGIC 0xFCDC8C3Bu

enum {
    SECTOR_SIZE = 512,
    IDB_FIRST = 0x40,
    IDB_STRIDE = 0x400,
    IDB_COPIES = 5,
    IDB_SYSTEM_MIB = 0x202,
    IDB_TAG = 0x20A,
    FIRMWARE_START = 0x2000,
    SECTORS_PER_MIB = 0x800,
    SCAN_LIMIT = FIRMWARE_START + 0x100000,
    SIGNATURE = 0x1FE,
    PARTITION_ENTRY = 0x1BE,
    IMAGE_SIZE_AT = 0x1F4,
    IMAGE_MAGIC_AT = 0x1F8,
    MAX_IMAGE_BYTES = 0x1000000,
    SECOND_SLOT_MARGIN = 0x4000,
    SECOND_SLOT_TRIES = 20
};

static uint32_t le16(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t le32(const uint8_t *bytes)
{
    return le16(bytes) | le16(bytes + 2) << 16;
}

static bool matches(const uint8_t *bytes, const char *text, int length)
{
    for (int index = 0; index < length; index++) {
        if (bytes[index] != (uint8_t)text[index]) {
            return false;
        }
    }
    return true;
}

static bool read_system_mib(const BlockDevice *device, uint32_t *system_mib)
{
    static uint32_t words[2 * SECTOR_SIZE / 4];
    const uint8_t *bytes = (const uint8_t *)words;
    for (uint32_t copy = 0; copy < IDB_COPIES; copy++) {
        uint32_t lba = IDB_FIRST + copy * IDB_STRIDE;
        if (device->read(device->context, lba, 2, words) && le32(bytes) == IDB_MAGIC &&
            matches(bytes + IDB_TAG, "NANO", 4) && le16(bytes + IDB_SYSTEM_MIB) != 0) {
            *system_mib = le16(bytes + IDB_SYSTEM_MIB);
            return true;
        }
    }
    return false;
}

static bool is_volume_start(const uint8_t *sector)
{
    if (le16(sector + SIGNATURE) != 0xAA55) {
        return false;
    }
    const uint8_t *entry = sector + PARTITION_ENTRY;
    bool exfat = matches(sector + 3, "EXFAT   ", 8);
    bool fat = (sector[0] == 0xEB || sector[0] == 0xE9) && le16(sector + 11) == SECTOR_SIZE;
    bool mbr = (entry[0] == 0x00 || entry[0] == 0x80) && entry[4] != 0 && le32(entry + 8) != 0;
    return exfat || fat || mbr;
}

static bool find_volume(const BlockDevice *device, uint32_t first, uint32_t total, uint32_t *start)
{
    static uint32_t words[SECTOR_SIZE / 4];
    for (uint32_t lba = first; lba < SCAN_LIMIT && lba < total; lba += SECTORS_PER_MIB) {
        if (device->read(device->context, lba, 1, words) &&
            is_volume_start((const uint8_t *)words)) {
            *start = lba;
            return true;
        }
    }
    return false;
}

static bool inside(uint32_t start, uint32_t length, uint32_t lba, uint32_t count)
{
    return lba >= start && lba - start < length && count <= length - (lba - start);
}

bool emmc_layout_allows_write(const EmmcLayout *layout, uint32_t lba, uint32_t count)
{
    return count > 0 && (inside(layout->volume_start, layout->volume_sectors, lba, count) ||
                         inside(layout->slot_1, layout->slot_sectors, lba, count) ||
                         inside(layout->slot_2, layout->slot_sectors, lba, count));
}

static bool read_image_header(const BlockDevice *device, uint32_t lba, uint32_t *image_bytes,
                              uint32_t *sum)
{
    static uint32_t words[SECTOR_SIZE / 4];
    const uint8_t *bytes = (const uint8_t *)words;
    if (!device->read(device->context, lba, 1, words) ||
        !matches(bytes + IMAGE_MAGIC_AT, "RKnanoFW", 8)) {
        return false;
    }
    *image_bytes = le32(bytes + IMAGE_SIZE_AT);
    *sum = 0;
    for (uint32_t index = 0; index < SECTOR_SIZE / 4; index++) {
        *sum = (*sum << 5 | *sum >> 27) ^ words[index];
    }
    return true;
}

static void find_second_slot(EmmcLayout *layout, const BlockDevice *device)
{
    uint32_t ignored = 0;
    uint32_t image_bytes = 0;
    uint32_t slot_1_sum = 0;
    uint32_t sum = 0;
    if (read_image_header(device, layout->slot_2, &ignored, &sum) ||
        !read_image_header(device, layout->slot_1, &image_bytes, &slot_1_sum) || image_bytes == 0 ||
        image_bytes > MAX_IMAGE_BYTES) {
        return;
    }
    uint32_t first = ((image_bytes + SECOND_SLOT_MARGIN) / SECTOR_SIZE + SECTORS_PER_MIB - 1) /
                     SECTORS_PER_MIB * SECTORS_PER_MIB;
    uint32_t found = 0;
    for (uint32_t attempt = 0; attempt < SECOND_SLOT_TRIES; attempt++) {
        uint32_t lba = FIRMWARE_START + first + attempt * SECTORS_PER_MIB;
        if (lba + SECTORS_PER_MIB > layout->firmware_end) {
            break;
        }
        if (read_image_header(device, lba, &ignored, &sum)) {
            found = found == 0 || sum == slot_1_sum ? lba : found;
            if (sum == slot_1_sum) {
                break;
            }
        }
    }
    if (found == 0) {
        return;
    }
    layout->slot_2 = found;
    uint32_t room = layout->firmware_end - found;
    uint32_t gap = found - layout->slot_1;
    uint32_t room_for_both = room < gap ? room : gap;
    if (room_for_both < layout->slot_sectors) {
        layout->slot_sectors = room_for_both;
    }
}

bool emmc_layout_read(EmmcLayout *layout, const BlockDevice *device, uint32_t total_sectors)
{
    uint32_t system_mib = 0;
    if (!read_system_mib(device, &system_mib)) {
        return false;
    }
    uint32_t firmware_sectors = system_mib * SECTORS_PER_MIB;
    *layout = (EmmcLayout){
        .slot_1 = FIRMWARE_START,
        .slot_2 = FIRMWARE_START + firmware_sectors / 2,
        .slot_sectors = firmware_sectors / 2,
        .firmware_end = FIRMWARE_START + firmware_sectors,
    };
    find_second_slot(layout, device);
    if (!find_volume(device, layout->firmware_end, total_sectors, &layout->volume_start)) {
        return false;
    }
    layout->volume_sectors = total_sectors - layout->volume_start;
    return true;
}
