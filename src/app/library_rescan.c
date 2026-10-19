#include "library_rescan.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/storage.h"
#include "hal/system.h"

static const char LIBRARY_PATH[] = "/.echo/library.db";
static const char MUSIC_ROOT[] = "/";

typedef struct RescanFiles {
    HalDir dir;
    HalFile file;
    HalDirEntry entry;
    RescanProgress progress;
} RescanFiles;

static bool open_dir(void *context, const char *path)
{
    RescanFiles *files = context;
    return hal_dir_open(&files->dir, path);
}

static bool next_entry(void *context, ScanEntry *entry)
{
    RescanFiles *files = context;
    hal_system_keep_alive();
    if (!hal_dir_next(&files->dir, &files->entry)) {
        return false;
    }
    entry->directory = files->entry.directory;
    for (size_t index = 0; index < sizeof entry->name; index++) {
        entry->name[index] = files->entry.name[index];
    }
    return true;
}

static void close_dir(void *context)
{
    RescanFiles *files = context;
    hal_dir_close(&files->dir);
}

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    RescanFiles *files = context;
    return hal_file_read(files->file, offset, buffer, length);
}

static bool open_file(void *context, const char *path, ByteSource *source, uint32_t *size)
{
    RescanFiles *files = context;
    if (!hal_file_open(&files->file, path)) {
        return false;
    }
    *source = (ByteSource){.read = read_file, .context = files};
    *size = files->file.size;
    return true;
}

static void close_file(void *context)
{
    RescanFiles *files = context;
    hal_file_close(&files->file);
}

static bool write_library(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    (void)context;
    hal_system_keep_alive();
    return hal_file_write_at(LIBRARY_PATH, offset, bytes, length);
}

static void report(void *context, uint32_t files, uint32_t tracks)
{
    RescanFiles *rescan = context;
    hal_system_keep_alive();
    if (rescan->progress != NULL) {
        rescan->progress(files, tracks);
    }
}

static uint32_t reserved_size(void)
{
    HalFile file;
    if (!hal_file_open(&file, LIBRARY_PATH)) {
        return 0;
    }
    uint32_t size = file.size;
    hal_file_close(&file);
    return size;
}

static bool music_root_exists(void)
{
    HalDir dir;
    if (!hal_dir_open(&dir, MUSIC_ROOT)) {
        return false;
    }
    hal_dir_close(&dir);
    return true;
}

RescanOutcome library_rescan_run(RescanProgress progress)
{
    static RescanFiles files;
    RescanOutcome outcome = {.result = SCAN_NO_MUSIC_ROOT};
    uint32_t capacity = reserved_size();
    if (!music_root_exists()) {
        return outcome;
    }
    if (capacity == 0) {
        outcome.result = SCAN_NO_ROOM;
        return outcome;
    }
    const LibraryScanEntry *entry = hal_system_load_rescan_code();
    if (entry == NULL || entry->magic != LIBRARY_SCAN_MAGIC) {
        outcome.result = SCAN_NO_CODE;
        return outcome;
    }
    files.progress = progress;
    uint32_t size = 0;
    void *memory = hal_system_borrow_scratch(&size);
    ScanJob job = {
        .files = {open_dir, next_entry, close_dir, open_file, close_file, &files},
        .memory = memory,
        .size = size,
        .sink = {write_library, NULL},
        .capacity = capacity,
        .progress = {report, &files},
    };
    outcome.result = memory != NULL ? entry->scan(&job, &outcome.stats) : SCAN_OUT_OF_MEMORY;
    hal_system_return_scratch();
    return outcome;
}

static const char *error_text(ScanResult result)
{
    switch (result) {
    case SCAN_OK:
        return "";
    case SCAN_NO_MUSIC_ROOT:
        return "No music found on the card";
    case SCAN_OUT_OF_MEMORY:
    case SCAN_TOO_MANY:
        return "Library too big for the player";
    case SCAN_FOLDER_TOO_BIG:
        return "A folder holds too many files";
    case SCAN_PATH_TOO_LONG:
        return "A folder path is too long";
    case SCAN_WRITE_FAILED:
        return "Could not write the library";
    case SCAN_NO_ROOM:
        return "No room for the library: run save_files.py";
    case SCAN_NO_CODE:
        return "Rescan code missing: reinstall the update";
    }
    return "";
}

const char *library_rescan_message(RescanOutcome outcome, char *text, uint32_t capacity)
{
    if (outcome.result != SCAN_OK) {
        return error_text(outcome.result);
    }
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, "Library: ");
    string_builder_append_unsigned(&builder, outcome.stats.tracks);
    string_builder_append(&builder, outcome.stats.tracks == 1 ? " track" : " tracks");
    return text;
}
