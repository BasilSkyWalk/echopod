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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_card_root_lists_its_folders),
        TEST_CASE(a_folder_lists_its_files_with_sizes),
        TEST_CASE(a_missing_folder_does_not_open),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
