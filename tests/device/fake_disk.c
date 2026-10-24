#include "fake_disk.h"

#include <string.h>

void fake_disk_init(FakeDisk *disk, uint32_t sector_count)
{
    memset(disk, 0, sizeof *disk);
    disk->sector_count = sector_count;
}

static int find(const FakeDisk *disk, uint32_t lba)
{
    for (int index = 0; index < disk->used; index++) {
        if (disk->lbas[index] == lba) {
            return index;
        }
    }
    return -1;
}

uint8_t *fake_disk_sector(FakeDisk *disk, uint32_t lba)
{
    int index = find(disk, lba);
    if (index < 0 && disk->used < FAKE_DISK_SECTORS) {
        index = disk->used++;
        disk->lbas[index] = lba;
    }
    return index < 0 ? NULL : disk->sectors[index];
}

static bool read_sectors(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    FakeDisk *disk = context;
    uint8_t *out = buffer;
    for (uint32_t offset = 0; offset < count; offset++) {
        if (lba + offset >= disk->sector_count) {
            return false;
        }
        int index = find(disk, lba + offset);
        disk->reads++;
        if (index < 0) {
            memset(out + offset * 512, 0, 512);
        } else {
            memcpy(out + offset * 512, disk->sectors[index], 512);
        }
    }
    return true;
}

BlockDevice fake_disk_device(FakeDisk *disk)
{
    return (BlockDevice){read_sectors, disk, NULL};
}
