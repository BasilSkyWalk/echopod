#include <string.h>

#include "check.h"
#include "device/emmc_layout.h"
#include "fake_disk.h"

enum {
    TOTAL_SECTORS = 15269888,
    RECHORD_SYSTEM_MIB = 0x44
};

static void write_idb(FakeDisk *disk, int copy, uint16_t system_mib)
{
    uint32_t lba = 0x40u + (uint32_t)copy * 0x400u;
    uint8_t *first = fake_disk_sector(disk, lba);
    uint8_t *second = fake_disk_sector(disk, lba + 1);
    first[0] = 0x3B;
    first[1] = 0x8C;
    first[2] = 0xDC;
    first[3] = 0xFC;
    second[0x02] = (uint8_t)system_mib;
    second[0x03] = (uint8_t)(system_mib >> 8);
    memcpy(second + 0x0A, "NANO", 4);
}

static void write_fat_boot_sector(FakeDisk *disk, uint32_t lba)
{
    uint8_t *sector = fake_disk_sector(disk, lba);
    sector[0] = 0xEB;
    sector[11] = 0x00;
    sector[12] = 0x02;
    sector[510] = 0x55;
    sector[511] = 0xAA;
}

static void write_save_block(FakeDisk *disk, uint32_t lba)
{
    uint8_t *sector = fake_disk_sector(disk, lba);
    memcpy(sector, "SAVE", 4);
}

static bool read_layout(FakeDisk *disk, EmmcLayout *layout)
{
    BlockDevice device = fake_disk_device(disk);
    return emmc_layout_read(layout, &device, TOTAL_SECTORS);
}

static void the_firmware_area_and_slots_follow_the_idb_system_size(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x2000, layout.slot_1);
    CHECK_EQUAL(0x13000, layout.slot_2);
    CHECK_EQUAL(0x11000, layout.slot_sectors);
    CHECK_EQUAL(0x24000, layout.firmware_end);
}

static void the_volume_right_after_the_firmware_is_found(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x24000, layout.volume_start);
    CHECK_EQUAL(TOTAL_SECTORS - 0x24000, layout.volume_sectors);
}

static void a_volume_moved_past_a_save_block_is_found_by_scanning(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_save_block(&disk, 0x24000);
    write_fat_boot_sector(&disk, 0x88000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x88000, layout.volume_start);
    CHECK_EQUAL(TOTAL_SECTORS - 0x88000, layout.volume_sectors);
}

static void an_mbr_or_exfat_volume_is_also_accepted(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    uint8_t *mbr = fake_disk_sector(&disk, 0x24800);
    mbr[0x1BE] = 0x00;
    mbr[0x1C2] = 0x0C;
    mbr[0x1C6] = 0x3F;
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x24800, layout.volume_start);

    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    uint8_t *exfat = fake_disk_sector(&disk, 0x25000);
    memcpy(exfat + 3, "EXFAT   ", 8);
    exfat[510] = 0x55;
    exfat[511] = 0xAA;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x25000, layout.volume_start);
}

static void the_first_valid_idb_copy_is_used(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 2, 0x20);
    write_idb(&disk, 3, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x2000u + 0x20u * 0x800u);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x2000u + 0x20u * 0x800u, layout.firmware_end);
}

static void an_idb_without_the_nano_tag_is_ignored(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    memcpy(fake_disk_sector(&disk, 0x41) + 0x0A, "NONO", 4);
    write_fat_boot_sector(&disk, 0x24000);
    EmmcLayout layout;
    CHECK(!read_layout(&disk, &layout));
}

static void no_volume_within_the_scan_limit_fails(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x2000u + 0x100000u);
    EmmcLayout layout;
    CHECK(!read_layout(&disk, &layout));
}

static void a_volume_is_never_accepted_inside_the_firmware_area(void)
{
    static FakeDisk disk;
    fake_disk_init(&disk, TOTAL_SECTORS);
    write_idb(&disk, 0, RECHORD_SYSTEM_MIB);
    write_fat_boot_sector(&disk, 0x13000);
    write_fat_boot_sector(&disk, 0x30000);
    EmmcLayout layout;
    CHECK(read_layout(&disk, &layout));
    CHECK_EQUAL(0x30000, layout.volume_start);
}

static EmmcLayout rechord_layout(void)
{
    return (EmmcLayout){.slot_1 = 0x2000,
                        .slot_2 = 0x13000,
                        .slot_sectors = 0x11000,
                        .firmware_end = 0x24000,
                        .volume_start = 0x88000,
                        .volume_sectors = TOTAL_SECTORS - 0x88000};
}

static void writes_inside_the_user_volume_are_allowed(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(emmc_layout_allows_write(&layout, 0x88000, 8));
    CHECK(emmc_layout_allows_write(&layout, TOTAL_SECTORS - 1, 1));
    CHECK(!emmc_layout_allows_write(&layout, TOTAL_SECTORS - 1, 2));
}

static void writes_inside_one_firmware_slot_are_allowed(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(emmc_layout_allows_write(&layout, 0x2000, 16));
    CHECK(emmc_layout_allows_write(&layout, 0x13000 + 0x11000 - 1, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x13000 - 1, 2));
}

static void writes_to_the_idb_the_save_block_or_the_gap_are_refused(void)
{
    EmmcLayout layout = rechord_layout();
    CHECK(!emmc_layout_allows_write(&layout, 0x40, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x1FFF, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x24000, 1));
    CHECK(!emmc_layout_allows_write(&layout, 0x87FFF, 2));
    CHECK(!emmc_layout_allows_write(&layout, 0x88000, 0));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_firmware_area_and_slots_follow_the_idb_system_size),
        TEST_CASE(the_volume_right_after_the_firmware_is_found),
        TEST_CASE(a_volume_moved_past_a_save_block_is_found_by_scanning),
        TEST_CASE(an_mbr_or_exfat_volume_is_also_accepted),
        TEST_CASE(the_first_valid_idb_copy_is_used),
        TEST_CASE(an_idb_without_the_nano_tag_is_ignored),
        TEST_CASE(no_volume_within_the_scan_limit_fails),
        TEST_CASE(a_volume_is_never_accepted_inside_the_firmware_area),
        TEST_CASE(writes_inside_the_user_volume_are_allowed),
        TEST_CASE(writes_inside_one_firmware_slot_are_allowed),
        TEST_CASE(writes_to_the_idb_the_save_block_or_the_gap_are_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
