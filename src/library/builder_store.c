#include <stddef.h>

#include "builder_internal.h"

uint16_t builder_get16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | bytes[1] << 8);
}

uint32_t builder_get32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

void builder_put16(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

void builder_put32(uint8_t *bytes, uint32_t value)
{
    builder_put16(bytes, value & 0xFFFF);
    builder_put16(bytes + 2, value >> 16);
}

uint32_t builder_text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void copy_bytes(uint8_t *to, const uint8_t *from, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        to[index] = from[index];
    }
}

void arena_init(Arena *arena, void *memory, uint32_t size)
{
    uintptr_t start = (uintptr_t)memory;
    uint32_t skip = (uint32_t)((BUILDER_ALIGNMENT - start % BUILDER_ALIGNMENT) % BUILDER_ALIGNMENT);
    *arena = (Arena){.base = (uint8_t *)memory + skip,
                     .size = size > skip ? (size - skip) & ~(uint32_t)(BUILDER_ALIGNMENT - 1) : 0,
                     .used = 0,
                     .failed = false};
}

void *arena_take(Arena *arena, uint32_t bytes)
{
    uint32_t need = (bytes + BUILDER_ALIGNMENT - 1) & ~(uint32_t)(BUILDER_ALIGNMENT - 1);
    if (arena->failed || need > arena->size - arena->used) {
        arena->failed = true;
        return NULL;
    }
    void *block = arena->base + arena->used;
    arena->used += need;
    return block;
}

uint32_t arena_left(const Arena *arena)
{
    return arena->failed ? 0 : arena->size - arena->used;
}

void store_init(Store *store, BuilderScratch io, uint32_t start)
{
    *store = (Store){.io = io, .end = start, .failed = false, .io_failed = false};
}

Region store_take(Store *store, uint32_t bytes)
{
    uint32_t need = (bytes + 3) & ~3u;
    if (store->failed || store->end > store->io.size || need > store->io.size - store->end) {
        store->failed = true;
        return (Region){.offset = store->io.size, .size = 0};
    }
    Region region = {.offset = store->end, .size = need};
    store->end += need;
    return region;
}

bool store_read(Store *store, uint32_t offset, void *bytes, uint32_t length)
{
    if (length == 0) {
        return true;
    }
    bool read = !store->io_failed && store->io.read(store->io.context, offset, bytes, length);
    store->io_failed = !read;
    return read;
}

bool store_write(Store *store, uint32_t offset, const void *bytes, uint32_t length)
{
    if (length == 0) {
        return true;
    }
    bool written = !store->io_failed && store->io.write(store->io.context, offset, bytes, length);
    store->io_failed = !written;
    return written;
}

void out_open(Out *out, Store *store, Region region, uint8_t *buffer, uint32_t capacity)
{
    *out = (Out){.store = store,
                 .start = region.offset,
                 .limit = region.size,
                 .written = 0,
                 .buffer = buffer,
                 .capacity = capacity,
                 .used = 0};
}

static bool out_flush(Out *out)
{
    bool written =
        store_write(out->store, out->start + out->written - out->used, out->buffer, out->used);
    out->used = 0;
    return written;
}

bool out_put(Out *out, const void *bytes, uint32_t length)
{
    const uint8_t *from = bytes;
    if (length > out->limit - out->written) {
        out->store->failed = true;
        return false;
    }
    while (length > 0) {
        uint32_t room = out->capacity - out->used;
        uint32_t step = length < room ? length : room;
        copy_bytes(out->buffer + out->used, from, step);
        out->used += step;
        out->written += step;
        from += step;
        length -= step;
        if (out->used == out->capacity && !out_flush(out)) {
            return false;
        }
    }
    return true;
}

bool out_close(Out *out)
{
    return out->used == 0 || out_flush(out);
}

uint32_t out_position(const Out *out)
{
    return out->start + out->written;
}

void in_open(In *in, Store *store, uint32_t start, uint32_t end, uint8_t *buffer, uint32_t capacity)
{
    *in = (In){.store = store,
               .position = start,
               .end = end,
               .buffer = buffer,
               .capacity = capacity,
               .filled = 0,
               .taken = 0};
}

static bool in_refill(In *in)
{
    uint32_t left = in->filled - in->taken;
    for (uint32_t index = 0; index < left; index++) {
        in->buffer[index] = in->buffer[in->taken + index];
    }
    in->filled = left;
    in->taken = 0;
    uint32_t room = in->capacity - in->filled;
    uint32_t available = in->end - in->position;
    uint32_t step = room < available ? room : available;
    if (!store_read(in->store, in->position, in->buffer + in->filled, step)) {
        return false;
    }
    in->position += step;
    in->filled += step;
    return true;
}

bool in_get(In *in, void *bytes, uint32_t length)
{
    if (length > in->filled - in->taken) {
        if (length > in->capacity || !in_refill(in) || length > in->filled - in->taken) {
            return false;
        }
    }
    copy_bytes(bytes, in->buffer + in->taken, length);
    in->taken += length;
    return true;
}

bool in_record(In *in, uint8_t *record)
{
    uint8_t header[2];
    if (!in_get(in, header, 2)) {
        return false;
    }
    uint32_t length = builder_get16(header);
    record[0] = header[0];
    record[1] = header[1];
    return length >= 2 && length <= BUILDER_MAX_RECORD && in_get(in, record + 2, length - 2);
}

bool in_done(const In *in)
{
    return in->taken == in->filled && in->position == in->end;
}

bool cache_init(Cache *cache, Store *store, Arena *arena, uint32_t block_size, uint32_t count)
{
    *cache = (Cache){.store = store,
                     .blocks = arena_take(arena, block_size * count),
                     .tags = arena_take(arena, count * 4),
                     .ages = arena_take(arena, count * 4),
                     .block_size = block_size,
                     .count = count,
                     .clock = 0};
    if (arena->failed) {
        return false;
    }
    for (uint32_t index = 0; index < count; index++) {
        cache->tags[index] = BUILDER_UNSET;
        cache->ages[index] = 0;
    }
    return true;
}

static uint8_t *cache_block(Cache *cache, uint32_t tag)
{
    uint32_t victim = 0;
    for (uint32_t index = 0; index < cache->count; index++) {
        if (cache->tags[index] == tag) {
            cache->ages[index] = ++cache->clock;
            return cache->blocks + index * cache->block_size;
        }
        victim = cache->ages[index] < cache->ages[victim] ? index : victim;
    }
    uint8_t *block = cache->blocks + victim * cache->block_size;
    uint32_t start = tag * cache->block_size;
    uint32_t size = cache->store->io.size;
    uint32_t length =
        start < size && size - start < cache->block_size ? size - start : cache->block_size;
    if (start >= size || !store_read(cache->store, start, block, length)) {
        return NULL;
    }
    cache->tags[victim] = tag;
    cache->ages[victim] = ++cache->clock;
    return block;
}

bool cache_read(Cache *cache, uint32_t offset, void *bytes, uint32_t length)
{
    uint8_t *to = bytes;
    while (length > 0) {
        uint32_t tag = offset / cache->block_size;
        uint32_t within = offset % cache->block_size;
        uint32_t step = cache->block_size - within < length ? cache->block_size - within : length;
        uint8_t *block = cache_block(cache, tag);
        if (block == NULL) {
            return false;
        }
        copy_bytes(to, block + within, step);
        to += step;
        offset += step;
        length -= step;
    }
    return true;
}

void builder_row_encode(const TrackRow *row, uint8_t *bytes)
{
    for (int field = 0; field < BUILDER_FIELDS; field++) {
        builder_put32(bytes + 4 * field, row->text[field]);
        builder_put16(bytes + 24 + 2 * field, row->length[field]);
    }
    builder_put16(bytes + 36, row->folder);
    builder_put16(bytes + 38, row->year);
    bytes[40] = row->disc;
    bytes[41] = row->number;
    builder_put16(bytes + 42, 0);
    builder_put32(bytes + 44, row->duration_ms);
}

void builder_row_decode(const uint8_t *bytes, TrackRow *row)
{
    for (int field = 0; field < BUILDER_FIELDS; field++) {
        row->text[field] = builder_get32(bytes + 4 * field);
        row->length[field] = builder_get16(bytes + 24 + 2 * field);
    }
    row->folder = builder_get16(bytes + 36);
    row->year = builder_get16(bytes + 38);
    row->disc = bytes[40];
    row->number = bytes[41];
    row->duration_ms = builder_get32(bytes + 44);
}

void builder_folder_encode(const FolderRow *row, uint8_t *bytes)
{
    builder_put32(bytes, row->name);
    builder_put16(bytes + 4, row->length);
    builder_put16(bytes + 6, row->parent);
}

void builder_folder_decode(const uint8_t *bytes, FolderRow *row)
{
    row->name = builder_get32(bytes);
    row->length = builder_get16(bytes + 4);
    row->parent = builder_get16(bytes + 6);
}

bool builder_read_row(Cache *rows, const BuilderState *state, uint32_t track, TrackRow *row)
{
    uint8_t bytes[BUILDER_ROW_SIZE];
    if (!cache_read(rows, state->rows.offset + track * BUILDER_ROW_SIZE, bytes, sizeof bytes)) {
        return false;
    }
    builder_row_decode(bytes, row);
    return true;
}

bool builder_read_folder(Cache *folders, const BuilderState *state, uint32_t node, FolderRow *row)
{
    uint8_t bytes[BUILDER_FOLDER_SIZE];
    if (!cache_read(folders, state->folders.offset + node * BUILDER_FOLDER_SIZE, bytes,
                    sizeof bytes)) {
        return false;
    }
    builder_folder_decode(bytes, row);
    return true;
}

bool builder_read_text(Cache *heap, uint32_t offset, uint32_t length, char *text)
{
    if (length >= BUILDER_TEXT_CAPACITY) {
        return false;
    }
    if (!cache_read(heap, offset, text, length)) {
        return false;
    }
    text[length] = '\0';
    return true;
}

bool builder_write_column(BuilderState *state, Region column, const uint16_t *values,
                          uint32_t count)
{
    uint8_t chunk[512];
    uint32_t used = 0;
    uint32_t offset = column.offset;
    for (uint32_t index = 0; index < count; index++) {
        builder_put16(chunk + used, values[index]);
        used += 2;
        if (used == sizeof chunk || index + 1 == count) {
            if (!store_write(&state->store, offset, chunk, used)) {
                return false;
            }
            offset += used;
            used = 0;
        }
    }
    return true;
}

bool builder_read_column(BuilderState *state, Region column, uint16_t *values, uint32_t count)
{
    uint8_t chunk[512];
    uint32_t done = 0;
    while (done < count) {
        uint32_t step = count - done < sizeof chunk / 2 ? count - done : sizeof chunk / 2;
        if (!store_read(&state->store, column.offset + done * 2, chunk, step * 2)) {
            return false;
        }
        for (uint32_t index = 0; index < step; index++) {
            values[done + index] = builder_get16(chunk + 2 * index);
        }
        done += step;
    }
    return true;
}

uint16_t *builder_load_column(BuilderState *state, Region column, uint32_t count)
{
    uint16_t *values = arena_take(&state->arena, count * 2 + 2);
    if (values == NULL || !builder_read_column(state, column, values, count)) {
        return NULL;
    }
    return values;
}
