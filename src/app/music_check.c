#include "music_check.h"

#include <stddef.h>
#include <stdint.h>

#include "hal/storage.h"
#include "hal/system.h"

const char MUSIC_CHECK_PATH[] = "/.echo/scan.bin";

enum {
    PRINT_BYTES = 16,
    VERSION_BYTES = 16,
    RECORD_BYTES = PRINT_BYTES + VERSION_BYTES,
    RECORD_MAGIC = 0x4353454D
};

typedef struct MusicWalk {
    HalDir dirs[FINGERPRINT_MAX_DEPTH];
    HalDirEntry entry;
} MusicWalk;

static MusicWalk walk;
static FingerprintWork work;

static bool open_dir(void *context, uint32_t level, const char *path)
{
    MusicWalk *state = context;
    hal_system_keep_alive();
    return hal_dir_open(&state->dirs[level], path);
}

static bool next_entry(void *context, uint32_t level, FingerprintEntry *entry)
{
    MusicWalk *state = context;
    hal_system_keep_alive();
    if (!hal_dir_next(&state->dirs[level], &state->entry)) {
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

static void close_dir(void *context, uint32_t level)
{
    MusicWalk *state = context;
    hal_dir_close(&state->dirs[level]);
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

static bool read_bytes(uint8_t *bytes)
{
    HalFile file;
    if (!hal_file_open(&file, MUSIC_CHECK_PATH)) {
        return false;
    }
    bool read = file.size >= RECORD_BYTES && hal_file_read(file, 0, bytes, RECORD_BYTES);
    hal_file_close(&file);
    return read;
}

static bool decode_print(const uint8_t *bytes, MusicFingerprint *print)
{
    if (get_word(bytes) != RECORD_MAGIC) {
        return false;
    }
    *print = (MusicFingerprint){.complete = true,
                                .first = get_word(bytes + 4),
                                .second = get_word(bytes + 8),
                                .entries = get_word(bytes + 12)};
    return true;
}

static bool read_record(MusicFingerprint *print)
{
    uint8_t bytes[RECORD_BYTES];
    return read_bytes(bytes) && decode_print(bytes, print);
}

bool music_check_unchanged(void)
{
    MusicFingerprint stored;
    return read_record(&stored) && music_fingerprints_match(stored, music_check_fingerprint());
}

static void put_version(uint8_t *bytes)
{
    const char *version = hal_system_version();
    for (size_t index = 0; index < VERSION_BYTES; index++) {
        bytes[index] = 0;
    }
    for (size_t index = 0; index + 1 < VERSION_BYTES && version[index] != '\0'; index++) {
        bytes[index] = (uint8_t)version[index];
    }
}

static bool version_matches(const uint8_t *bytes)
{
    uint8_t current[VERSION_BYTES];
    put_version(current);
    for (size_t index = 0; index < VERSION_BYTES; index++) {
        if (bytes[index] != current[index]) {
            return false;
        }
    }
    return true;
}

static void write_record(MusicFingerprint print)
{
    uint8_t bytes[RECORD_BYTES];
    put_word(bytes, RECORD_MAGIC);
    put_word(bytes + 4, print.first);
    put_word(bytes + 8, print.second);
    put_word(bytes + 12, print.entries);
    put_version(bytes + PRINT_BYTES);
    (void)hal_file_write_at(MUSIC_CHECK_PATH, 0, bytes, sizeof bytes);
}

void music_check_remember(MusicFingerprint print)
{
    if (print.complete) {
        write_record(print);
    }
}

bool music_check_firmware_is_new(void)
{
    uint8_t bytes[RECORD_BYTES];
    return read_bytes(bytes) && !version_matches(bytes + PRINT_BYTES);
}

void music_check_note_firmware(void)
{
    uint8_t bytes[RECORD_BYTES];
    MusicFingerprint stored;
    if (read_bytes(bytes) && decode_print(bytes, &stored) &&
        !version_matches(bytes + PRINT_BYTES)) {
        write_record(stored);
    }
}
