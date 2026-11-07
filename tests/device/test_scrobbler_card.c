#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/fat.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "listen_history.h"
#include "scrobbler_log.h"
#include "sparse_card.h"

enum {
    RING_SLOTS = 16,
    RING_BYTES = RING_SLOTS * HISTORY_SLOT_BYTES,
    NEWEST = 11,
    FILE_LIMIT = 8192,
    OPEN_LIMIT = 4,
    HEADER_LINES = 3
};

static SparseCard sparse;
static FatVolume volume;
static FatFile open_files[OPEN_LIMIT];
static bool open_used[OPEN_LIMIT];
static ListenHistory history;
static uint8_t ring[RING_BYTES];
static char all_log[FILE_LIMIT];
static uint32_t all_length;

bool hal_file_open(HalFile *file, const char *path)
{
    *file = (HalFile){.handle = -1, .size = 0};
    for (int slot = 0; slot < OPEN_LIMIT; slot++) {
        if (!open_used[slot]) {
            if (!fat_open(&volume, path, &open_files[slot])) {
                return false;
            }
            open_used[slot] = true;
            *file = (HalFile){.handle = slot, .size = open_files[slot].size};
            return true;
        }
    }
    return false;
}

bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length)
{
    return file.handle >= 0 && open_used[file.handle] &&
           fat_read(&volume, &open_files[file.handle], offset, buffer, length);
}

void hal_file_close(HalFile *file)
{
    if (file->handle >= 0) {
        open_used[file->handle] = false;
    }
    file->handle = -1;
}

bool hal_file_write_at(const char *path, uint32_t offset, const void *bytes, uint32_t length)
{
    FatFile file;
    return fat_open(&volume, path, &file) && fat_write(&volume, &file, offset, bytes, length);
}

static HalReserveResult result_of(FatReserveResult result)
{
    static const HalReserveResult RESULTS[] = {HAL_RESERVE_EXISTS, HAL_RESERVE_CREATED,
                                               HAL_RESERVE_NO_SPACE, HAL_RESERVE_FAILED};
    return RESULTS[result];
}

HalReserveResult hal_file_reserve(const char *path, uint32_t size)
{
    return result_of(fat_reserve(&volume, path, size));
}

HalReserveResult hal_file_grow(const char *path, uint32_t size)
{
    return result_of(fat_grow(&volume, path, size));
}

void hal_system_keep_alive(void)
{
}

static uint32_t read_fixture(const char *name, void *buffer, uint32_t capacity)
{
    char path[512];
    snprintf(path, sizeof path, "%s/history/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    size_t length = fread(buffer, 1, capacity, file);
    fclose(file);
    return (uint32_t)length;
}

static void remount(void)
{
    CHECK(fat_mount(&volume, sparse_card_device(&sparse)));
}

static void mount_with_history(const char *name)
{
    CHECK(sparse_card_load(&sparse, name));
    remount();
    CHECK_EQUAL(RING_BYTES, read_fixture("history.bin", ring, sizeof ring));
    all_length = read_fixture("all.log", all_log, sizeof all_log);
    CHECK_EQUAL(FAT_RESERVE_CREATED, fat_reserve(&volume, LISTEN_HISTORY_PATH, RING_BYTES));
    CHECK(hal_file_write_at(LISTEN_HISTORY_PATH, 0, ring, RING_BYTES));
    history = (ListenHistory){.located = true, .slots = RING_SLOTS, .sequence = NEWEST + 1};
}

static bool file_is(const char *path, const char *text, uint32_t length)
{
    static char bytes[FILE_LIMIT];
    FatFile file;
    remount();
    return fat_open(&volume, path, &file) && file.size == length && length <= sizeof bytes &&
           fat_read(&volume, &file, 0, bytes, length) && memcmp(bytes, text, length) == 0;
}

static uint32_t cluster_bytes(void)
{
    return (uint32_t)FAT_SECTOR_SIZE << volume.cluster_shift;
}

static const char *after_header(const char *log)
{
    for (int line = 0; line < HEADER_LINES; line++) {
        log = strchr(log, '\n') + 1;
    }
    return log;
}

static void check_card(const char *name)
{
    mount_with_history(name);
    CHECK_EQUAL(6, scrobbler_log_write(&history, "test"));
    CHECK(file_is(SCROBBLER_LOG_PATH, all_log, all_length));
    CHECK(file_is(SCROBBLER_MARK_PATH, "0000000011\n", SCROBBLER_MARK_BYTES));
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    CHECK(file_is(SCROBBLER_LOG_PATH, all_log, all_length));
    sparse_card_free(&sparse);
}

static void check_append_across_a_cluster(const char *name)
{
    mount_with_history(name);
    uint32_t kept = cluster_bytes() - 20;
    char *expected = malloc(kept + all_length);
    for (uint32_t index = 0; index < kept; index++) {
        expected[index] = index % 2 == 0 ? 'x' : '\n';
    }
    CHECK_EQUAL(FAT_RESERVE_CREATED, fat_reserve(&volume, SCROBBLER_LOG_PATH, kept));
    CHECK(hal_file_write_at(SCROBBLER_LOG_PATH, 0, expected, kept));
    const char *lines = after_header(all_log);
    uint32_t lines_length = all_length - (uint32_t)(lines - all_log);
    memcpy(expected + kept, lines, lines_length);
    CHECK_EQUAL(6, scrobbler_log_write(&history, "test"));
    CHECK(file_is(SCROBBLER_LOG_PATH, expected, kept + lines_length));
    CHECK(file_is(SCROBBLER_MARK_PATH, "0000000011\n", SCROBBLER_MARK_BYTES));
    FatFile notes;
    CHECK(fat_open(&volume, "/notes.txt", &notes));
    CHECK_EQUAL(700, notes.size);
    free(expected);
    sparse_card_free(&sparse);
}

static void the_log_is_made_on_a_fat32_card(void)
{
    check_card("fat32_mbr");
}

static void the_log_is_made_on_a_fat16_card(void)
{
    check_card("fat16_superfloppy");
}

static void the_log_is_made_on_an_exfat_card(void)
{
    check_card("exfat_mbr");
}

static void a_log_grows_past_its_last_cluster(void)
{
    check_append_across_a_cluster("fat32_mbr");
    check_append_across_a_cluster("fat16_superfloppy");
    check_append_across_a_cluster("exfat_mbr");
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_log_is_made_on_a_fat32_card),
        TEST_CASE(the_log_is_made_on_a_fat16_card),
        TEST_CASE(the_log_is_made_on_an_exfat_card),
        TEST_CASE(a_log_grows_past_its_last_cluster),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
