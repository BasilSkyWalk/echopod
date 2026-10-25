#include <stdint.h>
#include <string.h>

#include "check.h"
#include "hal/storage.h"
#include "sim_storage.h"

typedef struct Found {
    bool found;
    HalDirEntry entry;
} Found;

static Found find(const char *path, const char *name)
{
    Found result = {.found = false};
    HalDir dir;
    CHECK(hal_dir_open(&dir, path));
    static HalDirEntry entry;
    while (hal_dir_next(&dir, &entry)) {
        if (strcmp(entry.name, name) == 0) {
            result = (Found){.found = true, .entry = entry};
        }
    }
    hal_dir_close(&dir);
    return result;
}

static void the_card_root_lists_its_folders(void)
{
    sim_storage_configure(ECHO_FIXTURE_DIR, NULL);
    Found flac = find("/", "flac");
    CHECK(flac.found);
    CHECK(flac.entry.directory);
}

static void a_folder_lists_its_files_with_sizes(void)
{
    sim_storage_configure(ECHO_FIXTURE_DIR, NULL);
    Found file = find("/flac", "big_blocks_16.flac");
    CHECK(file.found);
    CHECK(!file.entry.directory);
    CHECK(file.entry.size > 0);
    CHECK(!find("/flac", ".").found);
    CHECK(!find("/flac", "..").found);
}

static void a_missing_folder_does_not_open(void)
{
    sim_storage_configure(ECHO_FIXTURE_DIR, NULL);
    HalDir dir;
    CHECK(!hal_dir_open(&dir, "/no such folder"));
    CHECK(!hal_dir_open(&dir, "/small_library.db"));
}

static void a_read_after_an_overwrite_sees_the_new_bytes(void)
{
    static const char PATH[] = "/.echo/overwritten.bin";
    static const uint8_t FRESH[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    static uint8_t zeros[8192];
    uint8_t seen[sizeof FRESH];
    HalFile file;
    sim_storage_configure(ECHO_FIXTURE_DIR, ECHO_TEST_OUTPUT_DIR);
    CHECK(hal_file_write(PATH, zeros, sizeof zeros));
    CHECK(hal_file_open(&file, PATH));
    CHECK(hal_file_read(file, 4096, seen, sizeof seen));
    CHECK(hal_file_write_at(PATH, 4100, FRESH, sizeof FRESH));
    CHECK(hal_file_read(file, 4100, seen, sizeof seen));
    hal_file_close(&file);
    CHECK(memcmp(seen, FRESH, sizeof FRESH) == 0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_card_root_lists_its_folders),
        TEST_CASE(a_folder_lists_its_files_with_sizes),
        TEST_CASE(a_missing_folder_does_not_open),
        TEST_CASE(a_read_after_an_overwrite_sees_the_new_bytes),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
