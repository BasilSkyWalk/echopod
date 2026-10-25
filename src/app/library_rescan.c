#include "library_rescan.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "cover_cache.h"
#include "hal/clock.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "library_store.h"

static const char MUSIC_ROOT[] = "/";
const char LIBRARY_SCRATCH_PATH[] = "/.echo/build.tmp";
const char RESCAN_TIMES_PATH[] = "/.echo/rescan_times.bin";

enum {
    TIME_LIST,
    TIME_OPEN,
    TIME_READ,
    TIME_WRITE,
    TIME_TOTAL,
    TIME_KINDS,
    RESCAN_PASSES = 2,
    TIME_SLOTS = RESCAN_PASSES * TIME_KINDS
};

static uint32_t spent[TIME_SLOTS * 2];
static uint32_t pass;

static void spend(uint32_t kind, uint32_t since)
{
    uint32_t slot = pass * TIME_KINDS + kind;
    spent[slot] += hal_clock_microseconds() - since;
    spent[TIME_SLOTS + slot]++;
}

typedef struct RescanFiles {
    HalDir dir;
    HalFile file;
    HalFile library;
    HalFile scratch;
    HalDirEntry entry;
    RescanProgress progress;
} RescanFiles;

static bool open_dir(void *context, const char *path)
{
    RescanFiles *files = context;
    uint32_t since = hal_clock_microseconds();
    bool opened = hal_dir_open(&files->dir, path);
    spend(TIME_LIST, since);
    return opened;
}

static bool next_entry(void *context, ScanEntry *entry)
{
    RescanFiles *files = context;
    hal_system_keep_alive();
    uint32_t since = hal_clock_microseconds();
    bool listed = hal_dir_next(&files->dir, &files->entry);
    spend(TIME_LIST, since);
    if (!listed) {
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
    hal_system_keep_alive();
    uint32_t since = hal_clock_microseconds();
    bool read = hal_file_read(files->file, offset, buffer, length);
    spend(TIME_READ, since);
    return read;
}

static bool open_file(void *context, const char *path, ByteSource *source, uint32_t *size)
{
    RescanFiles *files = context;
    uint32_t since = hal_clock_microseconds();
    bool opened = hal_file_open(&files->file, path);
    spend(TIME_OPEN, since);
    if (!opened) {
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

static bool timed_write(const char *path, uint32_t offset, const void *bytes, uint32_t length)
{
    uint32_t since = hal_clock_microseconds();
    bool written = hal_file_write_at(path, offset, bytes, length);
    spend(TIME_WRITE, since);
    return written;
}

static bool write_library(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    (void)context;
    hal_system_keep_alive();
    return timed_write(LIBRARY_STORE_PATH, offset, bytes, length);
}

static bool read_scratch(void *context, uint32_t offset, void *bytes, uint32_t length)
{
    RescanFiles *files = context;
    hal_system_keep_alive();
    uint32_t since = hal_clock_microseconds();
    bool read = hal_file_read(files->scratch, offset, bytes, length);
    spend(TIME_READ, since);
    return read;
}

static bool write_scratch(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    (void)context;
    hal_system_keep_alive();
    return timed_write(LIBRARY_SCRATCH_PATH, offset, bytes, length);
}

static void report(void *context, uint32_t files, uint32_t tracks)
{
    RescanFiles *rescan = context;
    hal_system_keep_alive();
    if (rescan->progress.tracks != NULL) {
        rescan->progress.tracks(files, tracks);
    }
}

static bool read_library(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    RescanFiles *files = context;
    hal_system_keep_alive();
    uint32_t since = hal_clock_microseconds();
    bool read = hal_file_read(files->library, offset, buffer, length);
    spend(TIME_READ, since);
    return read;
}

static bool write_covers(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    (void)context;
    hal_system_keep_alive();
    return timed_write(COVER_CACHE_PATH, offset, bytes, length);
}

static void report_covers(void *context, uint32_t done, uint32_t total)
{
    RescanFiles *rescan = context;
    hal_system_keep_alive();
    if (rescan->progress.covers != NULL) {
        rescan->progress.covers(done, total);
    }
}

static uint32_t reserved_size_of(const char *path)
{
    HalFile file;
    if (!hal_file_open(&file, path)) {
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

static ScanResult build_covers(const LibraryScanEntry *entry, const ScanJob *scan,
                               CoverStats *stats)
{
    RescanFiles *files = scan->files.context;
    uint32_t capacity = reserved_size_of(COVER_CACHE_PATH);
    *stats = (CoverStats){.albums = 0, .covers = 0, .missing = 0};
    if (capacity == 0) {
        return SCAN_NO_ROOM;
    }
    if (!hal_file_open(&files->library, LIBRARY_STORE_PATH)) {
        return SCAN_WRITE_FAILED;
    }
    CoverJob job = {
        .files = scan->files,
        .library = {read_library, files},
        .library_size = files->library.size,
        .sink = {write_covers, NULL},
        .capacity = capacity,
        .memory = scan->memory,
        .size = scan->size,
        .progress = {report_covers, files},
    };
    ScanResult result = entry->covers(&job, stats);
    hal_file_close(&files->library);
    return result;
}

static void run_passes(const LibraryScanEntry *entry, const ScanJob *job, RescanOutcome *outcome)
{
    for (uint32_t slot = 0; slot < TIME_SLOTS * 2; slot++) {
        spent[slot] = 0;
    }
    pass = 0;
    uint32_t since = hal_clock_microseconds();
    outcome->result = job->memory != NULL ? entry->scan(job, &outcome->stats) : SCAN_NO_SCRATCH;
    spend(TIME_TOTAL, since);
    if (outcome->result == SCAN_OK) {
        pass = 1;
        since = hal_clock_microseconds();
        outcome->cover_result = build_covers(entry, job, &outcome->covers);
        spend(TIME_TOTAL, since);
    }
    (void)hal_file_write(RESCAN_TIMES_PATH, spent, sizeof spent);
}

RescanOutcome library_rescan_run(RescanProgress progress)
{
    static RescanFiles files;
    RescanOutcome outcome = {.result = SCAN_NO_MUSIC_ROOT, .cover_result = SCAN_OK};
    if (hal_system_restart_pending()) {
        outcome.result = SCAN_RESTART_NEEDED;
        return outcome;
    }
    uint32_t capacity = reserved_size_of(LIBRARY_STORE_PATH);
    if (!music_root_exists()) {
        return outcome;
    }
    if (capacity == 0 || !hal_file_open(&files.scratch, LIBRARY_SCRATCH_PATH)) {
        outcome.result = SCAN_NO_ROOM;
        return outcome;
    }
    const LibraryScanEntry *entry = hal_system_load_rescan_code();
    if (entry == NULL || entry->magic != LIBRARY_SCAN_MAGIC) {
        hal_file_close(&files.scratch);
        outcome.result = SCAN_NO_CODE;
        return outcome;
    }
    files.progress = progress;
    uint32_t size = 0;
    void *memory = hal_system_borrow_scratch(&size);
    uint32_t listing_size = 0;
    void *listing = memory != NULL ? hal_system_borrow_listing(&listing_size) : NULL;
    ScanJob job = {
        .files = {open_dir, next_entry, close_dir, open_file, close_file, &files},
        .memory = memory,
        .size = size,
        .listing = listing,
        .listing_size = listing_size,
        .scratch = {read_scratch, write_scratch, &files, files.scratch.size},
        .sink = {write_library, NULL},
        .capacity = capacity,
        .progress = {report, &files},
    };
    run_passes(entry, &job, &outcome);
    hal_file_close(&files.scratch);
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
        return "Library too big for the player's memory";
    case SCAN_TOO_MANY:
        return "Too many tracks for the player";
    case SCAN_NO_SCRATCH:
        return "Couldn't free memory for the scan";
    case SCAN_FOLDER_TOO_BIG:
        return "A folder holds too many files";
    case SCAN_PATH_TOO_LONG:
        return "A folder path is too long";
    case SCAN_WRITE_FAILED:
        return "Could not write the library";
    case SCAN_NO_ROOM:
        return "No room for the library on the card";
    case SCAN_NO_CODE:
        return "Rescan code missing: reinstall the update";
    case SCAN_RESTART_NEEDED:
        return "Restart the player to finish the update";
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
    if (outcome.cover_result != SCAN_OK) {
        string_builder_append(&builder, ", no covers");
    } else if (outcome.covers.albums > 0) {
        string_builder_append(&builder, ", ");
        string_builder_append_unsigned(&builder, outcome.covers.covers);
        string_builder_append(&builder, outcome.covers.covers == 1 ? " cover" : " covers");
    }
    return text;
}
