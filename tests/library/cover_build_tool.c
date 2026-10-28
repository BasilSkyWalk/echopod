#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/storage.h"
#include "library/cover_build.h"
#include "library/library_scan.h"
#include "sim_storage.h"

enum {
    MEMORY_SIZE = 384 * 1024,
    LIBRARY_CAPACITY = 4 * 1024 * 1024,
    COVERS_CAPACITY = 8 * 1024 * 1024,
    SCRATCH_CAPACITY = 32 * 1024 * 1024
};

typedef struct HostFiles {
    HalDir dir;
    HalFile file;
    HalDirEntry entry;
} HostFiles;

typedef struct Output {
    uint8_t *bytes;
    uint32_t capacity;
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
    entry->modified = files->entry.modified;
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
    if (offset > output->capacity || length > output->capacity - offset) {
        return false;
    }
    memcpy(output->bytes + offset, bytes, length);
    output->length = offset + length > output->length ? offset + length : output->length;
    return true;
}

static bool read_output(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const Output *output = context;
    if (offset > output->capacity || length > output->capacity - offset) {
        return false;
    }
    memcpy(buffer, output->bytes + offset, length);
    return true;
}

static bool save(const char *folder, const char *name, const Output *output)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", folder, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    bool written = fwrite(output->bytes, 1, output->length, file) == output->length;
    return fclose(file) == 0 && written;
}

static bool load(const char *folder, const char *name, Output *output)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", folder, name);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    size_t length = fread(output->bytes, 1, output->capacity, file);
    fclose(file);
    output->length = (uint32_t)length;
    return length > 0;
}

int main(int argc, char **argv)
{
    static uint8_t memory[MEMORY_SIZE];
    static HostFiles files;
    if (argc != 3) {
        fprintf(stderr, "usage: cover_build_tool MUSIC OUTPUT_FOLDER\n");
        return 1;
    }
    Output library = {calloc(LIBRARY_CAPACITY, 1), LIBRARY_CAPACITY, 0};
    Output covers = {calloc(COVERS_CAPACITY, 1), COVERS_CAPACITY, 0};
    Output scratch = {calloc(SCRATCH_CAPACITY, 1), SCRATCH_CAPACITY, 0};
    sim_storage_configure(argv[1], NULL);
    ScanFileSystem system = {open_dir, next_entry, close_dir, open_file, close_file, &files};
    ScanStats scan_stats;
    ScanJob scan = {.files = system,
                    .memory = memory,
                    .size = sizeof memory,
                    .scratch = {read_output, write_output, &scratch, SCRATCH_CAPACITY},
                    .sink = {write_output, &library},
                    .capacity = LIBRARY_CAPACITY};
    ScanResult result = library_scan(&scan, &scan_stats);
    if (result != SCAN_OK) {
        fprintf(stderr, "scan failed: %d\n", result);
        return 2;
    }
    CoverStats cover_stats;
    bool previous = load(argv[2], "covers.bin", &covers);
    CoverJob job = {
        .files = system,
        .library = {read_output, &library},
        .library_size = LIBRARY_CAPACITY,
        .sink = {write_output, &covers},
        .previous = {previous ? read_output : NULL, &covers},
        .capacity = COVERS_CAPACITY,
        .memory = memory,
        .size = sizeof memory,
    };
    result = cover_build(&job, &cover_stats);
    if (result != SCAN_OK) {
        fprintf(stderr, "covers failed: %d\n", result);
        return 3;
    }
    printf("%u tracks, %u albums, %u covers, %u reused, %u missing\n", scan_stats.tracks,
           cover_stats.albums, cover_stats.covers, cover_stats.reused, cover_stats.missing);
    return save(argv[2], "library.db", &library) && save(argv[2], "covers.bin", &covers) ? 0 : 4;
}
