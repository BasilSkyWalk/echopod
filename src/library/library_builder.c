#include "library/library_builder.h"

#include <stddef.h>

#include "builder_internal.h"
#include "core/hash.h"
#include "library/spoken_path.h"

enum {
    ROW_BUFFER = 64 * BUILDER_ROW_SIZE,
    FOLDER_BUFFER = 64 * BUILDER_FOLDER_SIZE,
    HEAP_BUFFER = 4096,
    ROW_CACHE_BLOCK = 4096,
    ROW_CACHE_BLOCKS = 4,
    FOLDER_CACHE_BLOCK = 1024,
    FOLDER_CACHE_BLOCKS = 4,
    HEAP_CACHE_BLOCK = 4096,
    HEAP_CACHE_BLOCKS = 4,
    PLAYLIST_NAME_BYTES = BUILDER_MAX_PLAYLISTS * BUILDER_TEXT_CAPACITY,
    PLAYLIST_LINK_BYTES = 2 * 1024 * 1024,
    LINK_BUFFER = 1024,
    NAME_BUFFER = 1024,
    FOLDER_ENTRY_BYTES = 14,
    MINIMUM_FOLDERS = 64
};

#define SECOND_SEED (HASH_START ^ UINT32_C(0x5BD1E995))

static BuilderResult outcome(const BuilderState *state)
{
    if (state->store.io_failed) {
        return BUILDER_WRITE_FAILED;
    }
    if (state->store.failed) {
        return BUILDER_NO_ROOM;
    }
    if (state->arena.failed) {
        return BUILDER_OUT_OF_MEMORY;
    }
    return state->error;
}

static BuilderState *state_of(const LibraryBuilder *builder)
{
    return builder->state;
}

static bool write_folder_row(BuilderState *state, uint32_t name, uint32_t length, uint32_t parent)
{
    uint8_t bytes[BUILDER_FOLDER_SIZE];
    FolderRow row = {.name = name, .length = (uint16_t)length, .parent = (uint16_t)parent};
    builder_folder_encode(&row, bytes);
    return out_put(&state->scan->folders, bytes, sizeof bytes);
}

static bool open_folder_table(BuilderState *state)
{
    ScanTime *scan = state->scan;
    uint32_t budget = arena_left(&state->arena) / 4 * 3;
    uint32_t slots = 16;
    uint32_t capacity = 0;
    for (uint32_t candidate = 16; candidate <= BUILDER_MAX_INDEX + 1u; candidate *= 2) {
        uint32_t loaded = candidate / 4 * 3;
        uint32_t affordable = budget > candidate * 2 ? (budget - candidate * 2) / 10 : 0;
        uint32_t usable = loaded < affordable ? loaded : affordable;
        if (usable > capacity) {
            capacity = usable;
            slots = candidate;
        }
    }
    capacity = capacity < BUILDER_MAX_INDEX ? capacity : BUILDER_MAX_INDEX;
    if (capacity < MINIMUM_FOLDERS) {
        state->arena.failed = true;
        return false;
    }
    scan->folder_first = arena_take(&state->arena, capacity * 4);
    scan->folder_second = arena_take(&state->arena, capacity * 4);
    scan->folder_parent = arena_take(&state->arena, capacity * 2);
    scan->folder_slots = arena_take(&state->arena, slots * 2);
    scan->folder_capacity = capacity;
    scan->folder_mask = slots - 1;
    if (state->arena.failed) {
        return false;
    }
    for (uint32_t slot = 0; slot < slots; slot++) {
        scan->folder_slots[slot] = 0;
    }
    return true;
}

static uint32_t folder_hash(uint32_t seed, uint32_t parent, const char *name, uint32_t length)
{
    uint8_t prefix[2] = {(uint8_t)parent, (uint8_t)(parent >> 8)};
    return hash_update(hash_update(seed, prefix, sizeof prefix), name, length);
}

static uint32_t find_folder(const ScanTime *scan, uint32_t parent, uint32_t first, uint32_t second)
{
    for (uint32_t probe = 0; probe <= scan->folder_mask; probe++) {
        uint32_t slot = scan->folder_slots[(first + probe) & scan->folder_mask];
        if (slot == 0) {
            return BUILDER_UNSET;
        }
        uint32_t node = slot - 1u;
        if (scan->folder_parent[node] == parent && scan->folder_first[node] == first &&
            scan->folder_second[node] == second) {
            return node;
        }
    }
    return BUILDER_UNSET;
}

static void remember_folder(ScanTime *scan, uint32_t node, uint32_t parent, uint32_t first,
                            uint32_t second)
{
    scan->folder_first[node] = first;
    scan->folder_second[node] = second;
    scan->folder_parent[node] = (uint16_t)parent;
    for (uint32_t probe = 0;; probe++) {
        uint16_t *slot = &scan->folder_slots[(first + probe) & scan->folder_mask];
        if (*slot == 0) {
            *slot = (uint16_t)(node + 1);
            return;
        }
    }
}

void library_builder_init(LibraryBuilder *builder, void *memory, uint32_t size,
                          BuilderScratch scratch)
{
    Arena arena;
    arena_init(&arena, memory, size);
    BuilderState *state = arena_take(&arena, (uint32_t)sizeof(BuilderState));
    builder->state = state;
    if (state == NULL) {
        return;
    }
    *state = (BuilderState){.arena = arena, .error = BUILDER_OK, .prepare_done = false};
    store_init(&state->store, scratch, 0);
    state->rows = store_take(&state->store, BUILDER_MAX_INDEX * BUILDER_ROW_SIZE);
    state->folders = store_take(&state->store, BUILDER_MAX_INDEX * BUILDER_FOLDER_SIZE);
    state->heap_start = state->store.end;
    ScanTime *scan = arena_take(&state->arena, (uint32_t)sizeof(ScanTime));
    uint8_t *row_buffer = arena_take(&state->arena, ROW_BUFFER);
    uint8_t *folder_buffer = arena_take(&state->arena, FOLDER_BUFFER);
    uint8_t *heap_buffer = arena_take(&state->arena, HEAP_BUFFER);
    state->scan = scan;
    if (state->arena.failed || state->store.failed) {
        return;
    }
    out_open(&scan->rows, &state->store, state->rows, row_buffer, ROW_BUFFER);
    out_open(&scan->folders, &state->store, state->folders, folder_buffer, FOLDER_BUFFER);
    uint32_t heap_size = scratch.size > state->heap_start ? scratch.size - state->heap_start : 0;
    out_open(&scan->heap, &state->store, (Region){.offset = state->heap_start, .size = heap_size},
             heap_buffer, HEAP_BUFFER);
    if (!open_folder_table(state)) {
        return;
    }
    scan->chain_depth = 0;
    scan->chain_path_length = 0;
    for (int field = 0; field < BUILDER_FIELDS; field++) {
        scan->previous_text[field] = BUILDER_UNSET;
        scan->previous_length[field] = 0;
    }
    state->folder_count = 1;
    write_folder_row(state, 0, 0, BUILDER_NONE);
}

static bool same_text(const char *left, const char *right, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static uint32_t heap_put(BuilderState *state, const char *text, uint32_t length)
{
    uint32_t offset = out_position(&state->scan->heap);
    return out_put(&state->scan->heap, text, length) ? offset : BUILDER_UNSET;
}

static bool store_field(BuilderState *state, int field, const char *text, uint32_t length,
                        TrackRow *row)
{
    ScanTime *scan = state->scan;
    if (length >= BUILDER_TEXT_CAPACITY) {
        state->arena.failed = true;
        return false;
    }
    row->length[field] = (uint16_t)length;
    if (length == 0) {
        row->text[field] = 0;
        return true;
    }
    if (scan->previous_text[field] != BUILDER_UNSET && scan->previous_length[field] == length &&
        same_text(scan->previous[field], text, length)) {
        row->text[field] = scan->previous_text[field];
        return true;
    }
    uint32_t offset = heap_put(state, text, length);
    if (offset == BUILDER_UNSET) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        scan->previous[field][index] = text[index];
    }
    scan->previous_text[field] = offset;
    scan->previous_length[field] = (uint16_t)length;
    row->text[field] = offset;
    return true;
}

static bool chain_matches(const ScanTime *scan, uint32_t level, const char *name, uint32_t length)
{
    const ChainLevel *chain = &scan->chain[level];
    return chain->length == length && same_text(scan->chain_text + chain->start, name, length);
}

static bool push_folder(BuilderState *state, const char *name, uint32_t length)
{
    ScanTime *scan = state->scan;
    uint32_t start = scan->chain_depth == 0 ? 0
                                            : scan->chain[scan->chain_depth - 1].start +
                                                  scan->chain[scan->chain_depth - 1].length;
    if (scan->chain_depth >= BUILDER_MAX_DEPTH || start + length > BUILDER_TEXT_CAPACITY) {
        state->arena.failed = true;
        return false;
    }
    uint32_t parent = scan->chain_depth == 0 ? 0 : scan->chain[scan->chain_depth - 1].node;
    uint32_t first = folder_hash(HASH_START, parent, name, length);
    uint32_t second = folder_hash(SECOND_SEED, parent, name, length);
    uint32_t node = find_folder(scan, parent, first, second);
    if (node == BUILDER_UNSET) {
        if (state->folder_count >= scan->folder_capacity) {
            state->arena.failed = true;
            return false;
        }
        uint32_t offset = length > 0 ? heap_put(state, name, length) : 0;
        if (offset == BUILDER_UNSET || !write_folder_row(state, offset, length, parent)) {
            return false;
        }
        node = state->folder_count++;
        remember_folder(scan, node, parent, first, second);
    }
    for (uint32_t index = 0; index < length; index++) {
        scan->chain_text[start + index] = name[index];
    }
    scan->chain[scan->chain_depth++] =
        (ChainLevel){.node = (uint16_t)node, .start = (uint16_t)start, .length = (uint16_t)length};
    scan->chain_path_length += length + 1;
    return true;
}

static void pop_to(ScanTime *scan, uint32_t depth)
{
    while (scan->chain_depth > depth) {
        scan->chain_depth--;
        scan->chain_path_length -= scan->chain[scan->chain_depth].length + 1u;
    }
}

static bool place_path(BuilderState *state, const char *path, TrackRow *row)
{
    ScanTime *scan = state->scan;
    uint32_t level = 0;
    const char *start = path;
    for (const char *cursor = path;; cursor++) {
        if (*cursor != '/' && *cursor != '\0') {
            continue;
        }
        uint32_t length = (uint32_t)(cursor - start);
        if (*cursor == '\0') {
            pop_to(scan, level);
            row->folder = level == 0 ? 0 : scan->chain[level - 1].node;
            return store_field(state, BUILDER_FILE, start, length, row);
        }
        if (level < scan->chain_depth && !chain_matches(scan, level, start, length)) {
            pop_to(scan, level);
        }
        if (level >= scan->chain_depth && !push_folder(state, start, length)) {
            return false;
        }
        level++;
        start = cursor + 1;
    }
}

BuilderResult library_builder_add_track(LibraryBuilder *builder, const BuilderTrack *track)
{
    BuilderState *state = state_of(builder);
    if (state == NULL) {
        return BUILDER_OUT_OF_MEMORY;
    }
    if (outcome(state) != BUILDER_OK) {
        return outcome(state);
    }
    if (state->prepare_done) {
        return BUILDER_OUT_OF_MEMORY;
    }
    if (state->track_count >= BUILDER_MAX_INDEX) {
        return BUILDER_TOO_MANY;
    }
    TrackRow row = {.year = track->year,
                    .disc = track->disc,
                    .number = track->number,
                    .spoken = spoken_kind_of(track->path) != SPOKEN_NONE,
                    .guessed = track->album_artist_guessed ? 1 : 0,
                    .duration_ms = track->duration_ms};
    const char *texts[BUILDER_FIELDS - 1] = {track->title, track->artist, track->album_artist,
                                             track->album, track->genre};
    bool stored = place_path(state, track->path, &row);
    for (int field = 0; stored && field < BUILDER_FIELDS - 1; field++) {
        stored = store_field(state, field, texts[field], builder_text_length(texts[field]), &row);
    }
    uint8_t bytes[BUILDER_ROW_SIZE];
    builder_row_encode(&row, bytes);
    if (!stored || !out_put(&state->scan->rows, bytes, sizeof bytes)) {
        return outcome(state) != BUILDER_OK ? outcome(state) : BUILDER_OUT_OF_MEMORY;
    }
    uint32_t title = row.length[BUILDER_TITLE];
    state->text_bytes +=
        (title > 0 ? title + 1 : 0) + state->scan->chain_path_length + row.length[BUILDER_FILE] + 1;
    state->track_count++;
    return BUILDER_OK;
}

static bool finish_scan(BuilderState *state, uint32_t scan_mark)
{
    ScanTime *scan = state->scan;
    bool closed = out_close(&scan->rows) && out_close(&scan->folders) && out_close(&scan->heap);
    state->store.end = (out_position(&scan->heap) + 3) & ~3u;
    state->scan = NULL;
    state->arena.used = scan_mark;
    return closed;
}

static bool open_caches(BuilderState *state, Prepared *prepared)
{
    return cache_init(&prepared->rows, &state->store, &state->arena, ROW_CACHE_BLOCK,
                      ROW_CACHE_BLOCKS) &&
           cache_init(&prepared->folders, &state->store, &state->arena, FOLDER_CACHE_BLOCK,
                      FOLDER_CACHE_BLOCKS) &&
           cache_init(&prepared->heap, &state->store, &state->arena, HEAP_CACHE_BLOCK,
                      HEAP_CACHE_BLOCKS);
}

static bool open_playlists(BuilderState *state, Prepared *prepared)
{
    prepared->playlists =
        arena_take(&state->arena, BUILDER_MAX_PLAYLISTS * (uint32_t)sizeof(PlaylistRow));
    uint8_t *name_buffer = arena_take(&state->arena, NAME_BUFFER);
    uint8_t *link_buffer = arena_take(&state->arena, LINK_BUFFER);
    prepared->playlist_names = store_take(&state->store, PLAYLIST_NAME_BYTES);
    prepared->playlist_links = store_take(&state->store, PLAYLIST_LINK_BYTES);
    if (state->arena.failed || state->store.failed) {
        return false;
    }
    out_open(&prepared->playlist_name_out, &state->store, prepared->playlist_names, name_buffer,
             NAME_BUFFER);
    out_open(&prepared->playlist_link_out, &state->store, prepared->playlist_links, link_buffer,
             LINK_BUFFER);
    prepared->playlist_count = 0;
    prepared->playlist_link_count = 0;
    return true;
}

BuilderResult library_builder_prepare(LibraryBuilder *builder)
{
    BuilderState *state = state_of(builder);
    if (state == NULL) {
        return BUILDER_OUT_OF_MEMORY;
    }
    if (state->prepare_done) {
        return state->prepare_result;
    }
    state->prepare_done = true;
    state->prepare_result = outcome(state);
    if (state->prepare_result != BUILDER_OK || state->scan == NULL) {
        return state->prepare_result;
    }
    uint32_t scan_mark = (uint32_t)((uint8_t *)state->scan - state->arena.base);
    bool ready = finish_scan(state, scan_mark);
    Prepared *prepared = arena_take(&state->arena, (uint32_t)sizeof(Prepared));
    state->prepared = prepared;
    ready = ready && prepared != NULL && open_caches(state, prepared) &&
            builder_guess_album_artists(state, prepared) && builder_rank(state, prepared) &&
            builder_model(state, prepared) && open_playlists(state, prepared) &&
            builder_index_paths(state, prepared);
    state->prepare_result = outcome(state);
    if (!ready && state->prepare_result == BUILDER_OK) {
        state->prepare_result = BUILDER_OUT_OF_MEMORY;
    }
    return state->prepare_result;
}

BuilderResult library_builder_begin_playlist(LibraryBuilder *builder, const char *name,
                                             const char *folder)
{
    BuilderResult result = library_builder_prepare(builder);
    if (result != BUILDER_OK) {
        return result;
    }
    BuilderState *state = state_of(builder);
    Prepared *prepared = state->prepared;
    if (prepared->playlist_count >= BUILDER_MAX_PLAYLISTS) {
        return BUILDER_TOO_MANY;
    }
    uint32_t length = builder_text_length(name);
    uint32_t folder_length = builder_text_length(folder);
    if (length >= BUILDER_TEXT_CAPACITY || folder_length >= BUILDER_TEXT_CAPACITY) {
        return BUILDER_OUT_OF_MEMORY;
    }
    uint32_t offset = out_position(&prepared->playlist_name_out);
    if (!out_put(&prepared->playlist_name_out, name, length)) {
        return outcome(state);
    }
    for (uint32_t index = 0; index <= folder_length; index++) {
        prepared->playlist_folder[index] = folder[index];
    }
    prepared->playlists[prepared->playlist_count++] =
        (PlaylistRow){.name = offset,
                      .length = (uint16_t)length,
                      .reserved = 0,
                      .first_link = prepared->playlist_link_count,
                      .count = 0};
    return BUILDER_OK;
}

BuilderResult library_builder_playlist_entry(LibraryBuilder *builder, const char *entry)
{
    BuilderState *state = state_of(builder);
    if (state == NULL || !state->prepare_done || state->prepare_result != BUILDER_OK ||
        state->prepared->playlist_count == 0) {
        return BUILDER_OUT_OF_MEMORY;
    }
    Prepared *prepared = state->prepared;
    uint32_t position = builder_resolve_entry(state, prepared, prepared->playlist_folder, entry);
    if (outcome(state) != BUILDER_OK) {
        return outcome(state);
    }
    if (position == BUILDER_UNSET) {
        return BUILDER_OK;
    }
    uint8_t bytes[2];
    builder_put16(bytes, position);
    if (!out_put(&prepared->playlist_link_out, bytes, sizeof bytes)) {
        return outcome(state);
    }
    prepared->playlist_link_count++;
    prepared->playlists[prepared->playlist_count - 1].count++;
    return BUILDER_OK;
}

BuilderResult library_builder_finish(LibraryBuilder *builder, ByteSink sink, uint32_t capacity,
                                     uint32_t *length)
{
    BuilderResult result = library_builder_prepare(builder);
    if (result != BUILDER_OK) {
        return result;
    }
    BuilderState *state = state_of(builder);
    Prepared *prepared = state->prepared;
    if (!out_close(&prepared->playlist_name_out) || !out_close(&prepared->playlist_link_out)) {
        return outcome(state);
    }
    result = builder_encode(state, prepared, sink, capacity, length);
    return result != BUILDER_OK ? result : outcome(state);
}
