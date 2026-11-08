#include "library_rescan.h"

#include <stddef.h>

#include "card_files.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "cover_cache.h"
#include "folders_store.h"
#include "hal/clock.h"
#include "hal/input.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "library_store.h"

static const char MUSIC_ROOT[] = "/";
const char LIBRARY_SCRATCH_PATH[] = "/.echo/build.tmp";
const char RESCAN_TIMES_PATH[] = "/.echo/rescan_times.bin";

enum {
    NUMBER_CAPACITY = 12
};

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
    HalFile covers;
    HalFile scratch;
    HalDirEntry entry;
    RescanProgress progress;
    const char *folder;
    const MusicFolders *skip;
} RescanFiles;

static bool open_dir(void *context, const char *path)
{
    RescanFiles *files = context;
    uint32_t since = hal_clock_microseconds();
    bool opened = hal_dir_open(&files->dir, path);
    spend(TIME_LIST, since);
    files->folder = path;
    return opened;
}

static bool next_entry(void *context, ScanEntry *entry)
{
    RescanFiles *files = context;
    bool listed = false;
    do {
        hal_system_keep_alive();
        uint32_t since = hal_clock_microseconds();
        listed = hal_dir_next(&files->dir, &files->entry);
        spend(TIME_LIST, since);
    } while (listed && files->entry.directory &&
             music_folders_skips(files->skip, files->folder, files->entry.name));
    if (!listed) {
        return false;
    }
    entry->directory = files->entry.directory;
    entry->modified = files->entry.modified;
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

static bool read_handle(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const HalFile *file = context;
    hal_system_keep_alive();
    uint32_t since = hal_clock_microseconds();
    bool read = hal_file_read(*file, offset, buffer, length);
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
    *source = (ByteSource){.read = read_handle, .context = &files->file};
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

static ScanResult unopened(const char *path)
{
    return card_files_reserve(path) == HAL_RESERVE_NO_SPACE ? SCAN_NO_ROOM : SCAN_CARD_UNREADABLE;
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
    *stats = (CoverStats){.albums = 0, .covers = 0, .reused = 0, .missing = 0};
    if (capacity == 0) {
        return unopened(COVER_CACHE_PATH);
    }
    if (!hal_file_open(&files->library, LIBRARY_STORE_PATH)) {
        return SCAN_WRITE_FAILED;
    }
    Library library;
    if (library_open(&library, (ByteSource){read_handle, &files->library}, files->library.size) ==
        LIBRARY_OK) {
        card_files_fit_covers(library_count(&library, LIBRARY_ALBUMS));
        capacity = reserved_size_of(COVER_CACHE_PATH);
    }
    bool previous = hal_file_open(&files->covers, COVER_CACHE_PATH);
    CoverJob job = {
        .files = scan->files,
        .library = {read_handle, &files->library},
        .library_size = files->library.size,
        .sink = {write_covers, NULL},
        .previous = {previous ? read_handle : NULL, &files->covers},
        .capacity = capacity,
        .memory = scan->memory,
        .size = scan->size,
        .progress = {report_covers, files},
    };
    ScanResult result = entry->covers(&job, stats);
    if (previous) {
        hal_file_close(&files->covers);
    }
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
    if (capacity == 0) {
        outcome.result = unopened(LIBRARY_STORE_PATH);
        return outcome;
    }
    if (!hal_file_open(&files.scratch, LIBRARY_SCRATCH_PATH)) {
        outcome.result = unopened(LIBRARY_SCRATCH_PATH);
        return outcome;
    }
    const LibraryScanEntry *entry = hal_system_load_rescan_code();
    if (entry == NULL || entry->magic != LIBRARY_SCAN_MAGIC) {
        hal_file_close(&files.scratch);
        outcome.result = SCAN_NO_CODE;
        return outcome;
    }
    files.progress = progress;
    MusicFolders skip;
    folders_store_load(&skip);
    files.skip = &skip;
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
        .scratch = {read_handle, write_scratch, &files.scratch, files.scratch.size},
        .sink = {write_library, NULL},
        .capacity = capacity,
        .progress = {report, &files},
    };
    run_passes(entry, &job, &outcome);
    hal_file_close(&files.scratch);
    hal_system_return_scratch();
    hal_input_drop_presses();
    return outcome;
}

static const char *error_text(ScanResult result)
{
    switch (result) {
    case SCAN_OK:
        return "";
    case SCAN_NO_MUSIC_ROOT:
        return i18n_text(STR_RESCAN_NO_MUSIC);
    case SCAN_OUT_OF_MEMORY:
        return i18n_text(STR_RESCAN_OUT_OF_MEMORY);
    case SCAN_TOO_MANY:
        return i18n_text(STR_RESCAN_TOO_MANY);
    case SCAN_NO_SCRATCH:
        return i18n_text(STR_RESCAN_NO_SCRATCH);
    case SCAN_FOLDER_TOO_BIG:
        return i18n_text(STR_RESCAN_FOLDER_TOO_BIG);
    case SCAN_PATH_TOO_LONG:
        return i18n_text(STR_RESCAN_PATH_TOO_LONG);
    case SCAN_WRITE_FAILED:
        return i18n_text(STR_RESCAN_WRITE_FAILED);
    case SCAN_NO_ROOM:
        return i18n_text(STR_RESCAN_NO_ROOM);
    case SCAN_NO_CODE:
        return i18n_text(STR_RESCAN_NO_CODE);
    case SCAN_RESTART_NEEDED:
        return i18n_text(STR_RESCAN_RESTART_NEEDED);
    case SCAN_CARD_UNREADABLE:
        return i18n_text(STR_RESCAN_CARD_UNREADABLE);
    }
    return "";
}

static void number_text(char *text, size_t capacity, uint32_t value)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append_unsigned(&builder, value);
}

static StringId library_pattern(RescanOutcome outcome)
{
    static const StringId PATTERNS[2][4] = {
        {STR_RESCAN_LIBRARY_TRACKS, STR_RESCAN_LIBRARY_TRACKS_NO_COVERS,
         STR_RESCAN_LIBRARY_TRACKS_COVER, STR_RESCAN_LIBRARY_TRACKS_COVERS},
        {STR_RESCAN_LIBRARY_TRACK, STR_RESCAN_LIBRARY_TRACK_NO_COVERS,
         STR_RESCAN_LIBRARY_TRACK_COVER, STR_RESCAN_LIBRARY_TRACK_COVERS},
    };
    size_t kind = 0;
    if (outcome.cover_result != SCAN_OK) {
        kind = 1;
    } else if (outcome.covers.albums > 0) {
        kind = outcome.covers.covers == 1 ? 2 : 3;
    }
    return PATTERNS[outcome.stats.tracks == 1 ? 1 : 0][kind];
}

const char *library_rescan_message(RescanOutcome outcome, char *text, uint32_t capacity)
{
    if (outcome.result != SCAN_OK) {
        return error_text(outcome.result);
    }
    char tracks[NUMBER_CAPACITY];
    char covers[NUMBER_CAPACITY];
    number_text(tracks, sizeof tracks, outcome.stats.tracks);
    number_text(covers, sizeof covers, outcome.covers.covers);
    const char *arguments[] = {tracks, covers};
    StringBuilder builder = string_builder_make(text, capacity);
    i18n_format(&builder, library_pattern(outcome), arguments, 2);
    return text;
}
