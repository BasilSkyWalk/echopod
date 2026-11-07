#include <string.h>

#include "check.h"
#include "core/hash.h"
#include "library/music_folders.h"

static const uint32_t ROCK_KEY = 3125253844u;
static const uint32_t INTERNAL_ROCK_KEY = 910752148u;
static const uint32_t ELECTRO_KEY = 491043282u;

static MusicFolders two_folders(void)
{
    return (MusicFolders){.count = 2, .keys = {ROCK_KEY, INTERNAL_ROCK_KEY}};
}

static void a_saved_list_reads_back(void)
{
    uint8_t bytes[MUSIC_FOLDERS_FILE_BYTES];
    MusicFolders folders = two_folders();
    music_folders_encode(&folders, bytes);
    CHECK_EQUAL(0, memcmp("EMMF", bytes, 4));
    CHECK_EQUAL(1, bytes[4]);
    CHECK_EQUAL(2, bytes[5]);
    MusicFolders read = {.count = 0};
    CHECK(music_folders_decode(bytes, sizeof bytes, &read));
    CHECK_EQUAL(2u, read.count);
    CHECK_EQUAL(ROCK_KEY, read.keys[0]);
    CHECK_EQUAL(INTERNAL_ROCK_KEY, read.keys[1]);
}

static void the_file_layout_is_fixed(void)
{
    uint8_t bytes[MUSIC_FOLDERS_FILE_BYTES];
    MusicFolders folders = two_folders();
    music_folders_encode(&folders, bytes);
    CHECK_EQUAL(140, MUSIC_FOLDERS_FILE_BYTES);
    CHECK_EQUAL(0xD4, bytes[8]);
    CHECK_EQUAL(0x00, bytes[16]);
    uint32_t check = hash_update(HASH_START, bytes, 136);
    CHECK_EQUAL((uint8_t)check, bytes[136]);
    CHECK_EQUAL((uint8_t)(check >> 24), bytes[139]);
}

static void a_damaged_or_foreign_file_means_every_folder(void)
{
    uint8_t bytes[MUSIC_FOLDERS_FILE_BYTES];
    MusicFolders folders = two_folders();
    MusicFolders read;
    music_folders_encode(&folders, bytes);
    CHECK(!music_folders_decode(bytes, sizeof bytes - 1, &read));
    CHECK_EQUAL(0u, read.count);
    bytes[9] ^= 1;
    CHECK(!music_folders_decode(bytes, sizeof bytes, &read));
    music_folders_encode(&folders, bytes);
    bytes[0] = 'X';
    CHECK(!music_folders_decode(bytes, sizeof bytes, &read));
    uint8_t zeros[MUSIC_FOLDERS_FILE_BYTES] = {0};
    CHECK(!music_folders_decode(zeros, sizeof zeros, &read));
    CHECK_EQUAL(0u, read.count);
}

static void a_count_beyond_the_limit_is_refused(void)
{
    uint8_t bytes[MUSIC_FOLDERS_FILE_BYTES];
    MusicFolders folders = two_folders();
    music_folders_encode(&folders, bytes);
    bytes[5] = MUSIC_FOLDERS_MAX + 1;
    uint32_t check = hash_update(HASH_START, bytes, 136);
    for (int index = 0; index < 4; index++) {
        bytes[136 + index] = (uint8_t)(check >> (8 * index));
    }
    MusicFolders read;
    CHECK(!music_folders_decode(bytes, sizeof bytes, &read));
}

static void keys_are_the_hash_of_the_library_path(void)
{
    uint32_t key = 0;
    CHECK(music_folders_key("/", "Rock", &key));
    CHECK_EQUAL(ROCK_KEY, key);
    CHECK(music_folders_key("/:internal", "Rock", &key));
    CHECK_EQUAL(INTERNAL_ROCK_KEY, key);
    CHECK(music_folders_key("/:internal/", "Rock", &key));
    CHECK_EQUAL(INTERNAL_ROCK_KEY, key);
    CHECK(music_folders_key("/", "\xC3\x89lectro", &key));
    CHECK_EQUAL(ELECTRO_KEY, key);
}

static void only_top_level_folders_have_keys(void)
{
    uint32_t key = 0;
    CHECK(!music_folders_key("/Rock", "Rock", &key));
    CHECK(!music_folders_key("/Rock/", "Live", &key));
    CHECK(!music_folders_key("/:internal/Rock", "Live", &key));
    CHECK(!music_folders_key("/:internalx", "Rock", &key));
}

static void only_listed_top_level_folders_are_skipped(void)
{
    MusicFolders folders = two_folders();
    CHECK(music_folders_skips(&folders, "/", "Rock"));
    CHECK(music_folders_skips(&folders, "/:internal/", "Rock"));
    CHECK(!music_folders_skips(&folders, "/", "rock"));
    CHECK(!music_folders_skips(&folders, "/", "Jazz"));
    CHECK(!music_folders_skips(&folders, "/Rock/", "Rock"));
    CHECK(!music_folders_skips(NULL, "/", "Rock"));
    MusicFolders none = {.count = 0};
    CHECK(!music_folders_skips(&none, "/", "Rock"));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_saved_list_reads_back),
        TEST_CASE(the_file_layout_is_fixed),
        TEST_CASE(a_damaged_or_foreign_file_means_every_folder),
        TEST_CASE(a_count_beyond_the_limit_is_refused),
        TEST_CASE(keys_are_the_hash_of_the_library_path),
        TEST_CASE(only_top_level_folders_have_keys),
        TEST_CASE(only_listed_top_level_folders_are_skipped),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
