#include "library/library_builder.h"

#include <stdint.h>

#include "builder_internal.h"
#include "core/hash.h"
#include "library/sort_key.h"

enum {
    BYTES_PER_SLOT_SHARE = 32,
    BYTES_PER_FOLDER_SHARE = 192,
    MINIMUM_SLOTS = 16,
    MINIMUM_FOLDERS = 8,
    MAX_INDEX = 0xFFFF,
    ALIGNMENT = 8
};

const char *builder_text(const LibraryBuilder *builder, uint32_t string)
{
    return (const char *)builder->memory + string + BUILDER_STRING_HEADER;
}

uint32_t *builder_output_offset(const LibraryBuilder *builder, uint32_t string)
{
    return (uint32_t *)(void *)(builder->memory + string);
}

static uint32_t *slots(const LibraryBuilder *builder)
{
    return (uint32_t *)(void *)builder->memory;
}

TrackRecord *builder_track(const LibraryBuilder *builder, uint32_t index)
{
    return (TrackRecord *)(void *)(builder->memory + builder->tracks_offset +
                                   index * BUILDER_TRACK_SIZE);
}

FolderNode *builder_folder(const LibraryBuilder *builder, uint32_t index)
{
    return (FolderNode *)(void *)(builder->memory + builder->slot_count * 4u +
                                  index * BUILDER_FOLDER_SIZE);
}

static uint32_t tracks_end(const LibraryBuilder *builder)
{
    return builder->tracks_offset + builder->track_count * BUILDER_TRACK_SIZE;
}

static uint32_t power_of_two_at_most(uint32_t value)
{
    uint32_t result = 1;
    while (result * 2 <= value) {
        result *= 2;
    }
    return result;
}

static uint32_t intern(LibraryBuilder *builder, const char *text, uint32_t length);

void library_builder_init(LibraryBuilder *builder, void *memory, uint32_t size)
{
    uint32_t skip = (uint32_t)((ALIGNMENT - (uintptr_t)memory % ALIGNMENT) % ALIGNMENT);
    size = size > skip ? size - skip : 0;
    uint32_t slot_share = size / BYTES_PER_SLOT_SHARE;
    uint32_t folders = size / BYTES_PER_FOLDER_SHARE;
    *builder = (LibraryBuilder){.memory = (uint8_t *)memory + skip, .size = size & ~3u};
    builder->slot_count =
        power_of_two_at_most(slot_share > MINIMUM_SLOTS ? slot_share : MINIMUM_SLOTS);
    builder->folder_capacity = folders > MINIMUM_FOLDERS ? folders : MINIMUM_FOLDERS;
    builder->folder_capacity =
        builder->folder_capacity < MAX_INDEX ? builder->folder_capacity : MAX_INDEX;
    builder->tracks_offset =
        builder->slot_count * 4u + builder->folder_capacity * BUILDER_FOLDER_SIZE;
    builder->strings_floor = builder->size;
    for (uint32_t slot = 0; slot < builder->slot_count && slot * 4u < builder->size; slot++) {
        slots(builder)[slot] = 0;
    }
    builder->empty_string = intern(builder, "", 0);
    if (builder->empty_string != BUILDER_UNSET) {
        *builder_folder(builder, 0) =
            (FolderNode){.name = builder->empty_string, .parent = BUILDER_NONE};
        builder->folder_count = 1;
    }
}

static bool same_text(const char *stored, const char *text, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        if (stored[index] != text[index]) {
            return false;
        }
    }
    return stored[length] == '\0';
}

static uint32_t store_string(LibraryBuilder *builder, const char *text, uint32_t length,
                             uint32_t hash)
{
    uint32_t need = (BUILDER_STRING_HEADER + length + 1 + 3) & ~3u;
    if (builder->tracks_offset > builder->size || builder->strings_floor < need ||
        builder->strings_floor - need < tracks_end(builder)) {
        return BUILDER_UNSET;
    }
    builder->strings_floor -= need;
    uint32_t string = builder->strings_floor;
    *builder_output_offset(builder, string) = BUILDER_UNSET;
    *(uint32_t *)(void *)(builder->memory + string + 4) = hash;
    char *stored = (char *)builder->memory + string + BUILDER_STRING_HEADER;
    for (uint32_t index = 0; index < length; index++) {
        stored[index] = text[index];
    }
    stored[length] = '\0';
    builder->string_count++;
    return string;
}

static uint32_t intern(LibraryBuilder *builder, const char *text, uint32_t length)
{
    if (builder->tracks_offset > builder->size) {
        return BUILDER_UNSET;
    }
    uint32_t hash = hash_update(HASH_START, text, length);
    uint32_t mask = builder->slot_count - 1;
    for (uint32_t probe = 0; probe < builder->slot_count; probe++) {
        uint32_t *slot = &slots(builder)[(hash + probe) & mask];
        if (*slot == 0) {
            if (builder->string_count * 4 >= builder->slot_count * 3) {
                return BUILDER_UNSET;
            }
            uint32_t string = store_string(builder, text, length, hash);
            *slot = string == BUILDER_UNSET ? 0 : string + 1;
            return string;
        }
        uint32_t string = *slot - 1;
        if (*(uint32_t *)(void *)(builder->memory + string + 4) == hash &&
            same_text(builder_text(builder, string), text, length)) {
            return string;
        }
    }
    return BUILDER_UNSET;
}

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static uint32_t intern_text(LibraryBuilder *builder, const char *text)
{
    return intern(builder, text, text_length(text));
}

static uint32_t child_folder(LibraryBuilder *builder, uint16_t parent, uint32_t name)
{
    for (uint32_t index = 1; index < builder->folder_count; index++) {
        FolderNode *node = builder_folder(builder, index);
        if (node->parent == parent && node->name == name) {
            return index;
        }
    }
    if (builder->folder_count >= builder->folder_capacity) {
        return BUILDER_UNSET;
    }
    *builder_folder(builder, builder->folder_count) =
        (FolderNode){.name = name, .parent = parent, .reserved = 0};
    return builder->folder_count++;
}

static BuilderResult place_path(LibraryBuilder *builder, const char *path, TrackRecord *record)
{
    uint16_t folder = 0;
    const char *start = path;
    for (const char *cursor = path;; cursor++) {
        if (*cursor != '/' && *cursor != '\0') {
            continue;
        }
        uint32_t name = intern(builder, start, (uint32_t)(cursor - start));
        if (name == BUILDER_UNSET) {
            return BUILDER_OUT_OF_MEMORY;
        }
        if (*cursor == '\0') {
            record->file_name = name;
            record->folder = folder;
            return BUILDER_OK;
        }
        uint32_t child = child_folder(builder, folder, name);
        if (child == BUILDER_UNSET) {
            return BUILDER_OUT_OF_MEMORY;
        }
        folder = (uint16_t)child;
        start = cursor + 1;
    }
}

static BuilderResult intern_fields(LibraryBuilder *builder, const BuilderTrack *track,
                                   TrackRecord *record)
{
    record->title = intern_text(builder, track->title);
    record->artist = intern_text(builder, track->artist);
    record->album_artist = intern_text(builder, track->album_artist);
    record->album = intern_text(builder, track->album);
    record->genre = intern_text(builder, track->genre);
    bool missing = record->title == BUILDER_UNSET || record->artist == BUILDER_UNSET ||
                   record->album_artist == BUILDER_UNSET || record->album == BUILDER_UNSET ||
                   record->genre == BUILDER_UNSET;
    return missing ? BUILDER_OUT_OF_MEMORY : BUILDER_OK;
}

BuilderResult library_builder_add_track(LibraryBuilder *builder, const BuilderTrack *track)
{
    if (builder->empty_string == BUILDER_UNSET) {
        return BUILDER_OUT_OF_MEMORY;
    }
    if (builder->track_count >= MAX_INDEX) {
        return BUILDER_TOO_MANY;
    }
    TrackRecord record = {.year = track->year,
                          .disc = track->disc,
                          .number = track->number,
                          .duration_ms = track->duration_ms};
    BuilderResult result = place_path(builder, track->path, &record);
    if (result == BUILDER_OK) {
        result = intern_fields(builder, track, &record);
    }
    if (result != BUILDER_OK || builder->strings_floor - tracks_end(builder) < BUILDER_TRACK_SIZE) {
        return BUILDER_OUT_OF_MEMORY;
    }
    *builder_track(builder, builder->track_count++) = record;
    return BUILDER_OK;
}

int builder_compare_keys(const LibraryBuilder *builder, uint32_t left, uint32_t right)
{
    return left == right
               ? 0
               : sort_key_compare(builder_text(builder, left), builder_text(builder, right));
}

int builder_compare_raw(const LibraryBuilder *builder, uint32_t left, uint32_t right)
{
    const unsigned char *a = (const unsigned char *)builder_text(builder, left);
    const unsigned char *b = (const unsigned char *)builder_text(builder, right);
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)*a - (int)*b;
}

void *work_alloc(Work *work, uint32_t bytes)
{
    uint32_t need = (bytes + 3) & ~3u;
    if (work->failed || need > work->top - work->used) {
        work->failed = true;
        return NULL;
    }
    void *block = work->memory + work->used;
    work->used += need;
    return block;
}

void *work_alloc_top(Work *work, uint32_t bytes)
{
    uint32_t need = (bytes + 3) & ~3u;
    if (work->failed || need > work->top - work->used) {
        work->failed = true;
        return NULL;
    }
    work->top -= need;
    return work->memory + work->top;
}

uint32_t builder_find(const LibraryBuilder *builder, const char *text, uint32_t length)
{
    uint32_t hash = hash_update(HASH_START, text, length);
    uint32_t mask = builder->slot_count - 1;
    for (uint32_t probe = 0; probe < builder->slot_count; probe++) {
        uint32_t slot = slots(builder)[(hash + probe) & mask];
        if (slot == 0) {
            return BUILDER_UNSET;
        }
        uint32_t string = slot - 1;
        if (*(uint32_t *)(void *)(builder->memory + string + 4) == hash &&
            same_text(builder_text(builder, string), text, length)) {
            return string;
        }
    }
    return BUILDER_UNSET;
}

static void merge(uint16_t *from, uint16_t *to, uint32_t start, uint32_t middle, uint32_t end,
                  IndexCompare compare, const void *context)
{
    uint32_t left = start;
    uint32_t right = middle;
    for (uint32_t out = start; out < end; out++) {
        bool take_left =
            left < middle && (right >= end || compare(context, from[left], from[right]) <= 0);
        to[out] = take_left ? from[left++] : from[right++];
    }
}

void stable_sort(uint16_t *items, uint16_t *scratch, uint32_t count, IndexCompare compare,
                 const void *context)
{
    uint16_t *from = items;
    uint16_t *to = scratch;
    for (uint32_t width = 1; width < count; width *= 2) {
        for (uint32_t start = 0; start < count; start += 2 * width) {
            uint32_t middle = start + width < count ? start + width : count;
            uint32_t end = start + 2 * width < count ? start + 2 * width : count;
            merge(from, to, start, middle, end, compare, context);
        }
        uint16_t *swap = from;
        from = to;
        to = swap;
    }
    if (from != items) {
        for (uint32_t index = 0; index < count; index++) {
            items[index] = from[index];
        }
    }
}

enum {
    PLAYLIST_SHARE = 64,
    MAX_PLAYLISTS = 1024
};

static Work open_work(const LibraryBuilder *builder)
{
    return (Work){.memory = builder->memory + builder->work_start,
                  .used = builder->work_used,
                  .top = builder->work_top,
                  .failed = false};
}

static void close_work(LibraryBuilder *builder, const Work *work)
{
    builder->work_used = work->used;
    builder->work_top = work->top;
}

static bool reserve_playlists(const LibraryBuilder *builder, Work *work, LibraryModel *model)
{
    uint32_t capacity = (work->top - work->used) / PLAYLIST_SHARE / (uint32_t)sizeof(PlaylistModel);
    capacity = capacity < MAX_PLAYLISTS ? capacity : MAX_PLAYLISTS;
    model->playlist_capacity = capacity;
    model->playlists =
        work_alloc(work, (capacity > 0 ? capacity : 1u) * (uint32_t)sizeof(PlaylistModel));
    model->playlist_links = (uint16_t *)(void *)(work->memory + work->used);
    (void)builder;
    return !work->failed;
}

BuilderResult library_builder_prepare(LibraryBuilder *builder)
{
    if (builder->prepared) {
        return builder->prepare_result;
    }
    builder->prepared = true;
    builder->prepare_result = BUILDER_OUT_OF_MEMORY;
    if (builder->empty_string == BUILDER_UNSET) {
        return builder->prepare_result;
    }
    builder->work_start = (tracks_end(builder) + 7) & ~7u;
    builder->work_used = 0;
    builder->work_top = builder->strings_floor > builder->work_start
                            ? (builder->strings_floor - builder->work_start) & ~3u
                            : 0;
    Work work = open_work(builder);
    LibraryModel *model = work_alloc(&work, (uint32_t)sizeof(LibraryModel));
    if (model == NULL || !builder_build_model(builder, &work, model) ||
        !builder_index_paths(builder, &work, model) || !reserve_playlists(builder, &work, model)) {
        return builder->prepare_result;
    }
    close_work(builder, &work);
    builder->model = model;
    builder->prepare_result = BUILDER_OK;
    return BUILDER_OK;
}

static uint32_t text_bytes(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static uint32_t keep_text(Work *work, const char *text)
{
    uint32_t length = text_bytes(text);
    char *copy = work_alloc_top(work, length + 1);
    if (copy == NULL) {
        return BUILDER_UNSET;
    }
    for (uint32_t index = 0; index <= length; index++) {
        copy[index] = text[index];
    }
    return (uint32_t)((uint8_t *)copy - work->memory);
}

BuilderResult library_builder_begin_playlist(LibraryBuilder *builder, const char *name,
                                             const char *folder)
{
    BuilderResult result = library_builder_prepare(builder);
    LibraryModel *model = builder->model;
    if (result != BUILDER_OK) {
        return result;
    }
    if (model->playlist_count >= model->playlist_capacity) {
        return BUILDER_TOO_MANY;
    }
    Work work = open_work(builder);
    uint32_t kept_name = keep_text(&work, name);
    uint32_t kept_folder = keep_text(&work, folder);
    if (kept_name == BUILDER_UNSET || kept_folder == BUILDER_UNSET) {
        return BUILDER_OUT_OF_MEMORY;
    }
    close_work(builder, &work);
    model->playlist_folder = kept_folder;
    model->playlists[model->playlist_count++] =
        (PlaylistModel){.name = kept_name,
                        .name_output = BUILDER_UNSET,
                        .first_link = model->playlist_link_count,
                        .count = 0};
    return BUILDER_OK;
}

BuilderResult library_builder_playlist_entry(LibraryBuilder *builder, const char *entry)
{
    LibraryModel *model = builder->model;
    if (!builder->prepared || builder->prepare_result != BUILDER_OK || model->playlist_count == 0) {
        return BUILDER_OUT_OF_MEMORY;
    }
    Work work = open_work(builder);
    const char *folder = (const char *)work.memory + model->playlist_folder;
    uint32_t position = builder_resolve_entry(builder, model, folder, entry);
    if (position == BUILDER_UNSET) {
        return BUILDER_OK;
    }
    if (work.top - work.used < 2) {
        return BUILDER_OUT_OF_MEMORY;
    }
    model->playlist_links[model->playlist_link_count] = (uint16_t)position;
    work.used += 2;
    close_work(builder, &work);
    model->playlist_link_count++;
    model->playlists[model->playlist_count - 1].count++;
    return BUILDER_OK;
}

BuilderResult library_builder_finish(LibraryBuilder *builder, ByteSink sink, uint32_t *length)
{
    BuilderResult result = library_builder_prepare(builder);
    if (result != BUILDER_OK) {
        return result;
    }
    Work work = open_work(builder);
    work.used = (work.used + 3) & ~3u;
    if (work.used > work.top) {
        return BUILDER_OUT_OF_MEMORY;
    }
    return builder_encode(builder, builder->model, &work, sink, length);
}
