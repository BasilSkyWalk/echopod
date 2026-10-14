#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/image_crc.h"
#include "device/updater.h"

enum {
    IMAGE_SIZE = 0x500004,
    FW_SIZE = 0x44C000,
    FW_END = 0x500000,
    FW_SECTORS = FW_END / 512,
    CHUNK_SECTORS = 16,
    SLOT_SECTORS = FW_SECTORS + 16,
    SLOT_1 = 16,
    SLOT_2 = SLOT_1 + SLOT_SECTORS,
    STORAGE_SECTORS = SLOT_2 + SLOT_SECTORS,
    PAYLOAD_END = 0x5000,
    CODE_OFFSET = 0xD68,
    CODE_SIZE = PAYLOAD_END - CODE_OFFSET - 8,
    LOG_CAPACITY = 4096
};

#define RESET_HANDLER 0x03050101u

typedef struct WriteRecord {
    uint32_t lba;
    uint32_t count;
    uint8_t first_byte;
} WriteRecord;

typedef struct FakeStorage {
    uint8_t *sectors;
    WriteRecord log[LOG_CAPACITY];
    int writes;
    uint32_t fail_lba;
    uint32_t corrupt_lba;
} FakeStorage;

typedef struct FakeFile {
    uint8_t *bytes;
    uint32_t size;
} FakeFile;

static FakeStorage storage;
static FakeFile file;
static int progress_reports;
static uint32_t last_done;
static bool progress_went_backwards;

static void put_le32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void seal(uint8_t *image)
{
    memcpy(image + FW_SIZE, image, 512);
    put_le32(image + FW_END, image_crc_update(0, image, FW_END));
}

static uint8_t *valid_image(uint8_t seed)
{
    uint8_t *image = calloc(IMAGE_SIZE, 1);
    for (uint32_t index = 0; index < 0x1F4; index++) {
        image[index] = (uint8_t)(index + seed);
    }
    put_le32(image + 0x1F4, FW_SIZE);
    memcpy(image + 0x1F8, "RKnanoFW", 8);
    put_le32(image + 0x80, PAYLOAD_END);
    for (uint32_t index = CODE_OFFSET; index < PAYLOAD_END; index++) {
        image[index] = (uint8_t)(index * 13 + seed);
    }
    put_le32(image + 0x200, 0x03050000);
    put_le32(image + 0x204, 91);
    uint32_t rows[3][4] = {{0x03050B68, 0x03050000, CODE_SIZE, 0x03050B68 + CODE_SIZE},
                           {0x03004000, 8, 0x03004008, 0x100},
                           {0x03078CD0, 0x03079DCC, 4, 0}};
    for (int row = 0; row < 3; row++) {
        for (int word = 0; word < 4; word++) {
            put_le32(image + 0x208 + 16 * row + 4 * word, rows[row][word]);
        }
    }
    put_le32(image + CODE_OFFSET, 0x03004000);
    put_le32(image + CODE_OFFSET + 4, RESET_HANDLER);
    put_le32(image + CODE_OFFSET + CODE_SIZE, 0x03004000);
    put_le32(image + CODE_OFFSET + CODE_SIZE + 4, RESET_HANDLER);
    seal(image);
    return image;
}

static uint8_t *broken_image(uint32_t offset, uint32_t value)
{
    uint8_t *image = valid_image(1);
    put_le32(image + offset, value);
    seal(image);
    return image;
}

static bool storage_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    (void)context;
    if (lba + count > STORAGE_SECTORS) {
        return false;
    }
    memcpy(buffer, storage.sectors + (size_t)lba * 512, (size_t)count * 512);
    return true;
}

static bool storage_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    (void)context;
    if (lba + count > STORAGE_SECTORS ||
        (storage.fail_lba >= lba && storage.fail_lba < lba + count)) {
        return false;
    }
    if (storage.writes < LOG_CAPACITY) {
        storage.log[storage.writes++] = (WriteRecord){lba, count, ((const uint8_t *)buffer)[0]};
    }
    memcpy(storage.sectors + (size_t)lba * 512, buffer, (size_t)count * 512);
    if (storage.corrupt_lba >= lba && storage.corrupt_lba < lba + count) {
        storage.sectors[(size_t)storage.corrupt_lba * 512] ^= 0xFF;
    }
    return true;
}

static bool file_read(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if (offset > file.size || length > file.size - offset) {
        return false;
    }
    memcpy(buffer, file.bytes + offset, length);
    return true;
}

static void report(void *context, uint32_t done, uint32_t total)
{
    (void)context;
    (void)total;
    progress_went_backwards = progress_went_backwards || done < last_done;
    last_done = done;
    progress_reports++;
}

static UpdateStorage target(void)
{
    return (UpdateStorage){storage_read, storage_write, NULL, SLOT_1, SLOT_2, SLOT_SECTORS};
}

static UpdateFile source(void)
{
    return (UpdateFile){file_read, file.size, NULL};
}

static UpdateProgress progress(void)
{
    return (UpdateProgress){report, NULL};
}

static void reset(uint8_t *image, uint32_t size)
{
    free(storage.sectors);
    free(file.bytes);
    memset(&storage, 0, sizeof storage);
    storage.sectors = calloc(STORAGE_SECTORS, 512);
    storage.fail_lba = UINT32_MAX;
    storage.corrupt_lba = UINT32_MAX;
    file = (FakeFile){image, size};
    progress_reports = 0;
    last_done = 0;
    progress_went_backwards = false;
}

static bool slot_holds(uint32_t slot, const uint8_t *image)
{
    return memcmp(storage.sectors + (size_t)slot * 512, image, FW_END) == 0;
}

static void the_crc_matches_the_image_builder(void)
{
    static uint8_t pattern[256 * 40];
    for (uint32_t index = 0; index < sizeof pattern; index++) {
        pattern[index] = (uint8_t)index;
    }
    CHECK_EQUAL(0x889A9615u, image_crc_update(0, (const uint8_t *)"123456789", 9));
    CHECK_EQUAL(0x749C3565u, image_crc_update(0, pattern, sizeof pattern));
    uint32_t split =
        image_crc_update(image_crc_update(0, pattern, 1000), pattern + 1000, sizeof pattern - 1000);
    CHECK_EQUAL(0x749C3565u, split);
}

static void a_valid_image_passes_the_check(void)
{
    reset(valid_image(1), IMAGE_SIZE);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK_EQUAL(UPDATE_VALID, updater_check(&update, &storage_target));
}

static void check_invalid(uint8_t *image, uint32_t size)
{
    reset(image, size);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK_EQUAL(UPDATE_INVALID, updater_check(&update, &storage_target));
}

static void images_with_the_wrong_shape_or_damage_are_invalid(void)
{
    check_invalid(valid_image(1), IMAGE_SIZE - 4);
    uint8_t *image = valid_image(1);
    image[0x1F8] = 'X';
    seal(image);
    check_invalid(image, IMAGE_SIZE);
    image = valid_image(1);
    image[FW_SIZE + 3] ^= 1;
    check_invalid(image, IMAGE_SIZE);
    image = valid_image(1);
    image[0x2000] ^= 1;
    check_invalid(image, IMAGE_SIZE);
    image = valid_image(1);
    put_le32(image + 0x1F4, 0x01FC4000);
    seal(image);
    check_invalid(image, IMAGE_SIZE);
}

static void an_image_that_does_not_fit_a_slot_is_invalid(void)
{
    reset(valid_image(1), IMAGE_SIZE);
    UpdateStorage small = target();
    small.slot_sectors = FW_SECTORS - 1;
    UpdateFile update = source();
    CHECK_EQUAL(UPDATE_INVALID, updater_check(&update, &small));
}

static void the_installed_image_is_recognised(void)
{
    uint8_t *image = valid_image(1);
    reset(image, IMAGE_SIZE);
    memcpy(storage.sectors + (size_t)SLOT_1 * 512, image, FW_END);
    memcpy(storage.sectors + (size_t)SLOT_2 * 512, image, FW_END);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK_EQUAL(UPDATE_ALREADY_INSTALLED, updater_check(&update, &storage_target));
}

static void installing_fills_both_slots_with_the_image(void)
{
    uint8_t *image = valid_image(2);
    reset(image, IMAGE_SIZE);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK(updater_install(&update, &storage_target, progress()));
    CHECK(slot_holds(SLOT_2, image));
    CHECK(slot_holds(SLOT_1, image));
    CHECK(progress_reports > 0);
    CHECK(!progress_went_backwards);
}

static int first_write_into(uint32_t slot)
{
    for (int index = 0; index < storage.writes; index++) {
        if (storage.log[index].lba >= slot && storage.log[index].lba < slot + SLOT_SECTORS) {
            return index;
        }
    }
    return -1;
}

static int last_write_into(uint32_t slot)
{
    for (int index = storage.writes - 1; index >= 0; index--) {
        if (storage.log[index].lba >= slot && storage.log[index].lba < slot + SLOT_SECTORS) {
            return index;
        }
    }
    return -1;
}

static void each_slot_is_invalidated_first_and_completed_last(void)
{
    uint8_t *image = valid_image(3);
    reset(image, IMAGE_SIZE);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK(updater_install(&update, &storage_target, progress()));
    for (int pass = 0; pass < 2; pass++) {
        uint32_t slot = pass == 0 ? SLOT_2 : SLOT_1;
        const WriteRecord *first = &storage.log[first_write_into(slot)];
        const WriteRecord *last = &storage.log[last_write_into(slot)];
        CHECK_EQUAL(slot, first->lba);
        CHECK_EQUAL(CHUNK_SECTORS, first->count);
        CHECK_EQUAL(0xFF, first->first_byte);
        CHECK_EQUAL(slot, last->lba);
        CHECK_EQUAL(image[0], last->first_byte);
    }
    CHECK(last_write_into(SLOT_2) < first_write_into(SLOT_1));
}

static void chunks_that_already_match_are_not_rewritten(void)
{
    uint8_t *image = valid_image(4);
    reset(image, IMAGE_SIZE);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK(updater_install(&update, &storage_target, progress()));
    int payload_chunks = (PAYLOAD_END + CHUNK_SECTORS * 512 - 1) / (CHUNK_SECTORS * 512);
    int expected_per_slot = 1 + payload_chunks + 1;
    CHECK_EQUAL(2 * expected_per_slot, storage.writes);
}

static void a_write_failure_in_slot_2_leaves_slot_1_untouched(void)
{
    uint8_t *image = valid_image(5);
    reset(image, IMAGE_SIZE);
    memset(storage.sectors + (size_t)SLOT_1 * 512, 0xA5, 512);
    storage.fail_lba = SLOT_2 + 2 * CHUNK_SECTORS;
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK(!updater_install(&update, &storage_target, progress()));
    CHECK_EQUAL(0xA5, storage.sectors[(size_t)SLOT_1 * 512]);
    CHECK_EQUAL(-1, first_write_into(SLOT_1));
}

static void a_block_that_reads_back_wrong_fails_the_install(void)
{
    uint8_t *image = valid_image(6);
    reset(image, IMAGE_SIZE);
    storage.corrupt_lba = SLOT_2 + CHUNK_SECTORS;
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK(!updater_install(&update, &storage_target, progress()));
    CHECK_EQUAL(-1, first_write_into(SLOT_1));
}

static void images_that_could_not_boot_are_invalid(void)
{
    check_invalid(broken_image(0x200, 0x03060000), IMAGE_SIZE);
    check_invalid(broken_image(0x204, 3), IMAGE_SIZE);
    check_invalid(broken_image(0x208, 0x03050000), IMAGE_SIZE);
    check_invalid(broken_image(0x208 + 12, 0), IMAGE_SIZE);
    check_invalid(broken_image(0x218, 0x03000000), IMAGE_SIZE);
    check_invalid(broken_image(0x218 + 4, 0), IMAGE_SIZE);
    check_invalid(broken_image(0x218 + 4, 184), IMAGE_SIZE);
    check_invalid(broken_image(0x228, 0x03078CD4), IMAGE_SIZE);
    check_invalid(broken_image(0x238, 0x03078CD4), IMAGE_SIZE);
    check_invalid(broken_image(CODE_OFFSET + CODE_SIZE, 0), IMAGE_SIZE);
    check_invalid(broken_image(CODE_OFFSET + CODE_SIZE + 4, 0x03050100), IMAGE_SIZE);
    check_invalid(broken_image(CODE_OFFSET + CODE_SIZE + 4, 0x03004001), IMAGE_SIZE);
    check_invalid(broken_image(CODE_OFFSET + 4, 0x03050201), IMAGE_SIZE);
}

static void a_half_installed_image_is_installed_again(void)
{
    uint8_t *image = valid_image(1);
    reset(image, IMAGE_SIZE);
    memcpy(storage.sectors + (size_t)SLOT_1 * 512, image, FW_END);
    memset(storage.sectors + (size_t)SLOT_2 * 512, 0xFF, CHUNK_SECTORS * 512);
    UpdateStorage storage_target = target();
    UpdateFile update = source();
    CHECK_EQUAL(UPDATE_VALID, updater_check(&update, &storage_target));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_crc_matches_the_image_builder),
        TEST_CASE(a_valid_image_passes_the_check),
        TEST_CASE(images_with_the_wrong_shape_or_damage_are_invalid),
        TEST_CASE(an_image_that_does_not_fit_a_slot_is_invalid),
        TEST_CASE(images_that_could_not_boot_are_invalid),
        TEST_CASE(the_installed_image_is_recognised),
        TEST_CASE(a_half_installed_image_is_installed_again),
        TEST_CASE(installing_fills_both_slots_with_the_image),
        TEST_CASE(each_slot_is_invalidated_first_and_completed_last),
        TEST_CASE(chunks_that_already_match_are_not_rewritten),
        TEST_CASE(a_write_failure_in_slot_2_leaves_slot_1_untouched),
        TEST_CASE(a_block_that_reads_back_wrong_fails_the_install),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    free(storage.sectors);
    free(file.bytes);
    return result;
}
