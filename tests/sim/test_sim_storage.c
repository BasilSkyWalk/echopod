#include <stdint.h>
#include <string.h>

#include "check.h"
#include "core/volume_path.h"
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

static void a_file_grows_and_keeps_its_bytes(void)
{
    static const char PATH[] = "/.echo/grown.bin";
    static const uint8_t FRESH[4] = {9, 8, 7, 6};
    static uint8_t zeros[4096];
    uint8_t seen[sizeof FRESH];
    HalFile file;
    sim_storage_configure(ECHO_FIXTURE_DIR, ECHO_TEST_OUTPUT_DIR);
    CHECK(hal_file_write(PATH, zeros, sizeof zeros));
    CHECK(hal_file_write_at(PATH, 100, FRESH, sizeof FRESH));
    CHECK_EQUAL(HAL_RESERVE_EXISTS, hal_file_grow(PATH, sizeof zeros));
    CHECK_EQUAL(HAL_RESERVE_CREATED, hal_file_grow(PATH, 3 * sizeof zeros));
    CHECK_EQUAL(HAL_RESERVE_FAILED, hal_file_grow("/.echo/never made.bin", 10));
    CHECK(hal_file_open(&file, PATH));
    CHECK_EQUAL(3 * sizeof zeros, file.size);
    CHECK(hal_file_read(file, 100, seen, sizeof seen));
    hal_file_close(&file);
    CHECK(memcmp(seen, FRESH, sizeof FRESH) == 0);
}

static void use_both_drives(bool shown)
{
    sim_storage_configure(ECHO_FIXTURE_DIR "/scan_music", ECHO_TEST_OUTPUT_DIR);
    sim_storage_configure_internal(ECHO_FIXTURE_DIR "/tags");
    hal_storage_show_internal(shown);
}

static void shown_internal_storage_is_the_last_folder_of_the_card_root(void)
{
    use_both_drives(true);
    HalDir dir;
    static HalDirEntry entry;
    static char last[HAL_NAME_CAPACITY];
    int internal_folders = 0;
    CHECK(hal_dir_open(&dir, "/"));
    while (hal_dir_next(&dir, &entry)) {
        internal_folders += strcmp(entry.name, VOLUME_INTERNAL_NAME) == 0;
        strcpy(last, entry.name);
    }
    hal_dir_close(&dir);
    CHECK_EQUAL(1, internal_folders);
    CHECK(strcmp(last, VOLUME_INTERNAL_NAME) == 0);
    Found internal = find("/", VOLUME_INTERNAL_NAME);
    CHECK(internal.entry.directory);
    CHECK_EQUAL(0u, internal.entry.size);
    CHECK_EQUAL(0u, internal.entry.modified);
    CHECK(!find("/Artist A", VOLUME_INTERNAL_NAME).found);
    hal_storage_show_internal(false);
}

static void hidden_internal_storage_is_not_listed_but_still_opens(void)
{
    use_both_drives(false);
    CHECK(!find("/", VOLUME_INTERNAL_NAME).found);
    CHECK(find("/:internal", "tagged.flac").found);
}

static void internal_paths_read_the_internal_drive(void)
{
    use_both_drives(true);
    CHECK(find("/:internal/", "untagged.flac").found);
    CHECK(!find("/:internal", "Root Song.mp3").found);
    HalFile file;
    uint8_t magic[4];
    CHECK(hal_file_open(&file, "/:internal/tagged.flac"));
    CHECK(hal_file_read(file, 0, magic, sizeof magic));
    hal_file_close(&file);
    CHECK(memcmp(magic, "fLaC", 4) == 0);
    CHECK(!hal_file_open(&file, "/tagged.flac"));
    CHECK(hal_file_open(&file, "/Root Song.mp3"));
    hal_file_close(&file);
    hal_storage_show_internal(false);
}

static void the_player_never_writes_to_internal_storage(void)
{
    static const uint8_t BYTES[4] = {1, 2, 3, 4};
    use_both_drives(true);
    CHECK(!hal_file_write("/:internal/new.bin", BYTES, sizeof BYTES));
    CHECK(!hal_file_write_at("/:internal/tagged.flac", 0, BYTES, sizeof BYTES));
    CHECK_EQUAL(HAL_RESERVE_FAILED, hal_file_reserve("/:internal/.echo/x.bin", 4096));
    hal_storage_show_internal(false);
}

static void without_internal_storage_nothing_is_listed(void)
{
    sim_storage_configure(ECHO_FIXTURE_DIR "/scan_music", NULL);
    hal_storage_show_internal(true);
    HalDir dir;
    CHECK(!find("/", VOLUME_INTERNAL_NAME).found);
    CHECK(!hal_dir_open(&dir, "/:internal"));
    hal_storage_show_internal(false);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_card_root_lists_its_folders),
        TEST_CASE(a_folder_lists_its_files_with_sizes),
        TEST_CASE(a_missing_folder_does_not_open),
        TEST_CASE(a_read_after_an_overwrite_sees_the_new_bytes),
        TEST_CASE(a_file_grows_and_keeps_its_bytes),
        TEST_CASE(shown_internal_storage_is_the_last_folder_of_the_card_root),
        TEST_CASE(hidden_internal_storage_is_not_listed_but_still_opens),
        TEST_CASE(internal_paths_read_the_internal_drive),
        TEST_CASE(the_player_never_writes_to_internal_storage),
        TEST_CASE(without_internal_storage_nothing_is_listed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
