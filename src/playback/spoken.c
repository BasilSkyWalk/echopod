#include "playback/spoken.h"

#include <stddef.h>

#include "core/hash.h"

enum {
    NAME_CAPACITY = 64,
    PATH_CAPACITY = 520,
    ROOT_FOLDER = 0
};

static char lower(char letter)
{
    return letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter;
}

static bool name_matches(const char *name, size_t length, const char *expected)
{
    size_t index = 0;
    for (; index < length && expected[index] != '\0'; index++) {
        if (lower(name[index]) != expected[index]) {
            return false;
        }
    }
    return index == length && expected[index] == '\0';
}

static SpokenKind kind_of_name(const char *name, size_t length)
{
    if (name_matches(name, length, "audiobooks") || name_matches(name, length, "audiobook")) {
        return SPOKEN_AUDIOBOOK;
    }
    if (name_matches(name, length, "podcasts") || name_matches(name, length, "podcast")) {
        return SPOKEN_PODCAST;
    }
    return SPOKEN_NONE;
}

static const char *relative(const char *path)
{
    while (*path == '/') {
        path++;
    }
    return path;
}

static size_t component_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0' && text[length] != '/') {
        length++;
    }
    return length;
}

static size_t text_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static size_t last_slash(const char *text)
{
    size_t found = text_length(text);
    for (size_t index = 0; text[index] != '\0'; index++) {
        if (text[index] == '/') {
            found = index;
        }
    }
    return found;
}

static uint32_t slash_count(const char *text)
{
    uint32_t count = 0;
    for (size_t index = 0; text[index] != '\0'; index++) {
        count += text[index] == '/' ? 1u : 0u;
    }
    return count;
}

SpokenKind spoken_kind_of(const char *path)
{
    const char *start = relative(path);
    size_t length = component_length(start);
    if (start[length] != '/') {
        return SPOKEN_NONE;
    }
    return kind_of_name(start, length);
}

uint32_t spoken_track_key(const char *path)
{
    const char *start = relative(path);
    return hash_update(HASH_START, start, text_length(start));
}

uint32_t spoken_bookmark_key(const char *path)
{
    const char *start = relative(path);
    if (spoken_kind_of(start) != SPOKEN_AUDIOBOOK || slash_count(start) < 2) {
        return spoken_track_key(start);
    }
    return hash_update(HASH_START, start, last_slash(start));
}

bool spoken_near_end(uint32_t position_ms, uint32_t duration_ms)
{
    if (duration_ms == 0) {
        return false;
    }
    uint32_t margin = duration_ms / SPOKEN_FINISH_DIVISOR;
    margin = margin < SPOKEN_FINISH_MARGIN_MS ? margin : SPOKEN_FINISH_MARGIN_MS;
    return position_ms + margin >= duration_ms;
}

static SpokenKind folder_kind(const Library *library, uint16_t folder)
{
    LibraryFolder record;
    char name[NAME_CAPACITY];
    if (!library_read_folder(library, folder, &record) ||
        !library_read_string(library, record.name, name, sizeof name)) {
        return SPOKEN_NONE;
    }
    return kind_of_name(name, text_length(name));
}

bool spoken_shelf_folder(const Library *library, SpokenKind kind, uint16_t *folder)
{
    LibraryFolder root;
    if (kind == SPOKEN_NONE || !library_read_folder(library, ROOT_FOLDER, &root)) {
        return false;
    }
    for (uint32_t index = 0; index < root.subfolder_count; index++) {
        uint16_t child = 0;
        if (library_read_index(library, LIBRARY_LINKS, root.first_link + index, &child) &&
            folder_kind(library, child) == kind) {
            *folder = child;
            return true;
        }
    }
    return false;
}

bool spoken_chapter_track(const Library *library, uint16_t folder, uint16_t chapter,
                          uint16_t *track)
{
    LibraryFolder record;
    if (!library_read_folder(library, folder, &record) || chapter >= record.track_count) {
        return false;
    }
    uint32_t link = record.first_link + record.subfolder_count + chapter;
    return library_read_index(library, LIBRARY_LINKS, link, track);
}

bool spoken_folder_key(const Library *library, uint16_t folder, uint32_t *key)
{
    uint16_t track = 0;
    LibraryTrack record;
    char path[PATH_CAPACITY];
    if (!spoken_chapter_track(library, folder, 0, &track) ||
        !library_read_track(library, track, &record) ||
        !library_read_string(library, record.path, path, sizeof path)) {
        return false;
    }
    *key = hash_update(HASH_START, relative(path), last_slash(relative(path)));
    return true;
}

bool spoken_chapter_of(const Library *library, uint16_t track, uint16_t *chapter)
{
    LibraryTrack record;
    LibraryFolder folder;
    if (!library_read_track(library, track, &record) ||
        !library_read_folder(library, record.folder, &folder)) {
        return false;
    }
    for (uint16_t index = 0; index < folder.track_count; index++) {
        uint16_t candidate = 0;
        if (spoken_chapter_track(library, record.folder, index, &candidate) && candidate == track) {
            *chapter = index;
            return true;
        }
    }
    return false;
}
