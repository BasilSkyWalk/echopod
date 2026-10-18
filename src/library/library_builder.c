#include "library/library_builder.h"

#include "builder_internal.h"
#include "core/hash.h"
#include "library/sort_key.h"

enum {
    BYTES_PER_SLOT_SHARE = 32,
    BYTES_PER_FOLDER_SHARE = 192,
    MINIMUM_SLOTS = 16,
    MINIMUM_FOLDERS = 8,
    MAX_INDEX = 0xFFFF
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
    uint32_t slot_share = size / BYTES_PER_SLOT_SHARE;
    uint32_t folders = size / BYTES_PER_FOLDER_SHARE;
    *builder = (LibraryBuilder){.memory = memory, .size = size & ~3u};
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
    if (work->failed || need > work->size - work->used) {
        work->failed = true;
        return NULL;
    }
    void *block = work->memory + work->used;
    work->used += need;
    return block;
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

BuilderResult library_builder_finish(LibraryBuilder *builder, ByteSink sink, uint32_t *length)
{
    if (builder->empty_string == BUILDER_UNSET) {
        return BUILDER_OUT_OF_MEMORY;
    }
    uint32_t start = (tracks_end(builder) + 3) & ~3u;
    Work work = {.memory = builder->memory + start,
                 .used = 0,
                 .size = builder->strings_floor > start ? builder->strings_floor - start : 0,
                 .failed = false};
    LibraryModel model;
    if (!builder_build_model(builder, &work, &model)) {
        return BUILDER_OUT_OF_MEMORY;
    }
    return builder_encode(builder, &model, &work, sink, length);
}
