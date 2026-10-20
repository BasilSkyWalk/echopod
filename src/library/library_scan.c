#include "library/library_scan.h"

#include <stddef.h>
#include <stdint.h>

#include "library/playlist_reader.h"
#include "library/track_fields.h"
#include "library/track_tags.h"

enum {
    LISTING_SHARE = 8,
    MINIMUM_LISTING = 16 * 1024,
    MAXIMUM_LISTING = 96 * 1024,
    ALIGNMENT = 8
};

typedef struct Level {
    uint32_t first;
    uint32_t count;
    uint32_t next;
    uint32_t path_length;
} Level;

typedef struct ScanState {
    LibraryBuilder builder;
    TrackTags tags;
    TrackFields fields;
    ScanEntry entry;
    Level levels[SCAN_MAX_DEPTH];
    uint32_t depth;
    char path[SCAN_PATH_CAPACITY + 2];
    uint32_t path_length;
    uint8_t *listing;
    uint32_t listing_size;
    uint32_t listing_used;
    uint32_t playlist_floor;
    uint32_t playlist_count;
    PlaylistReader playlist;
    char entry_text[PLAYLIST_ENTRY_CAPACITY];
    char playlist_name[SCAN_NAME_CAPACITY];
} ScanState;

typedef struct Listing {
    uint32_t *offsets;
    uint32_t *scratch;
} Listing;

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static const char *entry_name(const ScanState *state, uint32_t offset)
{
    return (const char *)state->listing + offset + 1;
}

static bool entry_is_directory(const ScanState *state, uint32_t offset)
{
    return state->listing[offset] != 0;
}

static int compare_names(const ScanState *state, uint32_t left, uint32_t right)
{
    const unsigned char *a = (const unsigned char *)entry_name(state, left);
    const unsigned char *b = (const unsigned char *)entry_name(state, right);
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)*a - (int)*b;
}

static void merge_names(const ScanState *state, const uint32_t *from, uint32_t *to, uint32_t start,
                        uint32_t middle, uint32_t end)
{
    uint32_t left = start;
    uint32_t right = middle;
    for (uint32_t out = start; out < end; out++) {
        bool take_left =
            left < middle && (right >= end || compare_names(state, from[left], from[right]) <= 0);
        to[out] = take_left ? from[left++] : from[right++];
    }
}

static void sort_names(const ScanState *state, uint32_t *items, uint32_t *scratch, uint32_t count)
{
    uint32_t *from = items;
    uint32_t *to = scratch;
    for (uint32_t width = 1; width < count; width *= 2) {
        for (uint32_t start = 0; start < count; start += 2 * width) {
            uint32_t middle = start + width < count ? start + width : count;
            uint32_t end = start + 2 * width < count ? start + 2 * width : count;
            merge_names(state, from, to, start, middle, end);
        }
        uint32_t *swap = from;
        from = to;
        to = swap;
    }
    for (uint32_t index = 0; from != items && index < count; index++) {
        items[index] = from[index];
    }
}

static bool store_entry(ScanState *state, const ScanEntry *entry)
{
    uint32_t length = text_length(entry->name);
    if (length + 2 > state->playlist_floor - state->listing_used) {
        return false;
    }
    uint8_t *record = state->listing + state->listing_used;
    record[0] = entry->directory ? 1 : 0;
    for (uint32_t index = 0; index <= length; index++) {
        record[1 + index] = (uint8_t)entry->name[index];
    }
    state->listing_used += length + 2;
    return true;
}

static uint32_t align4(uint32_t value)
{
    return (value + 3) & ~3u;
}

static ScanResult index_listing(ScanState *state, uint32_t start, uint32_t count, Level *level)
{
    uint32_t table = align4(state->listing_used);
    if (table > state->playlist_floor || (uint64_t)count * 8 > state->playlist_floor - table) {
        return SCAN_FOLDER_TOO_BIG;
    }
    uint32_t *offsets = (uint32_t *)(void *)(state->listing + table);
    uint32_t cursor = start;
    for (uint32_t index = 0; index < count; index++) {
        offsets[index] = cursor;
        cursor += 2 + text_length(entry_name(state, cursor));
    }
    sort_names(state, offsets, offsets + count, count);
    state->listing_used = table + count * 4;
    *level = (Level){.first = table, .count = count, .next = 0, .path_length = state->path_length};
    return SCAN_OK;
}

static ScanResult read_listing(ScanState *state, const ScanFileSystem *files, Level *level)
{
    if (!files->open_dir(files->context, state->path)) {
        return SCAN_NO_MUSIC_ROOT;
    }
    uint32_t start = state->listing_used;
    uint32_t count = 0;
    bool fits = true;
    while (fits && files->next_entry(files->context, &state->entry)) {
        if (state->entry.name[0] == '.') {
            continue;
        }
        fits = store_entry(state, &state->entry);
        count++;
    }
    files->close_dir(files->context);
    return fits ? index_listing(state, start, count, level) : SCAN_FOLDER_TOO_BIG;
}

static bool append_path(ScanState *state, const char *name, bool directory)
{
    uint32_t length = text_length(name);
    if (state->path_length + length + 2 > SCAN_PATH_CAPACITY) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        state->path[state->path_length + index] = name[index];
    }
    state->path_length += length;
    if (directory) {
        state->path[state->path_length++] = '/';
    }
    state->path[state->path_length] = '\0';
    return true;
}

static void truncate_path(ScanState *state, uint32_t length)
{
    state->path_length = length;
    state->path[length] = '\0';
}

static ScanResult builder_result(BuilderResult result)
{
    switch (result) {
    case BUILDER_OK:
        return SCAN_OK;
    case BUILDER_OUT_OF_MEMORY:
        return SCAN_OUT_OF_MEMORY;
    case BUILDER_TOO_MANY:
        return SCAN_TOO_MANY;
    case BUILDER_WRITE_FAILED:
        return SCAN_WRITE_FAILED;
    case BUILDER_NO_ROOM:
        return SCAN_NO_ROOM;
    }
    return SCAN_WRITE_FAILED;
}

static ScanResult add_file(ScanState *state, const ScanJob *job, ScanStats *stats)
{
    ByteSource source;
    uint32_t size = 0;
    stats->files++;
    bool read = job->files.open_file(job->files.context, state->path, &source, &size);
    if (read) {
        TagFormat format = track_tags_format_for_name(state->path);
        read = track_tags_read(source, size, format, &state->tags);
        job->files.close_file(job->files.context);
    }
    ScanResult result = SCAN_OK;
    if (read) {
        track_fields_from_tags(state->path + 1, &state->tags, &state->fields);
        result = builder_result(library_builder_add_track(&state->builder, &state->fields.track));
        stats->tracks += result == SCAN_OK;
    } else {
        stats->skipped++;
    }
    if (job->progress.report != NULL) {
        job->progress.report(job->progress.context, stats->files, stats->tracks);
    }
    return result;
}

static bool is_playlist_name(const char *name)
{
    uint32_t length = text_length(name);
    const char *dot = NULL;
    for (uint32_t index = 1; index < length; index++) {
        dot = name[index] == '.' ? name + index : dot;
    }
    if (dot == NULL) {
        return false;
    }
    char lower[6] = {0};
    uint32_t count = 0;
    for (const char *cursor = dot + 1; *cursor != '\0' && count < 5; cursor++) {
        lower[count++] = (char)(*cursor >= 'A' && *cursor <= 'Z' ? *cursor - 'A' + 'a' : *cursor);
    }
    bool m3u = count == 3 && lower[0] == 'm' && lower[1] == '3' && lower[2] == 'u';
    bool m3u8 =
        count == 4 && lower[0] == 'm' && lower[1] == '3' && lower[2] == 'u' && lower[3] == '8';
    return (m3u || m3u8) && dot[count + 1] == '\0';
}

static ScanResult note_playlist(ScanState *state, const char *name)
{
    uint32_t parent = state->path_length;
    uint32_t length = parent + text_length(name);
    uint32_t need = align4(length + 1 + 4);
    if (need > state->playlist_floor - align4(state->listing_used)) {
        return SCAN_FOLDER_TOO_BIG;
    }
    state->playlist_floor -= need;
    uint8_t *record = state->listing + state->playlist_floor;
    record[0] = (uint8_t)(length & 0xFF);
    record[1] = (uint8_t)(length >> 8);
    record[2] = (uint8_t)(parent & 0xFF);
    record[3] = (uint8_t)(parent >> 8);
    for (uint32_t index = 0; index < parent; index++) {
        record[4 + index] = (uint8_t)state->path[index];
    }
    for (uint32_t index = parent; index < length; index++) {
        record[4 + index] = (uint8_t)name[index - parent];
    }
    record[4 + length] = 0;
    state->playlist_count++;
    return SCAN_OK;
}

static ScanResult visit_entry(ScanState *state, const ScanJob *job, ScanStats *stats,
                              uint32_t offset)
{
    const char *name = entry_name(state, offset);
    bool directory = entry_is_directory(state, offset);
    if (!directory && is_playlist_name(name)) {
        return note_playlist(state, name);
    }
    if (!directory && track_tags_format_for_name(name) == TAG_FORMAT_UNKNOWN) {
        return SCAN_OK;
    }
    if (!append_path(state, name, directory)) {
        return SCAN_PATH_TOO_LONG;
    }
    if (!directory) {
        ScanResult result = add_file(state, job, stats);
        truncate_path(state, state->levels[state->depth - 1].path_length);
        return result;
    }
    if (state->depth >= SCAN_MAX_DEPTH) {
        return SCAN_PATH_TOO_LONG;
    }
    ScanResult result = read_listing(state, &job->files, &state->levels[state->depth]);
    state->depth += result == SCAN_OK;
    return result == SCAN_NO_MUSIC_ROOT ? SCAN_OK : result;
}

static ScanResult walk(ScanState *state, const ScanJob *job, ScanStats *stats)
{
    ScanResult result = read_listing(state, &job->files, &state->levels[0]);
    state->depth = result == SCAN_OK ? 1 : 0;
    while (result == SCAN_OK && state->depth > 0) {
        Level *level = &state->levels[state->depth - 1];
        if (level->next >= level->count) {
            state->listing_used = level->first;
            state->depth--;
            truncate_path(state,
                          state->depth > 0 ? state->levels[state->depth - 1].path_length : 1);
            continue;
        }
        const uint32_t *offsets = (const uint32_t *)(const void *)(state->listing + level->first);
        result = visit_entry(state, job, stats, offsets[level->next++]);
    }
    return result;
}

static bool split_memory(ScanState **state, const ScanJob *job)
{
    uintptr_t start = (uintptr_t)job->memory;
    uint32_t skip = (uint32_t)((ALIGNMENT - start % ALIGNMENT) % ALIGNMENT);
    uint32_t header = (uint32_t)((sizeof(ScanState) + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1));
    bool separate = job->listing != NULL && job->listing_size >= MINIMUM_LISTING;
    if (job->size < skip + header + MINIMUM_LISTING * (separate ? 1u : 2u)) {
        return false;
    }
    uint32_t size = job->size - skip;
    uint32_t listing = separate ? job->listing_size : size / LISTING_SHARE;
    listing = listing < MINIMUM_LISTING ? MINIMUM_LISTING : listing;
    listing = listing > MAXIMUM_LISTING ? MAXIMUM_LISTING : listing;
    uint8_t *memory = (uint8_t *)job->memory + skip;
    uint32_t taken = separate ? 0 : listing;
    *state = (ScanState *)(void *)memory;
    (*state)->listing = separate ? (uint8_t *)job->listing : memory + header;
    (*state)->listing_size = listing;
    (*state)->listing_used = 0;
    (*state)->playlist_floor = listing;
    (*state)->playlist_count = 0;
    library_builder_init(&(*state)->builder, memory + header + taken, size - header - taken);
    return true;
}

static void name_and_folder(ScanState *state, const uint8_t *record, char *folder)
{
    uint32_t length = (uint32_t)record[0] | (uint32_t)record[1] << 8;
    uint32_t parent = (uint32_t)record[2] | (uint32_t)record[3] << 8;
    const char *path = (const char *)record + 4;
    uint32_t folder_length = parent > 1 ? parent - 2 : 0;
    for (uint32_t index = 0; index < folder_length; index++) {
        folder[index] = path[index + 1];
    }
    folder[folder_length] = '\0';
    uint32_t stem_end = length;
    for (uint32_t index = parent + 1; index < length; index++) {
        stem_end = path[index] == '.' ? index : stem_end;
    }
    uint32_t count = 0;
    for (uint32_t index = parent; index < stem_end; index++) {
        state->playlist_name[count++] = path[index];
    }
    state->playlist_name[count] = '\0';
}

static ScanResult read_playlist(ScanState *state, const ScanJob *job, const uint8_t *record)
{
    ByteSource source;
    uint32_t size = 0;
    const char *path = (const char *)record + 4;
    if (!job->files.open_file(job->files.context, path, &source, &size)) {
        return SCAN_OK;
    }
    name_and_folder(state, record, state->path);
    ScanResult result = builder_result(
        library_builder_begin_playlist(&state->builder, state->playlist_name, state->path));
    playlist_reader_open(&state->playlist, source, size);
    while (result == SCAN_OK &&
           playlist_reader_next(&state->playlist, state->entry_text, sizeof state->entry_text)) {
        result = builder_result(library_builder_playlist_entry(&state->builder, state->entry_text));
    }
    job->files.close_file(job->files.context);
    return result;
}

static uint32_t record_length(const ScanState *state, uint32_t offset)
{
    uint32_t length = (uint32_t)state->listing[offset] | (uint32_t)state->listing[offset + 1] << 8;
    return align4(length + 1 + 4);
}

static uint32_t noted_playlist(const ScanState *state, uint32_t index)
{
    uint32_t offset = state->playlist_floor;
    for (uint32_t skip = 0; skip + 1 + index < state->playlist_count; skip++) {
        offset += record_length(state, offset);
    }
    return offset;
}

static ScanResult read_playlists(ScanState *state, const ScanJob *job)
{
    ScanResult result = builder_result(library_builder_prepare(&state->builder));
    for (uint32_t index = 0; result == SCAN_OK && index < state->playlist_count; index++) {
        result = read_playlist(state, job, state->listing + noted_playlist(state, index));
    }
    return result;
}

ScanResult library_scan(const ScanJob *job, ScanStats *stats)
{
    *stats = (ScanStats){.files = 0, .tracks = 0, .skipped = 0, .length = 0};
    ScanState *state = NULL;
    if (!split_memory(&state, job)) {
        return SCAN_OUT_OF_MEMORY;
    }
    state->path[0] = '/';
    truncate_path(state, 1);
    ScanResult result = walk(state, job, stats);
    if (result == SCAN_OK) {
        result = read_playlists(state, job);
    }
    if (result != SCAN_OK) {
        return result;
    }
    return builder_result(
        library_builder_finish(&state->builder, job->sink, job->capacity, &stats->length));
}
