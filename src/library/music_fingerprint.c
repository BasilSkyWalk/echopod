#include "library/music_fingerprint.h"

#include <stddef.h>

#include "core/hash.h"

#define SECOND_SEED (HASH_START ^ UINT32_C(0x5BD1E995))

typedef struct Walk {
    const FingerprintFiles *files;
    FingerprintWork *work;
    MusicFingerprint print;
    uint32_t depth;
} Walk;

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void mix(Walk *walk, const void *bytes, size_t length)
{
    walk->print.first = hash_update(walk->print.first, bytes, length);
    walk->print.second = hash_update(walk->print.second, bytes, length);
}

static void mix_word(Walk *walk, uint32_t value)
{
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16),
                        (uint8_t)(value >> 24)};
    mix(walk, bytes, sizeof bytes);
}

static bool read_entry(Walk *walk, uint32_t index)
{
    const FingerprintFiles *files = walk->files;
    if (!files->open_dir(files->context, walk->work->path)) {
        return false;
    }
    bool found = false;
    for (uint32_t skipped = 0; skipped <= index; skipped++) {
        found = files->next_entry(files->context, &walk->work->entry);
        if (!found) {
            break;
        }
    }
    files->close_dir(files->context);
    return found;
}

static void mix_entry(Walk *walk, const FingerprintEntry *entry)
{
    mix(walk, walk->work->path, text_length(walk->work->path));
    mix(walk, "/", 1);
    mix(walk, entry->name, text_length(entry->name));
    mix_word(walk, entry->directory ? 1u : 0u);
    mix_word(walk, entry->size);
    mix_word(walk, entry->modified);
    walk->print.entries++;
}

static bool enter(Walk *walk, const char *name)
{
    FingerprintWork *work = walk->work;
    uint32_t length = work->levels[walk->depth].path_length;
    uint32_t name_length = text_length(name);
    bool root = length == 1;
    uint32_t needed = length + (root ? 0u : 1u) + name_length + 1u;
    if (walk->depth + 1 >= FINGERPRINT_MAX_DEPTH || needed > FINGERPRINT_PATH_CAPACITY) {
        return false;
    }
    if (!root) {
        work->path[length++] = '/';
    }
    for (uint32_t index = 0; index < name_length; index++) {
        work->path[length++] = name[index];
    }
    work->path[length] = '\0';
    walk->depth++;
    work->levels[walk->depth] = (FingerprintLevel){.path_length = length, .next_index = 0};
    return true;
}

static void leave(Walk *walk)
{
    walk->depth--;
    walk->work->path[walk->work->levels[walk->depth].path_length] = '\0';
}

MusicFingerprint music_fingerprint(const FingerprintFiles *files, FingerprintWork *work)
{
    Walk walk = {.files = files,
                 .work = work,
                 .print = {.complete = false, .first = HASH_START, .second = SECOND_SEED},
                 .depth = 0};
    work->path[0] = '/';
    work->path[1] = '\0';
    work->levels[0] = (FingerprintLevel){.path_length = 1, .next_index = 0};
    if (!files->open_dir(files->context, work->path)) {
        return walk.print;
    }
    files->close_dir(files->context);
    for (;;) {
        FingerprintLevel *level = &work->levels[walk.depth];
        if (!read_entry(&walk, level->next_index)) {
            if (walk.depth == 0) {
                break;
            }
            leave(&walk);
            continue;
        }
        level->next_index++;
        const FingerprintEntry *entry = &work->entry;
        if (entry->name[0] == '.') {
            continue;
        }
        mix_entry(&walk, entry);
        if (entry->directory && !enter(&walk, entry->name)) {
            return walk.print;
        }
    }
    walk.print.complete = true;
    return walk.print;
}

bool music_fingerprints_match(MusicFingerprint left, MusicFingerprint right)
{
    return left.complete && right.complete && left.first == right.first &&
           left.second == right.second && left.entries == right.entries;
}
