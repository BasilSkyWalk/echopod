#include "library/cover_build.h"

#include <stddef.h>

#include "image/cover_image.h"
#include "library/covers.h"
#include "library/library.h"
#include "library/track_tags.h"

enum {
    ALIGNMENT = 8,
    MAX_FOLDERS = 16,
    STEM_COUNT = 4,
    SUFFIX_COUNT = 3,
    MATCH_CAPACITY = 16,
    FULL_BYTES = COVER_FULL_PIXELS * 2,
    HALF_BYTES = COVER_HALF_PIXELS * 2,
    INDEX_CHUNK = 64
};

static const char *const STEMS[STEM_COUNT] = {"cover", "folder", "front", "albumart"};
static const char *const SUFFIXES[SUFFIX_COUNT] = {".jpg", ".jpeg", ".png"};

typedef struct CoverEntry {
    uint32_t key;
    uint32_t offset;
} CoverEntry;

typedef struct CoverState {
    Library library;
    TrackTags tags;
    ScanEntry entry;
    char path[SCAN_PATH_CAPACITY + 2];
    char matches[STEM_COUNT][MATCH_CAPACITY];
    uint16_t full[COVER_FULL_PIXELS];
    uint16_t half[COVER_HALF_PIXELS];
    uint16_t folders[MAX_FOLDERS];
    uint32_t folder_count;
    CoverEntry *index;
    uint32_t count;
    uint32_t limit;
    uint32_t records_start;
    uint8_t *decode_memory;
    uint32_t decode_size;
} CoverState;

static uint8_t *take(uint8_t **cursor, uint8_t *end, uint32_t bytes)
{
    uintptr_t address = (uintptr_t)*cursor;
    uint8_t *start = *cursor + (ALIGNMENT - address % ALIGNMENT) % ALIGNMENT;
    if (start > end || (uint32_t)(end - start) < bytes) {
        return NULL;
    }
    *cursor = start + bytes;
    return start;
}

static uint32_t cover_limit(const CoverJob *job, uint32_t albums)
{
    uint32_t per_cover = COVERS_ENTRY_SIZE + COVER_BUILD_RECORD_BYTES;
    uint32_t room = job->capacity > COVERS_HEADER_SIZE ? job->capacity - COVERS_HEADER_SIZE : 0;
    uint32_t limit = room / per_cover;
    limit = limit < albums ? limit : albums;
    return limit < COVER_BUILD_MAX_COVERS ? limit : COVER_BUILD_MAX_COVERS;
}

static CoverState *prepare(const CoverJob *job, uint32_t albums)
{
    uint8_t *cursor = job->memory;
    uint8_t *end = cursor + job->size;
    CoverState *state = (CoverState *)take(&cursor, end, sizeof(CoverState));
    if (state == NULL) {
        return NULL;
    }
    state->limit = cover_limit(job, albums);
    state->index = (CoverEntry *)take(&cursor, end, state->limit * sizeof(CoverEntry) + 1);
    if (state->index == NULL) {
        return NULL;
    }
    state->count = 0;
    state->records_start = COVERS_HEADER_SIZE + state->limit * COVERS_ENTRY_SIZE;
    state->decode_memory = cursor;
    state->decode_size = (uint32_t)(end - cursor);
    return state;
}

static bool write_bytes(const CoverJob *job, uint32_t offset, const void *bytes, uint32_t length)
{
    return job->sink.write(job->sink.context, offset, bytes, length);
}

static void put16(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t value)
{
    put16(bytes, value);
    put16(bytes + 2, value >> 16);
}

static bool has_key(const CoverState *state, uint32_t key)
{
    for (uint32_t index = 0; index < state->count; index++) {
        if (state->index[index].key == key) {
            return true;
        }
    }
    return false;
}

static bool read_track_path(CoverState *state, uint32_t track_index, LibraryTrack *track)
{
    state->path[0] = '/';
    return library_read_track(&state->library, track_index, track) &&
           library_read_string(&state->library, track->path, state->path + 1,
                               sizeof state->path - 1);
}

static bool decode_from(CoverState *state, ByteSource source, uint32_t offset, uint32_t length)
{
    ImageSource image = {source.read, source.context, offset, length};
    return cover_image_decode(image, state->decode_memory, state->decode_size, state->full,
                              state->half) == COVER_IMAGE_OK;
}

static bool embedded_cover(CoverState *state, const CoverJob *job, uint32_t track_index,
                           bool *decoded)
{
    LibraryTrack track;
    ByteSource source;
    uint32_t size = 0;
    if (!read_track_path(state, track_index, &track) ||
        !job->files.open_file(job->files.context, state->path, &source, &size)) {
        return false;
    }
    TagFormat format = track_tags_format_for_name(state->path);
    bool tagged = track_tags_read(source, size, format, &state->tags);
    TagPicture picture = state->tags.picture;
    bool found = tagged && picture.kind == TAG_PICTURE_RAW && picture.length > 0;
    *decoded = found && decode_from(state, source, picture.offset, picture.length);
    job->files.close_file(job->files.context);
    return found;
}

static char lower(char character)
{
    return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static bool is_name(const char *name, const char *stem, const char *suffix)
{
    while (*stem != '\0') {
        if (lower(*name++) != *stem++) {
            return false;
        }
    }
    while (*suffix != '\0') {
        if (lower(*name++) != *suffix++) {
            return false;
        }
    }
    return *name == '\0';
}

static bool before(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return (uint8_t)*left < (uint8_t)*right;
}

static void copy_name(char *target, const char *name)
{
    uint32_t index = 0;
    while (name[index] != '\0' && index + 1 < MATCH_CAPACITY) {
        target[index] = name[index];
        index++;
    }
    target[index] = '\0';
}

static void consider(CoverState *state, const char *name)
{
    for (uint32_t stem = 0; stem < STEM_COUNT; stem++) {
        for (uint32_t suffix = 0; suffix < SUFFIX_COUNT; suffix++) {
            if (is_name(name, STEMS[stem], SUFFIXES[suffix]) &&
                (state->matches[stem][0] == '\0' || before(name, state->matches[stem]))) {
                copy_name(state->matches[stem], name);
            }
        }
    }
}

static uint32_t folder_length(const char *path)
{
    uint32_t slash = 0;
    for (uint32_t index = 0; path[index] != '\0'; index++) {
        slash = path[index] == '/' ? index : slash;
    }
    return slash;
}

static const char *best_match(CoverState *state, const CoverJob *job)
{
    uint32_t length = folder_length(state->path);
    state->path[length > 0 ? length : 1] = '\0';
    for (uint32_t stem = 0; stem < STEM_COUNT; stem++) {
        state->matches[stem][0] = '\0';
    }
    if (!job->files.open_dir(job->files.context, state->path)) {
        return NULL;
    }
    while (job->files.next_entry(job->files.context, &state->entry)) {
        if (!state->entry.directory) {
            consider(state, state->entry.name);
        }
    }
    job->files.close_dir(job->files.context);
    for (uint32_t stem = 0; stem < STEM_COUNT; stem++) {
        if (state->matches[stem][0] != '\0') {
            return state->matches[stem];
        }
    }
    return NULL;
}

static bool append_name(CoverState *state, const char *name)
{
    uint32_t length = 0;
    while (state->path[length] != '\0') {
        length++;
    }
    if (length > 1) {
        state->path[length++] = '/';
    }
    for (uint32_t index = 0; name[index] != '\0'; index++) {
        if (length + 1 >= sizeof state->path) {
            return false;
        }
        state->path[length++] = name[index];
    }
    state->path[length] = '\0';
    return true;
}

static bool decode_file(CoverState *state, const CoverJob *job)
{
    ByteSource source;
    uint32_t size = 0;
    if (!job->files.open_file(job->files.context, state->path, &source, &size)) {
        return false;
    }
    bool decoded = decode_from(state, source, 0, size);
    job->files.close_file(job->files.context);
    return decoded;
}

static bool folder_tried(CoverState *state, uint16_t folder)
{
    for (uint32_t index = 0; index < state->folder_count; index++) {
        if (state->folders[index] == folder) {
            return true;
        }
    }
    if (state->folder_count < MAX_FOLDERS) {
        state->folders[state->folder_count++] = folder;
    }
    return false;
}

static bool folder_cover(CoverState *state, const CoverJob *job, const LibraryAlbum *album)
{
    char match[MATCH_CAPACITY];
    state->folder_count = 0;
    for (uint32_t track = 0; track < album->track_count; track++) {
        LibraryTrack info;
        if (!read_track_path(state, album->first_track + track, &info) ||
            folder_tried(state, info.folder)) {
            continue;
        }
        const char *name = best_match(state, job);
        if (name != NULL) {
            copy_name(match, name);
            return append_name(state, match) && decode_file(state, job);
        }
    }
    return false;
}

static bool album_cover(CoverState *state, const CoverJob *job, const LibraryAlbum *album)
{
    bool decoded = false;
    if (album->track_count == 0) {
        return false;
    }
    if (embedded_cover(state, job, album->first_track, &decoded)) {
        return decoded;
    }
    return folder_cover(state, job, album);
}

static void pack_pixels(uint16_t *pixels, uint32_t count)
{
    uint8_t *bytes = (uint8_t *)pixels;
    for (uint32_t index = 0; index < count; index++) {
        uint16_t value = pixels[index];
        put16(bytes + index * 2, value);
    }
}

static bool store_cover(CoverState *state, const CoverJob *job, uint32_t key)
{
    uint32_t offset = state->records_start + state->count * COVER_BUILD_RECORD_BYTES;
    pack_pixels(state->full, COVER_FULL_PIXELS);
    pack_pixels(state->half, COVER_HALF_PIXELS);
    if (!write_bytes(job, offset, state->full, FULL_BYTES) ||
        !write_bytes(job, offset + FULL_BYTES, state->half, HALF_BYTES)) {
        return false;
    }
    state->index[state->count++] = (CoverEntry){key, offset};
    return true;
}

static void sort_index(CoverEntry *entries, uint32_t count)
{
    for (uint32_t index = 1; index < count; index++) {
        CoverEntry entry = entries[index];
        uint32_t position = index;
        while (position > 0 && entries[position - 1].key > entry.key) {
            entries[position] = entries[position - 1];
            position--;
        }
        entries[position] = entry;
    }
}

static bool write_index(const CoverState *state, const CoverJob *job)
{
    uint8_t chunk[INDEX_CHUNK * COVERS_ENTRY_SIZE];
    for (uint32_t first = 0; first < state->count; first += INDEX_CHUNK) {
        uint32_t count = state->count - first < INDEX_CHUNK ? state->count - first : INDEX_CHUNK;
        for (uint32_t index = 0; index < count; index++) {
            put32(chunk + index * COVERS_ENTRY_SIZE, state->index[first + index].key);
            put32(chunk + index * COVERS_ENTRY_SIZE + 4, state->index[first + index].offset);
        }
        uint32_t offset = COVERS_HEADER_SIZE + first * COVERS_ENTRY_SIZE;
        if (!write_bytes(job, offset, chunk, count * COVERS_ENTRY_SIZE)) {
            return false;
        }
    }
    return true;
}

static bool write_header(const CoverJob *job, uint32_t count, bool valid)
{
    uint8_t header[COVERS_HEADER_SIZE] = {0};
    if (valid) {
        header[0] = 'E';
        header[1] = 'M';
        header[2] = 'C';
        header[3] = 'V';
        put16(header + 4, COVERS_VERSION);
        put16(header + 6, count);
        put16(header + 8, COVER_FULL_SIZE);
        put16(header + 10, COVER_HALF_SIZE);
    }
    return write_bytes(job, 0, header, sizeof header);
}

static void report(const CoverJob *job, uint32_t done, uint32_t total)
{
    if (job->progress.report != NULL) {
        job->progress.report(job->progress.context, done, total);
    }
}

static ScanResult build_albums(CoverState *state, const CoverJob *job, CoverStats *stats)
{
    for (uint32_t album_index = 0; album_index < stats->albums; album_index++) {
        LibraryAlbum album;
        report(job, album_index, stats->albums);
        uint32_t key = library_album_key(&state->library, album_index);
        if (!library_read_album(&state->library, album_index, &album) || has_key(state, key)) {
            continue;
        }
        if (state->count == state->limit || !album_cover(state, job, &album)) {
            stats->missing++;
        } else if (!store_cover(state, job, key)) {
            return SCAN_WRITE_FAILED;
        }
    }
    report(job, stats->albums, stats->albums);
    return SCAN_OK;
}

ScanResult cover_build(const CoverJob *job, CoverStats *stats)
{
    *stats = (CoverStats){.albums = 0, .covers = 0, .missing = 0};
    Library probe;
    if (library_open(&probe, job->library, job->library_size) != LIBRARY_OK) {
        return SCAN_NO_MUSIC_ROOT;
    }
    stats->albums = library_count(&probe, LIBRARY_ALBUMS);
    CoverState *state = prepare(job, stats->albums);
    if (state == NULL) {
        return SCAN_OUT_OF_MEMORY;
    }
    if (state->limit == 0 && stats->albums > 0) {
        return SCAN_NO_ROOM;
    }
    state->library = probe;
    if (!write_header(job, 0, false)) {
        return SCAN_WRITE_FAILED;
    }
    ScanResult result = build_albums(state, job, stats);
    sort_index(state->index, state->count);
    stats->covers = state->count;
    if (result == SCAN_OK && (!write_index(state, job) || !write_header(job, state->count, true))) {
        result = SCAN_WRITE_FAILED;
    }
    return result;
}
