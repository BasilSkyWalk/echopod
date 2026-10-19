#include "music_check.h"

#include <stddef.h>
#include <stdint.h>

#include "hal/storage.h"
#include "hal/system.h"

const char MUSIC_CHECK_PATH[] = "/.echo/scan.bin";

enum {
    RECORD_BYTES = 16,
    RECORD_MAGIC = 0x4353454D
};

typedef struct MusicWalk {
    HalDir dir;
    HalDirEntry entry;
} MusicWalk;

static MusicWalk walk;
static FingerprintWork work;

static bool open_dir(void *context, const char *path)
{
    MusicWalk *state = context;
    hal_system_keep_alive();
    return hal_dir_open(&state->dir, path);
}

static bool next_entry(void *context, FingerprintEntry *entry)
{
    MusicWalk *state = context;
    if (!hal_dir_next(&state->dir, &state->entry)) {
        return false;
    }
    entry->directory = state->entry.directory;
    entry->size = state->entry.size;
    entry->modified = state->entry.modified;
    for (size_t index = 0; index < sizeof entry->name; index++) {
        entry->name[index] = state->entry.name[index];
    }
    return true;
}

static void close_dir(void *context)
{
    MusicWalk *state = context;
    hal_dir_close(&state->dir);
}

MusicFingerprint music_check_fingerprint(void)
{
    FingerprintFiles files = {open_dir, next_entry, close_dir, &walk};
    return music_fingerprint(&files, &work);
}

static void put_word(uint8_t *bytes, uint32_t value)
{
    for (int index = 0; index < 4; index++) {
        bytes[index] = (uint8_t)(value >> (8 * index));
    }
}

static uint32_t get_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool read_record(MusicFingerprint *print)
{
    HalFile file;
    uint8_t bytes[RECORD_BYTES];
    if (!hal_file_open(&file, MUSIC_CHECK_PATH)) {
        return false;
    }
    bool read = file.size >= RECORD_BYTES && hal_file_read(file, 0, bytes, sizeof bytes);
    hal_file_close(&file);
    if (!read || get_word(bytes) != RECORD_MAGIC) {
        return false;
    }
    *print = (MusicFingerprint){.complete = true,
                                .first = get_word(bytes + 4),
                                .second = get_word(bytes + 8),
                                .entries = get_word(bytes + 12)};
    return true;
}

bool music_check_unchanged(void)
{
    MusicFingerprint stored;
    return read_record(&stored) && music_fingerprints_match(stored, music_check_fingerprint());
}

void music_check_remember(MusicFingerprint print)
{
    if (!print.complete) {
        return;
    }
    uint8_t bytes[RECORD_BYTES];
    put_word(bytes, RECORD_MAGIC);
    put_word(bytes + 4, print.first);
    put_word(bytes + 8, print.second);
    put_word(bytes + 12, print.entries);
    (void)hal_file_write_at(MUSIC_CHECK_PATH, 0, bytes, sizeof bytes);
}
