#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "hal/storage.h"
#include "library/library_scan.h"
#include "sim_storage.h"

enum {
    MEMORY_SIZE = 384 * 1024,
    OUTPUT_CAPACITY = 64 * 1024
};

typedef struct HostFiles {
    HalDir dir;
    HalFile file;
    HalDirEntry entry;
} HostFiles;

typedef struct Output {
    uint8_t bytes[OUTPUT_CAPACITY];
    uint32_t length;
} Output;

static bool open_dir(void *context, const char *path)
{
    HostFiles *files = context;
    return hal_dir_open(&files->dir, path);
}

static bool next_entry(void *context, ScanEntry *entry)
{
    HostFiles *files = context;
    if (!hal_dir_next(&files->dir, &files->entry)) {
        return false;
    }
    entry->directory = files->entry.directory;
    memcpy(entry->name, files->entry.name, sizeof entry->name);
    return true;
}

static void close_dir(void *context)
{
    HostFiles *files = context;
    hal_dir_close(&files->dir);
}

static bool read_host(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    HostFiles *files = context;
    return hal_file_read(files->file, offset, buffer, length);
}

static bool open_file(void *context, const char *path, ByteSource *source, uint32_t *size)
{
    HostFiles *files = context;
    if (!hal_file_open(&files->file, path)) {
        return false;
    }
    *source = (ByteSource){.read = read_host, .context = files};
    *size = files->file.size;
    return true;
}

static void close_file(void *context)
{
    HostFiles *files = context;
    hal_file_close(&files->file);
}

static bool write_output(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Output *output = context;
    if (offset + length > OUTPUT_CAPACITY) {
        return false;
    }
    memcpy(output->bytes + offset, bytes, length);
    output->length = offset + length > output->length ? offset + length : output->length;
    return true;
}

typedef struct Progress {
    uint32_t calls;
    uint32_t last_tracks;
} Progress;

static void report(void *context, uint32_t files, uint32_t tracks)
{
    Progress *progress = context;
    (void)files;
    progress->calls++;
    progress->last_tracks = tracks;
}

static uint8_t *read_fixture(const char *name, uint32_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    fseek(file, 0, SEEK_END);
    *size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *bytes = malloc(*size);
    CHECK(fread(bytes, 1, *size, file) == *size);
    fclose(file);
    return bytes;
}

static ScanResult scan_folder(const char *folder, Output *output, Progress *progress,
                              ScanStats *stats)
{
    static uint8_t memory[MEMORY_SIZE];
    static HostFiles files;
    sim_storage_configure(folder, NULL);
    output->length = 0;
    ScanJob job = {
        .files = {open_dir, next_entry, close_dir, open_file, close_file, &files},
        .memory = memory,
        .size = sizeof memory,
        .sink = {write_output, output},
        .progress = {report, progress},
    };
    return library_scan(&job, stats);
}

static void scanning_the_tag_fixtures_matches_the_pc_indexer(void)
{
    static Output output;
    Progress progress = {.calls = 0};
    ScanStats stats;
    CHECK_EQUAL(SCAN_OK, scan_folder(ECHO_FIXTURE_DIR "/tags", &output, &progress, &stats));
    CHECK_EQUAL(13u, stats.tracks);
    CHECK_EQUAL(13u, progress.last_tracks);
    CHECK(progress.calls >= 13);
    uint32_t expected_size = 0;
    uint8_t *expected = read_fixture("tags_library.db", &expected_size);
    CHECK_EQUAL(expected_size, stats.length);
    CHECK_EQUAL(expected_size, output.length);
    CHECK_EQUAL(0, memcmp(expected, output.bytes, expected_size));
    free(expected);
}

static void scanning_folders_and_playlists_matches_the_pc_indexer(void)
{
    static Output output;
    Progress progress = {.calls = 0};
    ScanStats stats;
    CHECK_EQUAL(SCAN_OK, scan_folder(ECHO_FIXTURE_DIR "/scan_music", &output, &progress, &stats));
    CHECK_EQUAL(4u, stats.tracks);
    uint32_t expected_size = 0;
    uint8_t *expected = read_fixture("scan_music.db", &expected_size);
    CHECK_EQUAL(expected_size, stats.length);
    CHECK_EQUAL(0, memcmp(expected, output.bytes, expected_size));
    free(expected);
}

static void a_missing_music_root_says_so(void)
{
    static Output output;
    Progress progress = {.calls = 0};
    ScanStats stats;
    CHECK_EQUAL(SCAN_NO_MUSIC_ROOT,
                scan_folder(ECHO_FIXTURE_DIR "/no such folder", &output, &progress, &stats));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(scanning_the_tag_fixtures_matches_the_pc_indexer),
        TEST_CASE(scanning_folders_and_playlists_matches_the_pc_indexer),
        TEST_CASE(a_missing_music_root_says_so),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
