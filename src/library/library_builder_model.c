#include <stddef.h>

#include "builder_internal.h"

enum {
    STREAM_BUFFER = 4096,
    COLUMN_BUFFER = 1024,
    ALBUM_TUPLE = 18,
    ALBUM_PRE = 16,
    ORDER_TUPLE = 10,
    ALBUM_FINAL = 20,
    GROUP_TUPLE = 10,
    GROUP_ENTRY = 8,
    LINK_TUPLE = 12,
    FOLDER_FINAL = 16,
    TITLE_TUPLE = 8,
    KIND_CHILD = 0,
    KIND_TRACK = 1
};

typedef struct Tail {
    Out out;
    uint32_t start;
} Tail;

static bool open_tail(BuilderState *state, Tail *tail, uint8_t *buffer)
{
    Store *store = &state->store;
    if (store->end > store->io.size) {
        store->failed = true;
        return false;
    }
    tail->start = store->end;
    out_open(&tail->out, store, (Region){.offset = store->end, .size = store->io.size - store->end},
             buffer, STREAM_BUFFER);
    return true;
}

static bool close_tail(BuilderState *state, Tail *tail, Region *written)
{
    if (!out_close(&tail->out)) {
        return false;
    }
    *written = (Region){.offset = tail->start, .size = tail->out.written};
    state->store.end = (tail->start + tail->out.written + 3) & ~3u;
    return true;
}

static int compare_u16(const uint8_t *left, const uint8_t *right, uint32_t offset)
{
    uint32_t a = builder_get16(left + offset);
    uint32_t b = builder_get16(right + offset);
    return a < b ? -1 : a > b ? 1 : 0;
}

static int compare_u8(const uint8_t *left, const uint8_t *right, uint32_t offset)
{
    return (int)left[offset] - (int)right[offset];
}

static bool sort_with(BuilderState *state, Region input, SortCompare compare, Region *sorted)
{
    SortJob job = {.store = &state->store,
                   .arena = &state->arena,
                   .input = input,
                   .compare = compare,
                   .context = NULL};
    return sort_records(&job, sorted);
}

typedef struct Column {
    In in;
} Column;

static bool open_column(BuilderState *state, Column *column, Region region, uint32_t count)
{
    uint8_t *buffer = arena_take(&state->arena, COLUMN_BUFFER);
    if (buffer == NULL) {
        return false;
    }
    in_open(&column->in, &state->store, region.offset, region.offset + count * 2, buffer,
            COLUMN_BUFFER);
    return true;
}

static bool next_value(Column *column, uint16_t *value)
{
    uint8_t bytes[2];
    if (!in_get(&column->in, bytes, sizeof bytes)) {
        return false;
    }
    *value = builder_get16(bytes);
    return true;
}

static int by_album_key(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    int artist = compare_u16(left, right, 2);
    return artist != 0 ? artist : compare_u16(left, right, 4);
}

static int by_album_genre(const uint8_t *left, const uint8_t *right, const void *context)
{
    int album = by_album_key(left, right, context);
    return album != 0 ? album : compare_u16(left, right, 8);
}

static int by_album_track(const uint8_t *left, const uint8_t *right, const void *context)
{
    int album = by_album_key(left, right, context);
    if (album != 0) {
        return album;
    }
    int disc = compare_u8(left, right, 14);
    if (disc != 0) {
        return disc;
    }
    int number = compare_u8(left, right, 15);
    return number != 0 ? number : compare_u16(left, right, 12);
}

static bool emit_album_tuples(BuilderState *state, Prepared *prepared, Region *input)
{
    uint32_t mark = state->arena.used;
    Column album_artist;
    Column album;
    Column genre;
    Column genre_key;
    Column title;
    uint8_t *row_buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *tail_buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint32_t count = state->track_count;
    Tail tail;
    if (row_buffer == NULL || tail_buffer == NULL ||
        !open_column(state, &album_artist, prepared->columns[BUILDER_ALBUM_ARTIST], count) ||
        !open_column(state, &album, prepared->columns[BUILDER_ALBUM], count) ||
        !open_column(state, &genre, prepared->genre_spelling, count) ||
        !open_column(state, &genre_key, prepared->columns[BUILDER_GENRE], count) ||
        !open_column(state, &title, prepared->columns[BUILDER_TITLE], count) ||
        !open_tail(state, &tail, tail_buffer)) {
        return false;
    }
    In rows;
    in_open(&rows, &state->store, state->rows.offset, state->rows.offset + count * BUILDER_ROW_SIZE,
            row_buffer, STREAM_BUFFER);
    for (uint32_t track = 0; track < count; track++) {
        uint8_t bytes[BUILDER_ROW_SIZE];
        TrackRow row;
        uint16_t values[5];
        if (!in_get(&rows, bytes, sizeof bytes) || !next_value(&album_artist, &values[0]) ||
            !next_value(&album, &values[1]) || !next_value(&genre, &values[2]) ||
            !next_value(&title, &values[3]) || !next_value(&genre_key, &values[4])) {
            return false;
        }
        builder_row_decode(bytes, &row);
        uint8_t tuple[ALBUM_TUPLE];
        builder_put16(tuple, ALBUM_TUPLE);
        builder_put16(tuple + 2, values[0]);
        builder_put16(tuple + 4, values[1]);
        builder_put16(tuple + 6, track);
        builder_put16(tuple + 8, values[2]);
        builder_put16(tuple + 10, row.year);
        builder_put16(tuple + 12, values[3]);
        tuple[14] = row.disc;
        tuple[15] = row.number;
        builder_put16(tuple + 16, values[4]);
        if (!out_put(&tail.out, tuple, sizeof tuple)) {
            return false;
        }
    }
    bool closed = close_tail(state, &tail, input);
    state->arena.used = mark;
    return closed;
}

typedef struct Stream {
    In in;
    uint8_t *record;
    bool live;
} Stream;

static bool open_stream(BuilderState *state, Stream *stream, Region region)
{
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    stream->record = arena_take(&state->arena, BUILDER_MAX_RECORD);
    if (buffer == NULL || stream->record == NULL) {
        return false;
    }
    in_open(&stream->in, &state->store, region.offset, region.offset + region.size, buffer,
            STREAM_BUFFER);
    stream->live = false;
    return true;
}

static bool pull(Stream *stream)
{
    if (in_done(&stream->in)) {
        stream->live = false;
        return true;
    }
    stream->live = in_record(&stream->in, stream->record);
    return stream->live;
}

static bool album_genres(BuilderState *state, Region sorted, uint16_t *genre_of_album,
                         uint32_t *albums)
{
    uint32_t mark = state->arena.used;
    Stream stream;
    uint8_t *previous = arena_take(&state->arena, ALBUM_TUPLE);
    if (previous == NULL || !open_stream(state, &stream, sorted) || !pull(&stream)) {
        return false;
    }
    uint32_t album = 0;
    while (stream.live) {
        for (uint32_t index = 0; index < ALBUM_TUPLE; index++) {
            previous[index] = stream.record[index];
        }
        uint32_t best_genre = builder_get16(previous + 16);
        uint32_t best_count = 0;
        uint32_t best_first = BUILDER_UNSET;
        while (stream.live && by_album_key(previous, stream.record, NULL) == 0) {
            uint32_t genre = builder_get16(stream.record + 8);
            uint32_t genre_key = builder_get16(stream.record + 16);
            uint32_t first = builder_get16(stream.record + 6);
            uint32_t occurrences = 0;
            while (stream.live && by_album_key(previous, stream.record, NULL) == 0 &&
                   builder_get16(stream.record + 8) == genre) {
                occurrences++;
                if (!pull(&stream)) {
                    return false;
                }
            }
            if (occurrences > best_count || (occurrences == best_count && first < best_first)) {
                best_count = occurrences;
                best_first = first;
                best_genre = genre_key;
            }
        }
        genre_of_album[album++] = (uint16_t)best_genre;
    }
    *albums = album;
    state->arena.used = mark;
    return !state->store.io_failed;
}

static bool write_album_rows(BuilderState *state, Region sorted, const uint16_t *genre_of_album,
                             Region albums)
{
    uint32_t mark = state->arena.used;
    Stream stream;
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *previous = arena_take(&state->arena, ALBUM_TUPLE);
    if (buffer == NULL || previous == NULL || !open_stream(state, &stream, sorted) ||
        !pull(&stream)) {
        return false;
    }
    Out out;
    out_open(&out, &state->store, albums, buffer, STREAM_BUFFER);
    uint32_t album = 0;
    uint32_t member_start = 0;
    while (stream.live) {
        for (uint32_t index = 0; index < ALBUM_TUPLE; index++) {
            previous[index] = stream.record[index];
        }
        uint32_t count = 0;
        uint32_t year = 0;
        while (stream.live && by_album_key(previous, stream.record, NULL) == 0) {
            uint32_t candidate = builder_get16(stream.record + 10);
            if (candidate != 0 && (year == 0 || candidate < year)) {
                year = candidate;
            }
            count++;
            if (!pull(&stream)) {
                return false;
            }
        }
        uint8_t row[ALBUM_PRE];
        builder_put16(row, builder_get16(previous + 6));
        builder_put16(row + 2, builder_get16(previous + 2));
        builder_put16(row + 4, builder_get16(previous + 4));
        builder_put16(row + 6, year);
        builder_put16(row + 8, count);
        builder_put16(row + 10, genre_of_album[album++]);
        builder_put32(row + 12, member_start);
        member_start += count;
        if (!out_put(&out, row, sizeof row)) {
            return false;
        }
    }
    bool closed = out_close(&out);
    state->arena.used = mark;
    return closed;
}

static bool write_members(BuilderState *state, Region sorted, Region members)
{
    uint32_t mark = state->arena.used;
    Stream stream;
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    if (buffer == NULL || !open_stream(state, &stream, sorted) || !pull(&stream)) {
        return false;
    }
    Out out;
    out_open(&out, &state->store, members, buffer, STREAM_BUFFER);
    while (stream.live) {
        if (!out_put(&out, stream.record + 6, 2) || !pull(&stream)) {
            return false;
        }
    }
    bool closed = out_close(&out);
    state->arena.used = mark;
    return closed;
}

typedef struct AlbumPhase {
    Region pre;
    Region members;
    uint32_t count;
} AlbumPhase;

static bool group_albums(BuilderState *state, Prepared *prepared, AlbumPhase *albums)
{
    uint32_t count = state->track_count;
    albums->pre = store_take(&state->store, count * ALBUM_PRE + ALBUM_PRE);
    albums->members = store_take(&state->store, count * 2 + 2);
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    Region input;
    Region sorted;
    if (!emit_album_tuples(state, prepared, &input) ||
        !sort_with(state, input, by_album_genre, &sorted)) {
        return false;
    }
    uint16_t *genre_of_album = arena_take(&state->arena, count * 2 + 2);
    if (genre_of_album == NULL || !album_genres(state, sorted, genre_of_album, &albums->count)) {
        return false;
    }
    state->store.end = (input.offset + input.size + 3) & ~3u;
    if (!sort_with(state, input, by_album_key, &sorted) ||
        !write_album_rows(state, sorted, genre_of_album, albums->pre)) {
        return false;
    }
    state->store.end = (input.offset + input.size + 3) & ~3u;
    if (!sort_with(state, input, by_album_track, &sorted) ||
        !write_members(state, sorted, albums->members)) {
        return false;
    }
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return true;
}

static int by_album_order(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    int title = compare_u16(left, right, 2);
    if (title != 0) {
        return title;
    }
    int artist = compare_u16(left, right, 4);
    return artist != 0 ? artist : compare_u16(left, right, 6);
}

static bool read_album_pre(BuilderState *state, const AlbumPhase *albums, uint32_t album,
                           uint8_t *row)
{
    return store_read(&state->store, albums->pre.offset + album * ALBUM_PRE, row, ALBUM_PRE);
}

static bool order_albums(BuilderState *state, const AlbumPhase *albums, uint16_t *album_at)
{
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *pre_buffer = arena_take(&state->arena, STREAM_BUFFER);
    Tail tail;
    if (buffer == NULL || pre_buffer == NULL || !open_tail(state, &tail, buffer)) {
        return false;
    }
    In pre;
    in_open(&pre, &state->store, albums->pre.offset, albums->pre.offset + albums->count * ALBUM_PRE,
            pre_buffer, STREAM_BUFFER);
    for (uint32_t album = 0; album < albums->count; album++) {
        uint8_t row[ALBUM_PRE];
        if (!in_get(&pre, row, sizeof row)) {
            return false;
        }
        uint8_t tuple[ORDER_TUPLE];
        builder_put16(tuple, ORDER_TUPLE);
        builder_put16(tuple + 2, builder_get16(row + 4));
        builder_put16(tuple + 4, builder_get16(row + 2));
        builder_put16(tuple + 6, builder_get16(row));
        builder_put16(tuple + 8, album);
        if (!out_put(&tail.out, tuple, sizeof tuple)) {
            return false;
        }
    }
    Region input;
    Region sorted;
    if (!close_tail(state, &tail, &input)) {
        return false;
    }
    state->arena.used = arena_mark;
    Stream stream;
    if (!sort_with(state, input, by_album_order, &sorted) || !open_stream(state, &stream, sorted) ||
        !pull(&stream)) {
        return false;
    }
    uint32_t position = 0;
    while (stream.live) {
        album_at[position++] = builder_get16(stream.record + 8);
        if (!pull(&stream)) {
            return false;
        }
    }
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return true;
}

static bool place_tracks(BuilderState *state, Prepared *prepared, const AlbumPhase *albums,
                         const uint16_t *album_at)
{
    uint32_t count = state->track_count;
    uint32_t mark = state->arena.used;
    uint16_t *final_tracks = arena_take(&state->arena, count * 2 + 2);
    uint16_t *track_album = arena_take(&state->arena, count * 2 + 2);
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *members = arena_take(&state->arena, STREAM_BUFFER);
    if (state->arena.failed) {
        return false;
    }
    Out out;
    out_open(&out, &state->store, prepared->albums, buffer, STREAM_BUFFER);
    uint32_t position = 0;
    for (uint32_t final = 0; final < albums->count; final++) {
        uint8_t pre[ALBUM_PRE];
        if (!read_album_pre(state, albums, album_at[final], pre)) {
            return false;
        }
        uint32_t first_seen = builder_get16(pre);
        uint32_t member_count = builder_get16(pre + 8);
        uint32_t member_start = builder_get32(pre + 12);
        uint32_t first_track = position;
        for (uint32_t done = 0; done < member_count;) {
            uint32_t step =
                member_count - done < STREAM_BUFFER / 2 ? member_count - done : STREAM_BUFFER / 2;
            if (!store_read(&state->store, albums->members.offset + (member_start + done) * 2,
                            members, step * 2)) {
                return false;
            }
            for (uint32_t index = 0; index < step; index++) {
                final_tracks[position] = builder_get16(members + 2 * index);
                track_album[position++] = (uint16_t) final;
            }
            done += step;
        }
        TrackRow row;
        if (!builder_read_row(&prepared->rows, state, first_seen, &row)) {
            return false;
        }
        uint8_t record[ALBUM_FINAL];
        builder_put32(record, row.text[BUILDER_ALBUM]);
        builder_put16(record + 4, row.length[BUILDER_ALBUM]);
        builder_put16(record + 6, builder_get16(pre + 2));
        builder_put16(record + 8, builder_get16(pre + 10));
        builder_put16(record + 10, builder_get16(pre + 6));
        builder_put16(record + 12, first_track);
        builder_put16(record + 14, member_count);
        builder_put16(record + 16, builder_get16(pre + 4));
        builder_put16(record + 18, 0);
        if (!out_put(&out, record, sizeof record)) {
            return false;
        }
    }
    bool done = out_close(&out) &&
                builder_write_column(state, prepared->final_tracks, final_tracks, count) &&
                builder_write_column(state, prepared->track_album, track_album, count);
    state->arena.used = mark;
    return done;
}

static bool build_albums(BuilderState *state, Prepared *prepared)
{
    uint32_t count = state->track_count;
    prepared->final_tracks = store_take(&state->store, count * 2 + 2);
    prepared->track_album = store_take(&state->store, count * 2 + 2);
    prepared->albums = store_take(&state->store, count * ALBUM_FINAL + ALBUM_FINAL);
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    AlbumPhase albums = {.count = 0};
    if (!group_albums(state, prepared, &albums)) {
        return false;
    }
    uint16_t *album_at = arena_take(&state->arena, albums.count * 2 + 2);
    if (album_at == NULL || !order_albums(state, &albums, album_at) ||
        !place_tracks(state, prepared, &albums, album_at)) {
        return false;
    }
    prepared->album_count = albums.count;
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return true;
}

typedef struct Shared {
    uint16_t *final_tracks;
    uint16_t *track_album;
    uint16_t *album_year;
    uint16_t *album_title;
} Shared;

static bool load_shared(BuilderState *state, Prepared *prepared, Shared *shared)
{
    uint32_t count = state->track_count;
    uint32_t albums = prepared->album_count;
    shared->final_tracks = builder_load_column(state, prepared->final_tracks, count);
    shared->track_album = builder_load_column(state, prepared->track_album, count);
    shared->album_year = arena_take(&state->arena, albums * 2 + 2);
    shared->album_title = arena_take(&state->arena, albums * 2 + 2);
    if (shared->final_tracks == NULL || shared->track_album == NULL || shared->album_year == NULL ||
        shared->album_title == NULL) {
        return false;
    }
    for (uint32_t album = 0; album < albums; album++) {
        uint8_t record[ALBUM_FINAL];
        if (!store_read(&state->store, prepared->albums.offset + album * ALBUM_FINAL, record,
                        sizeof record)) {
            return false;
        }
        shared->album_year[album] = builder_get16(record + 10);
        shared->album_title[album] = builder_get16(record + 16);
    }
    return true;
}

static int by_group_chronology(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    for (uint32_t offset = 2; offset <= 8; offset += 2) {
        int order = compare_u16(left, right, offset);
        if (order != 0) {
            return order;
        }
    }
    return 0;
}

static int by_group_title(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    int group = compare_u16(left, right, 2);
    if (group != 0) {
        return group;
    }
    int title = compare_u16(left, right, 6);
    return title != 0 ? title : compare_u16(left, right, 8);
}

static bool emit_group_tuples(BuilderState *state, const Shared *shared, const uint16_t *groups,
                              Region *input)
{
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    Tail tail;
    if (buffer == NULL || !open_tail(state, &tail, buffer)) {
        return false;
    }
    for (uint32_t position = 0; position < state->track_count; position++) {
        uint32_t album = shared->track_album[position];
        uint8_t tuple[GROUP_TUPLE];
        builder_put16(tuple, GROUP_TUPLE);
        builder_put16(tuple + 2, groups[shared->final_tracks[position]]);
        builder_put16(tuple + 4, shared->album_year[album]);
        builder_put16(tuple + 6, shared->album_title[album]);
        builder_put16(tuple + 8, album);
        if (!out_put(&tail.out, tuple, sizeof tuple)) {
            return false;
        }
    }
    return close_tail(state, &tail, input);
}

static bool write_group_links(BuilderState *state, Region sorted, GroupTable *table)
{
    Stream stream;
    uint8_t *link_buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *group_buffer = arena_take(&state->arena, STREAM_BUFFER);
    if (link_buffer == NULL || group_buffer == NULL || !open_stream(state, &stream, sorted) ||
        !pull(&stream)) {
        return false;
    }
    Out links;
    Out groups;
    out_open(&links, &state->store, table->links, link_buffer, STREAM_BUFFER);
    out_open(&groups, &state->store, table->groups, group_buffer, STREAM_BUFFER);
    uint32_t total = 0;
    for (uint32_t group = 0; group < table->count; group++) {
        uint32_t first = total;
        uint32_t last_album = BUILDER_UNSET;
        while (stream.live && builder_get16(stream.record + 2) == group) {
            uint32_t album = builder_get16(stream.record + 8);
            if (album != last_album) {
                uint8_t value[2];
                builder_put16(value, album);
                if (!out_put(&links, value, sizeof value)) {
                    return false;
                }
                total++;
                last_album = album;
            }
            if (!pull(&stream)) {
                return false;
            }
        }
        uint8_t entry[GROUP_ENTRY];
        builder_put32(entry, first);
        builder_put16(entry + 4, total - first);
        builder_put16(entry + 6, 0);
        if (!out_put(&groups, entry, sizeof entry)) {
            return false;
        }
    }
    table->link_count = total;
    return out_close(&links) && out_close(&groups);
}

static bool build_group(BuilderState *state, Prepared *prepared, const Shared *shared, int field,
                        GroupTable *table, SortCompare order)
{
    table->groups = store_take(&state->store, table->count * GROUP_ENTRY + GROUP_ENTRY);
    table->links = store_take(&state->store, state->track_count * 2 + 2);
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    uint16_t *groups = builder_load_column(state, prepared->columns[field], state->track_count);
    Region input;
    Region sorted;
    if (groups == NULL || !emit_group_tuples(state, shared, groups, &input)) {
        return false;
    }
    if (!sort_with(state, input, order, &sorted) || !write_group_links(state, sorted, table)) {
        return false;
    }
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return true;
}

static int by_rank_path(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    uint32_t left_depth = (builder_get16(left) - 4u) / 2;
    uint32_t right_depth = (builder_get16(right) - 4u) / 2;
    uint32_t shorter = left_depth < right_depth ? left_depth : right_depth;
    for (uint32_t level = 0; level < shorter; level++) {
        int order = compare_u16(left, right, 4 + 2 * level);
        if (order != 0) {
            return order;
        }
    }
    return left_depth < right_depth ? -1 : left_depth > right_depth ? 1 : 0;
}

static bool emit_folder_paths(BuilderState *state, Prepared *prepared, const uint16_t *order,
                              Region *input)
{
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    Tail tail;
    if (buffer == NULL || !open_tail(state, &tail, buffer)) {
        return false;
    }
    for (uint32_t node = 0; node < state->folder_count; node++) {
        uint16_t chain[BUILDER_MAX_DEPTH];
        uint32_t depth = 0;
        for (uint32_t walk = node; walk != 0 && depth < BUILDER_MAX_DEPTH;) {
            FolderRow row;
            if (!builder_read_folder(&prepared->folders, state, walk, &row)) {
                return false;
            }
            chain[depth++] = order[walk];
            walk = row.parent;
        }
        uint8_t record[4 + 2 * BUILDER_MAX_DEPTH];
        builder_put16(record, 4 + 2 * depth);
        builder_put16(record + 2, node);
        for (uint32_t level = 0; level < depth; level++) {
            builder_put16(record + 4 + 2 * level, chain[depth - 1 - level]);
        }
        if (!out_put(&tail.out, record, 4 + 2 * depth)) {
            return false;
        }
    }
    return close_tail(state, &tail, input);
}

typedef struct FolderPhase {
    uint16_t *new_index;
    uint16_t *node_at;
    uint16_t *subfolders;
    uint16_t *path_length;
} FolderPhase;

static bool number_folders(BuilderState *state, Prepared *prepared, FolderPhase *phase)
{
    uint32_t count = state->folder_count;
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    uint16_t *order = builder_load_column(state, prepared->folder_order, count);
    Region input;
    Region sorted;
    if (order == NULL || !emit_folder_paths(state, prepared, order, &input)) {
        return false;
    }
    state->arena.used = arena_mark;
    Stream stream;
    if (!sort_with(state, input, by_rank_path, &sorted) || !open_stream(state, &stream, sorted) ||
        !pull(&stream)) {
        return false;
    }
    uint32_t position = 0;
    while (stream.live) {
        uint32_t node = builder_get16(stream.record + 2);
        phase->new_index[node] = (uint16_t)position;
        phase->node_at[position++] = (uint16_t)node;
        if (!pull(&stream)) {
            return false;
        }
    }
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return position == count;
}

static bool measure_folders(BuilderState *state, Prepared *prepared, FolderPhase *phase)
{
    uint32_t count = state->folder_count;
    for (uint32_t index = 0; index < count; index++) {
        phase->subfolders[index] = 0;
    }
    phase->path_length[0] = 0;
    for (uint32_t position = 1; position < count; position++) {
        uint32_t node = phase->node_at[position];
        FolderRow row;
        if (!builder_read_folder(&prepared->folders, state, node, &row)) {
            return false;
        }
        phase->subfolders[phase->new_index[row.parent]]++;
        phase->path_length[node] = (uint16_t)(phase->path_length[row.parent] + row.length + 1);
    }
    return true;
}

static int by_folder_link(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    int folder = compare_u16(left, right, 2);
    if (folder != 0) {
        return folder;
    }
    for (uint32_t offset = 4; offset <= 6; offset++) {
        int order = compare_u8(left, right, offset);
        if (order != 0) {
            return order;
        }
    }
    int key = compare_u16(left, right, 8);
    return key != 0 ? key : compare_u16(left, right, 10);
}

static bool put_link_tuple(Tail *tail, uint32_t folder, uint32_t kind, uint32_t disc,
                           uint32_t number, uint32_t key, uint32_t value)
{
    uint8_t tuple[LINK_TUPLE];
    builder_put16(tuple, LINK_TUPLE);
    builder_put16(tuple + 2, folder);
    tuple[4] = (uint8_t)kind;
    tuple[5] = (uint8_t)disc;
    tuple[6] = (uint8_t)number;
    tuple[7] = 0;
    builder_put16(tuple + 8, key);
    builder_put16(tuple + 10, value);
    return out_put(&tail->out, tuple, sizeof tuple);
}

static bool emit_folder_links(BuilderState *state, Prepared *prepared, const uint16_t *final_tracks,
                              const FolderPhase *phase, Region *input)
{
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint16_t *files =
        builder_load_column(state, prepared->columns[BUILDER_FILE], state->track_count);
    Tail tail;
    if (buffer == NULL || files == NULL || !open_tail(state, &tail, buffer)) {
        return false;
    }
    for (uint32_t node = 1; node < state->folder_count; node++) {
        FolderRow row;
        uint32_t child = phase->new_index[node];
        if (!builder_read_folder(&prepared->folders, state, node, &row) ||
            !put_link_tuple(&tail, phase->new_index[row.parent], KIND_CHILD, 0, 0, child, child)) {
            return false;
        }
    }
    for (uint32_t position = 0; position < state->track_count; position++) {
        uint32_t track = final_tracks[position];
        TrackRow row;
        if (!builder_read_row(&prepared->rows, state, track, &row) ||
            !put_link_tuple(&tail, phase->new_index[row.folder], KIND_TRACK, row.disc, row.number,
                            files[track], position)) {
            return false;
        }
    }
    return close_tail(state, &tail, input);
}

static bool write_folder_rows(BuilderState *state, Prepared *prepared, const FolderPhase *phase,
                              Region sorted)
{
    Stream stream;
    uint8_t *link_buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *row_buffer = arena_take(&state->arena, STREAM_BUFFER);
    if (link_buffer == NULL || row_buffer == NULL || !open_stream(state, &stream, sorted) ||
        !pull(&stream)) {
        return false;
    }
    Out links;
    Out rows;
    out_open(&links, &state->store, prepared->folder_links, link_buffer, STREAM_BUFFER);
    out_open(&rows, &state->store, prepared->folders_final, row_buffer, STREAM_BUFFER);
    uint32_t total = 0;
    for (uint32_t position = 0; position < state->folder_count; position++) {
        uint32_t first = total;
        uint32_t tracks = 0;
        while (stream.live && builder_get16(stream.record + 2) == position) {
            uint8_t value[2];
            builder_put16(value, builder_get16(stream.record + 10));
            tracks += stream.record[4] == KIND_TRACK;
            total++;
            if (!out_put(&links, value, sizeof value) || !pull(&stream)) {
                return false;
            }
        }
        uint32_t node = phase->node_at[position];
        FolderRow row;
        if (!builder_read_folder(&prepared->folders, state, node, &row)) {
            return false;
        }
        uint8_t record[FOLDER_FINAL];
        builder_put32(record, row.name);
        builder_put16(record + 4, row.length);
        builder_put16(record + 6, node == 0 ? BUILDER_NONE : phase->new_index[row.parent]);
        builder_put16(record + 8, phase->subfolders[position]);
        builder_put16(record + 10, tracks);
        builder_put32(record + 12, first);
        if (!out_put(&rows, record, sizeof record)) {
            return false;
        }
    }
    prepared->folder_link_count = total;
    return out_close(&links) && out_close(&rows);
}

static bool build_folders(BuilderState *state, Prepared *prepared, const uint16_t *final_tracks)
{
    uint32_t count = state->folder_count;
    prepared->folder_new_index = store_take(&state->store, count * 2 + 2);
    prepared->folder_path_length = store_take(&state->store, count * 2 + 2);
    prepared->folders_final = store_take(&state->store, count * FOLDER_FINAL + FOLDER_FINAL);
    prepared->folder_links = store_take(&state->store, (count + state->track_count) * 2 + 2);
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    FolderPhase phase = {.new_index = arena_take(&state->arena, count * 2 + 2),
                         .node_at = arena_take(&state->arena, count * 2 + 2),
                         .subfolders = arena_take(&state->arena, count * 2 + 2),
                         .path_length = arena_take(&state->arena, count * 2 + 2)};
    if (state->arena.failed || !number_folders(state, prepared, &phase) ||
        !measure_folders(state, prepared, &phase) ||
        !builder_write_column(state, prepared->folder_path_length, phase.path_length, count)) {
        return false;
    }
    state->arena.used = (uint32_t)((uint8_t *)phase.path_length - state->arena.base);
    phase.path_length = NULL;
    uint32_t links_mark = state->arena.used;
    Region input;
    Region sorted;
    if (!emit_folder_links(state, prepared, final_tracks, &phase, &input)) {
        return false;
    }
    state->arena.used = links_mark;
    if (!sort_with(state, input, by_folder_link, &sorted) ||
        !write_folder_rows(state, prepared, &phase, sorted) ||
        !builder_write_column(state, prepared->folder_new_index, phase.new_index, count)) {
        return false;
    }
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return true;
}

static int by_title_then_artist(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    int title = compare_u16(left, right, 2);
    return title != 0 ? title : compare_u16(left, right, 4);
}

static bool build_title_order(BuilderState *state, Prepared *prepared, const uint16_t *final_tracks)
{
    uint32_t count = state->track_count;
    prepared->title_order = store_take(&state->store, count * 2 + 2);
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    uint16_t *titles = builder_load_column(state, prepared->columns[BUILDER_TITLE], count);
    uint16_t *artists = builder_load_column(state, prepared->columns[BUILDER_ARTIST], count);
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    Tail tail;
    if (titles == NULL || artists == NULL || buffer == NULL || !open_tail(state, &tail, buffer)) {
        return false;
    }
    for (uint32_t position = 0; position < count; position++) {
        uint32_t track = final_tracks[position];
        uint8_t tuple[TITLE_TUPLE];
        builder_put16(tuple, TITLE_TUPLE);
        builder_put16(tuple + 2, titles[track]);
        builder_put16(tuple + 4, artists[track]);
        builder_put16(tuple + 6, position);
        if (!out_put(&tail.out, tuple, sizeof tuple)) {
            return false;
        }
    }
    Region input;
    Region sorted;
    if (!close_tail(state, &tail, &input)) {
        return false;
    }
    uint32_t sort_mark = state->arena.used;
    if (!sort_with(state, input, by_title_then_artist, &sorted)) {
        return false;
    }
    state->arena.used = sort_mark;
    Stream stream;
    uint8_t *out_buffer = arena_take(&state->arena, STREAM_BUFFER);
    if (out_buffer == NULL || !open_stream(state, &stream, sorted) || !pull(&stream)) {
        return false;
    }
    Out out;
    out_open(&out, &state->store, prepared->title_order, out_buffer, STREAM_BUFFER);
    while (stream.live) {
        if (!out_put(&out, stream.record + 6, 2) || !pull(&stream)) {
            return false;
        }
    }
    bool closed = out_close(&out);
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return closed;
}

bool builder_model(BuilderState *state, Prepared *prepared)
{
    if (!build_albums(state, prepared)) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    Shared shared;
    bool built =
        load_shared(state, prepared, &shared) &&
        build_group(state, prepared, &shared, BUILDER_ARTIST, &prepared->artists,
                    by_group_chronology) &&
        build_group(state, prepared, &shared, BUILDER_ALBUM_ARTIST, &prepared->album_artists,
                    by_group_chronology) &&
        build_group(state, prepared, &shared, BUILDER_GENRE, &prepared->genres, by_group_title);
    state->arena.used = arena_mark;
    if (!built) {
        return false;
    }
    uint16_t *final_tracks = builder_load_column(state, prepared->final_tracks, state->track_count);
    built = final_tracks != NULL && build_folders(state, prepared, final_tracks) &&
            build_title_order(state, prepared, final_tracks);
    state->arena.used = arena_mark;
    return built;
}
