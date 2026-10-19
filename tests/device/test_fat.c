#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/fat.h"
#include "sparse_card.h"

static const char LONG_NAME_PATH[] =
    "/Music/Sigur R\xc3\xb3s/\xc3\x81g\xc3\xa6tis byrjun/02 Svefn-g-englar with a long name.flac";
static const char INTRO_PATH[] =
    "/Music/Sigur R\xc3\xb3s/\xc3\x81g\xc3\xa6tis byrjun/01 Intro.flac";
static const char EMOJI_PATH[] = "/Music/\xf0\x9f\x8e\xb5 Emoji/track.mp3";

typedef struct Card {
    SparseCard sparse;
    FatVolume volume;
} Card;

static Card *mounted(const char *name)
{
    static Card card;
    CHECK(sparse_card_load(&card.sparse, name));
    CHECK(fat_mount(&card.volume, sparse_card_device(&card.sparse)));
    return &card;
}

static uint8_t pattern_byte(uint32_t seed, uint32_t index)
{
    return (uint8_t)(index * 31u + seed);
}

static void check_pattern(const uint8_t *bytes, uint32_t seed, uint32_t start, uint32_t length)
{
    uint32_t mismatches = 0;
    for (uint32_t index = 0; index < length; index++) {
        mismatches += bytes[index] != pattern_byte(seed, start + index);
    }
    CHECK_EQUAL(0, mismatches);
}

static void check_file(Card *card, const char *path, uint32_t seed, uint32_t size)
{
    FatFile file;
    CHECK(fat_open(&card->volume, path, &file));
    CHECK_EQUAL(size, file.size);
    uint8_t *bytes = malloc(size + 1);
    CHECK(fat_read(&card->volume, &file, 0, bytes, size));
    check_pattern(bytes, seed, 0, size);
    free(bytes);
}

static void check_missing(Card *card, const char *path)
{
    FatFile file;
    CHECK(!fat_open(&card->volume, path, &file));
}

static void the_fat32_card_mounts_from_its_mbr_partition(void)
{
    Card *card = mounted("fat32_mbr");
    CHECK_EQUAL(FAT_TYPE_32, card->volume.type);
    CHECK_EQUAL(63, card->volume.volume_start);
    sparse_card_free(&card->sparse);
}

static void fat32_files_read_back_their_bytes(void)
{
    Card *card = mounted("fat32_mbr");
    check_file(card, "/.echo/library.db", 1, 3000);
    check_file(card, "/README.TXT", 2, 100);
    check_file(card, "/notes.txt", 3, 700);
    check_file(card, INTRO_PATH, 4, 9000);
    check_file(card, LONG_NAME_PATH, 5, 20000);
    check_file(card, EMOJI_PATH, 6, 1500);
    sparse_card_free(&card->sparse);
}

static void a_folder_spanning_many_clusters_is_searched_to_its_end(void)
{
    Card *card = mounted("fat32_mbr");
    check_file(card, "/Many/file number 000.txt", 100, 64);
    check_file(card, "/Many/file number 119.txt", 219, 64);
    sparse_card_free(&card->sparse);
}

static void reads_at_offsets_cross_sector_and_cluster_boundaries(void)
{
    Card *card = mounted("fat32_mbr");
    FatFile file;
    uint8_t bytes[3000];
    CHECK(fat_open(&card->volume, LONG_NAME_PATH, &file));
    CHECK(fat_read(&card->volume, &file, 10000, bytes, 3000));
    check_pattern(bytes, 5, 10000, 3000);
    CHECK(fat_read(&card->volume, &file, 511, bytes, 2));
    check_pattern(bytes, 5, 511, 2);
    CHECK(fat_read(&card->volume, &file, 19990, bytes, 10));
    check_pattern(bytes, 5, 19990, 10);
    sparse_card_free(&card->sparse);
}

static void reads_past_the_end_fail(void)
{
    Card *card = mounted("fat32_mbr");
    FatFile file;
    uint8_t bytes[16];
    CHECK(fat_open(&card->volume, "/README.TXT", &file));
    CHECK(fat_read(&card->volume, &file, 100, bytes, 0));
    CHECK(!fat_read(&card->volume, &file, 95, bytes, 6));
    CHECK(!fat_read(&card->volume, &file, 101, bytes, 0));
    sparse_card_free(&card->sparse);
}

static void an_empty_file_opens_with_size_zero(void)
{
    Card *card = mounted("fat32_mbr");
    FatFile file;
    uint8_t byte = 0;
    CHECK(fat_open(&card->volume, "/Music/Empty.wav", &file));
    CHECK_EQUAL(0, file.size);
    CHECK(fat_read(&card->volume, &file, 0, &byte, 0));
    CHECK(!fat_read(&card->volume, &file, 0, &byte, 1));
    sparse_card_free(&card->sparse);
}

static void ascii_letters_match_in_any_case(void)
{
    Card *card = mounted("fat32_mbr");
    check_file(card, "/readme.txt", 2, 100);
    check_file(card, "/MUSIC/Sigur R\xc3\xb3s/\xc3\x81g\xc3\xa6tis BYRJUN/01 INTRO.flac", 4, 9000);
    sparse_card_free(&card->sparse);
}

static void missing_paths_and_folders_do_not_open_as_files(void)
{
    Card *card = mounted("fat32_mbr");
    check_missing(card, "/Music/Nope.flac");
    check_missing(card, "/README.TXT/inside");
    check_missing(card, "/Music");
    check_missing(card, "/Music/Sigur Ros/\xc3\x81g\xc3\xa6tis byrjun/01 Intro.flac");
    check_missing(card, "/Many/file number 120.txt");
    check_missing(card, "");
    sparse_card_free(&card->sparse);
}

static void a_decomposed_path_does_not_match_the_composed_name(void)
{
    Card *card = mounted("fat32_mbr");
    check_missing(card, "/Music/Sigur Ro\xcc\x81s/\xc3\x81g\xc3\xa6tis byrjun/01 Intro.flac");
    sparse_card_free(&card->sparse);
}

static void the_fat16_card_mounts_without_a_partition_table(void)
{
    Card *card = mounted("fat16_superfloppy");
    CHECK_EQUAL(FAT_TYPE_16, card->volume.type);
    CHECK_EQUAL(0, card->volume.volume_start);
    sparse_card_free(&card->sparse);
}

static void fat16_files_read_back_from_the_root_and_subfolders(void)
{
    Card *card = mounted("fat16_superfloppy");
    check_file(card, "/README.TXT", 2, 100);
    check_file(card, "/.echo/library.db", 1, 3000);
    check_file(card, LONG_NAME_PATH, 5, 20000);
    check_file(card, EMOJI_PATH, 6, 1500);
    check_file(card, "/Many/file number 042.txt", 142, 64);
    check_file(card, "/Many/file number 119.txt", 219, 64);
    sparse_card_free(&card->sparse);
}

static void a_blank_device_does_not_mount(void)
{
    static uint8_t blank[64 * 512];
    SparseCard sparse = {.bytes = blank, .sector_count = 64, .reads = 0};
    FatVolume volume;
    CHECK(!fat_mount(&volume, sparse_card_device(&sparse)));
}

static void put16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static uint8_t *looped_volume(SparseCard *sparse)
{
    enum {
        SECTORS = 5000,
        FAT_SECTORS = 20,
        ROOT_SECTORS = 1
    };
    uint8_t *bytes = calloc(SECTORS, 512);
    bytes[0] = 0xEB;
    put16(bytes + 11, 512);
    bytes[13] = 1;
    put16(bytes + 14, 1);
    bytes[16] = 1;
    put16(bytes + 17, 16);
    put16(bytes + 19, SECTORS);
    put16(bytes + 22, FAT_SECTORS);
    bytes[510] = 0x55;
    bytes[511] = 0xAA;
    uint8_t *fat = bytes + 512;
    put16(fat + 2 * 2, 2);
    uint8_t *root = bytes + 512 * (1 + FAT_SECTORS);
    memcpy(root, "LOOP       ", 11);
    root[11] = 0x10;
    put16(root + 26, 2);
    uint8_t *cluster = bytes + 512 * (1 + FAT_SECTORS + ROOT_SECTORS);
    for (int entry = 0; entry < 16; entry++) {
        cluster[entry * 32] = 0xE5;
    }
    *sparse = (SparseCard){.bytes = bytes, .sector_count = SECTORS, .reads = 0};
    return bytes;
}

static void a_folder_whose_clusters_loop_ends_the_search(void)
{
    SparseCard sparse;
    uint8_t *bytes = looped_volume(&sparse);
    FatVolume volume;
    FatFile file;
    CHECK(fat_mount(&volume, sparse_card_device(&sparse)));
    CHECK(!fat_open(&volume, "/LOOP/missing.txt", &file));
    free(bytes);
}

static void the_exfat_card_mounts_from_its_mbr_partition(void)
{
    Card *card = mounted("exfat_mbr");
    CHECK_EQUAL(FAT_TYPE_EXFAT, card->volume.type);
    CHECK_EQUAL(63, card->volume.volume_start);
    sparse_card_free(&card->sparse);
}

static void exfat_files_read_back_their_bytes(void)
{
    Card *card = mounted("exfat_mbr");
    check_file(card, "/.echo/library.db", 1, 3000);
    check_file(card, "/README.TXT", 2, 100);
    check_file(card, "/notes.txt", 3, 700);
    check_file(card, INTRO_PATH, 4, 9000);
    check_file(card, LONG_NAME_PATH, 5, 20000);
    check_file(card, EMOJI_PATH, 6, 1500);
    sparse_card_free(&card->sparse);
}

static void an_exfat_folder_with_a_cluster_chain_is_searched_to_its_end(void)
{
    Card *card = mounted("exfat_mbr");
    check_file(card, "/Many/file number 000.txt", 100, 64);
    check_file(card, "/Many/file number 119.txt", 219, 64);
    sparse_card_free(&card->sparse);
}

static void exfat_reads_at_offsets_cross_cluster_boundaries(void)
{
    Card *card = mounted("exfat_mbr");
    FatFile file;
    uint8_t bytes[5000];
    CHECK(fat_open(&card->volume, LONG_NAME_PATH, &file));
    CHECK(fat_read(&card->volume, &file, 4000, bytes, 5000));
    check_pattern(bytes, 5, 4000, 5000);
    CHECK(fat_read(&card->volume, &file, 4095, bytes, 2));
    check_pattern(bytes, 5, 4095, 2);
    CHECK(!fat_read(&card->volume, &file, 19990, bytes, 11));
    sparse_card_free(&card->sparse);
}

static void a_long_read_of_consecutive_sectors_takes_one_block_read(void)
{
    Card *card = mounted("exfat_mbr");
    FatFile file;
    uint8_t bytes[4096];
    CHECK(fat_open(&card->volume, LONG_NAME_PATH, &file));
    card->sparse.calls = 0;
    card->sparse.reads = 0;
    CHECK(fat_read(&card->volume, &file, 0, bytes, 4096));
    check_pattern(bytes, 5, 0, 4096);
    CHECK_EQUAL(1u, card->sparse.calls);
    CHECK_EQUAL(8u, card->sparse.reads);
    CHECK(fat_read(&card->volume, &file, 4096, bytes, 4096));
    check_pattern(bytes, 5, 4096, 4096);
    CHECK_EQUAL(2u, card->sparse.calls);
    sparse_card_free(&card->sparse);
}

static void a_read_that_starts_inside_the_cached_run_reads_only_the_rest(void)
{
    Card *card = mounted("exfat_mbr");
    FatFile file;
    uint8_t bytes[4096];
    CHECK(fat_open(&card->volume, LONG_NAME_PATH, &file));
    CHECK(fat_read(&card->volume, &file, 0, bytes, 4096));
    card->sparse.calls = 0;
    card->sparse.reads = 0;
    CHECK(fat_read(&card->volume, &file, 3000, bytes, 4096));
    check_pattern(bytes, 5, 3000, 4096);
    CHECK_EQUAL(1u, card->sparse.calls);
    CHECK_EQUAL(8u, card->sparse.reads);
    sparse_card_free(&card->sparse);
}

static void exfat_names_match_like_fat_names(void)
{
    Card *card = mounted("exfat_mbr");
    FatFile file;
    check_file(card, "/readme.txt", 2, 100);
    CHECK(fat_open(&card->volume, "/Music/Empty.wav", &file));
    CHECK_EQUAL(0, file.size);
    check_missing(card, "/Music/Nope.flac");
    check_missing(card, "/Music");
    check_missing(card, "/README.TXT/inside");
    check_missing(card, "/Music/Sigur Ro\xcc\x81s/\xc3\x81g\xc3\xa6tis byrjun/01 Intro.flac");
    check_missing(card, "/Many/file number 120.txt");
    sparse_card_free(&card->sparse);
}

static void fill_written(uint8_t *bytes, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        bytes[index] = (uint8_t)(0xA5u ^ index);
    }
}

static void check_written(const uint8_t *bytes, uint32_t length)
{
    uint32_t mismatches = 0;
    for (uint32_t index = 0; index < length; index++) {
        mismatches += bytes[index] != (uint8_t)(0xA5u ^ index);
    }
    CHECK_EQUAL(0, mismatches);
}

static void check_write_in_place(const char *name)
{
    Card *card = mounted(name);
    FatFile file;
    static uint8_t written[1200];
    static uint8_t bytes[20000];
    fill_written(written, sizeof written);
    CHECK(fat_open(&card->volume, LONG_NAME_PATH, &file));
    CHECK(fat_read(&card->volume, &file, 0, bytes, 600));
    CHECK(fat_write(&card->volume, &file, 3500, written, sizeof written));
    CHECK(fat_read(&card->volume, &file, 3500, bytes, sizeof written));
    check_written(bytes, sizeof written);
    CHECK(fat_mount(&card->volume, sparse_card_device(&card->sparse)));
    CHECK(fat_open(&card->volume, LONG_NAME_PATH, &file));
    CHECK_EQUAL(20000, file.size);
    CHECK(fat_read(&card->volume, &file, 0, bytes, 20000));
    check_pattern(bytes, 5, 0, 3500);
    check_written(bytes + 3500, sizeof written);
    check_pattern(bytes + 4700, 5, 4700, 20000 - 4700);
    check_file(card, INTRO_PATH, 4, 9000);
    sparse_card_free(&card->sparse);
}

static void fat32_writes_land_in_place_across_sectors_and_clusters(void)
{
    check_write_in_place("fat32_mbr");
}

static void fat16_writes_land_in_place_across_sectors_and_clusters(void)
{
    check_write_in_place("fat16_superfloppy");
}

static void exfat_writes_land_in_place_across_sectors_and_clusters(void)
{
    check_write_in_place("exfat_mbr");
}

static void a_write_past_the_end_fails_and_changes_nothing(void)
{
    Card *card = mounted("exfat_mbr");
    FatFile file;
    uint8_t written[20];
    fill_written(written, sizeof written);
    CHECK(fat_open(&card->volume, "/README.TXT", &file));
    CHECK(!fat_write(&card->volume, &file, 90, written, sizeof written));
    CHECK_EQUAL(0, card->sparse.writes);
    check_file(card, "/README.TXT", 2, 100);
    sparse_card_free(&card->sparse);
}

static void a_device_without_writes_refuses_to_write(void)
{
    Card *card = mounted("fat32_mbr");
    BlockDevice read_only = sparse_card_device(&card->sparse);
    read_only.write = NULL;
    CHECK(fat_mount(&card->volume, read_only));
    FatFile file;
    uint8_t written[4] = {1, 2, 3, 4};
    CHECK(fat_open(&card->volume, "/README.TXT", &file));
    CHECK(!fat_write(&card->volume, &file, 0, written, sizeof written));
    check_file(card, "/README.TXT", 2, 100);
    sparse_card_free(&card->sparse);
}

typedef struct Listing {
    int count;
    int directories;
    bool found;
    FatDirEntry match;
} Listing;

static Listing list(Card *card, const char *path, const char *wanted)
{
    Listing listing = {.count = 0, .directories = 0, .found = false};
    FatDirCursor cursor;
    CHECK(fat_open_directory(&card->volume, path, &cursor));
    static FatDirEntry entry;
    while (fat_next_entry(&card->volume, &cursor, &entry)) {
        if (strncmp(entry.name, "._", 2) == 0) {
            continue;
        }
        listing.count++;
        listing.directories += entry.file.directory;
        if (wanted != NULL && strcmp(entry.name, wanted) == 0) {
            listing.found = true;
            listing.match = entry;
        }
    }
    return listing;
}

static void check_root_listing(const char *name)
{
    Card *card = mounted(name);
    Listing music = list(card, "/", "Music");
    CHECK(music.found);
    CHECK(music.match.file.directory);
    Listing notes = list(card, "/", "notes.txt");
    CHECK(notes.found);
    CHECK(!notes.match.file.directory);
    CHECK_EQUAL(700, notes.match.file.size);
    CHECK_EQUAL(2026u, (notes.match.modified >> 25) + 1980u);
    CHECK_EQUAL(10u, notes.match.modified >> 21 & 15u);
    CHECK(list(card, "/", "README.TXT").found);
    CHECK(list(card, "/", ".echo").found);
    CHECK(!list(card, "/", ".").found);
    CHECK(!list(card, "/", "..").found);
    sparse_card_free(&card->sparse);
}

static void the_fat32_root_lists_its_files_and_folders(void)
{
    check_root_listing("fat32_mbr");
}

static void the_fat16_root_lists_its_files_and_folders(void)
{
    check_root_listing("fat16_superfloppy");
}

static void the_exfat_root_lists_its_files_and_folders(void)
{
    check_root_listing("exfat_mbr");
}

static void check_folder_listing(const char *name)
{
    Card *card = mounted(name);
    Listing album = list(card, "/Music/Sigur R\xc3\xb3s/\xc3\x81g\xc3\xa6tis byrjun",
                         "02 Svefn-g-englar with a long name.flac");
    CHECK_EQUAL(2, album.count);
    CHECK(album.found);
    CHECK_EQUAL(20000, album.match.file.size);
    Listing music = list(card, "/Music", "\xf0\x9f\x8e\xb5 Emoji");
    CHECK(music.found);
    CHECK(music.match.file.directory);
    Listing many = list(card, "/Many", "file number 119.txt");
    CHECK_EQUAL(120, many.count);
    CHECK_EQUAL(0, many.directories);
    CHECK(many.found);
    sparse_card_free(&card->sparse);
}

static void fat32_folders_list_long_unicode_names(void)
{
    check_folder_listing("fat32_mbr");
}

static void exfat_folders_list_long_unicode_names(void)
{
    check_folder_listing("exfat_mbr");
}

static void a_listed_file_opens_and_reads(void)
{
    Card *card = mounted("exfat_mbr");
    Listing readme = list(card, "/", "README.TXT");
    uint8_t bytes[100];
    CHECK(fat_read(&card->volume, &readme.match.file, 0, bytes, sizeof bytes));
    check_pattern(bytes, 2, 0, sizeof bytes);
    sparse_card_free(&card->sparse);
}

static void a_file_is_not_a_directory_to_list(void)
{
    Card *card = mounted("fat32_mbr");
    FatDirCursor cursor;
    CHECK(!fat_open_directory(&card->volume, "/notes.txt", &cursor));
    CHECK(!fat_open_directory(&card->volume, "/Missing", &cursor));
    sparse_card_free(&card->sparse);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_fat32_card_mounts_from_its_mbr_partition),
        TEST_CASE(fat32_files_read_back_their_bytes),
        TEST_CASE(a_folder_spanning_many_clusters_is_searched_to_its_end),
        TEST_CASE(reads_at_offsets_cross_sector_and_cluster_boundaries),
        TEST_CASE(reads_past_the_end_fail),
        TEST_CASE(an_empty_file_opens_with_size_zero),
        TEST_CASE(ascii_letters_match_in_any_case),
        TEST_CASE(missing_paths_and_folders_do_not_open_as_files),
        TEST_CASE(a_decomposed_path_does_not_match_the_composed_name),
        TEST_CASE(the_fat16_card_mounts_without_a_partition_table),
        TEST_CASE(fat16_files_read_back_from_the_root_and_subfolders),
        TEST_CASE(a_blank_device_does_not_mount),
        TEST_CASE(a_folder_whose_clusters_loop_ends_the_search),
        TEST_CASE(the_exfat_card_mounts_from_its_mbr_partition),
        TEST_CASE(exfat_files_read_back_their_bytes),
        TEST_CASE(an_exfat_folder_with_a_cluster_chain_is_searched_to_its_end),
        TEST_CASE(exfat_reads_at_offsets_cross_cluster_boundaries),
        TEST_CASE(a_long_read_of_consecutive_sectors_takes_one_block_read),
        TEST_CASE(a_read_that_starts_inside_the_cached_run_reads_only_the_rest),
        TEST_CASE(exfat_names_match_like_fat_names),
        TEST_CASE(fat32_writes_land_in_place_across_sectors_and_clusters),
        TEST_CASE(fat16_writes_land_in_place_across_sectors_and_clusters),
        TEST_CASE(exfat_writes_land_in_place_across_sectors_and_clusters),
        TEST_CASE(a_write_past_the_end_fails_and_changes_nothing),
        TEST_CASE(a_device_without_writes_refuses_to_write),
        TEST_CASE(the_fat32_root_lists_its_files_and_folders),
        TEST_CASE(the_fat16_root_lists_its_files_and_folders),
        TEST_CASE(the_exfat_root_lists_its_files_and_folders),
        TEST_CASE(fat32_folders_list_long_unicode_names),
        TEST_CASE(exfat_folders_list_long_unicode_names),
        TEST_CASE(a_listed_file_opens_and_reads),
        TEST_CASE(a_file_is_not_a_directory_to_list),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
