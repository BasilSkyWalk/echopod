#include <stddef.h>

#include "builder_internal.h"
#include "library/sort_key.h"

enum {
    STREAM_BUFFER = 4096,
    RECORD_HEADER = 12,
    FOLDER_HEADER = 6,
    DISPLAY_SIZE = 8
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

static int compare_bytes(const uint8_t *left, uint32_t left_length, const uint8_t *right,
                         uint32_t right_length)
{
    uint32_t shorter = left_length < right_length ? left_length : right_length;
    for (uint32_t index = 0; index < shorter; index++) {
        if (left[index] != right[index]) {
            return (int)left[index] - (int)right[index];
        }
    }
    return left_length < right_length ? -1 : left_length > right_length ? 1 : 0;
}

static uint32_t key_length_of(const uint8_t *record)
{
    return builder_get16(record + 10);
}

static int compare_keys_only(const uint8_t *left, const uint8_t *right)
{
    return compare_bytes(left + RECORD_HEADER, key_length_of(left), right + RECORD_HEADER,
                         key_length_of(right));
}

static int compare_raw_only(const uint8_t *left, const uint8_t *right)
{
    uint32_t left_key = key_length_of(left);
    uint32_t right_key = key_length_of(right);
    return compare_bytes(left + RECORD_HEADER + left_key,
                         builder_get16(left) - (uint32_t)RECORD_HEADER - left_key,
                         right + RECORD_HEADER + right_key,
                         builder_get16(right) - (uint32_t)RECORD_HEADER - right_key);
}

static int compare_field_keys(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    int key = compare_keys_only(left, right);
    return key != 0 ? key : compare_raw_only(left, right);
}

static bool emit_run(Prepared *prepared, Tail *tail, uint32_t run, const TrackRow *row, int field,
                     char *text, char *key)
{
    uint32_t length = row->length[field];
    if (length > 0 && !builder_read_text(&prepared->heap, row->text[field], length, text)) {
        return false;
    }
    text[length] = '\0';
    key[0] = (char)row->spoken;
    uint32_t key_length = 1u + (uint32_t)sort_key_make(text, key + 1, SORT_KEY_CAPACITY - 1);
    uint8_t header[RECORD_HEADER];
    builder_put16(header, RECORD_HEADER + key_length + length);
    builder_put16(header + 2, run);
    builder_put32(header + 4, row->text[field]);
    builder_put16(header + 8, length);
    builder_put16(header + 10, key_length);
    return out_put(&tail->out, header, sizeof header) && out_put(&tail->out, key, key_length) &&
           out_put(&tail->out, text, length);
}

typedef struct RunReader {
    In in;
    uint32_t text;
    uint32_t length;
    uint32_t run;
    uint8_t spoken;
    bool started;
} RunReader;

static bool next_row(RunReader *reader, int field, TrackRow *row, bool *new_run)
{
    uint8_t bytes[BUILDER_ROW_SIZE];
    if (!in_get(&reader->in, bytes, sizeof bytes)) {
        return false;
    }
    builder_row_decode(bytes, row);
    *new_run = !reader->started || row->text[field] != reader->text ||
               row->length[field] != reader->length || row->spoken != reader->spoken;
    if (*new_run) {
        reader->run += reader->started ? 1u : 0u;
        reader->started = true;
        reader->text = row->text[field];
        reader->length = row->length[field];
        reader->spoken = row->spoken;
    }
    return true;
}

static void open_rows(BuilderState *state, RunReader *reader, uint8_t *buffer)
{
    *reader = (RunReader){.run = 0, .started = false};
    in_open(&reader->in, &state->store, state->rows.offset,
            state->rows.offset + state->track_count * BUILDER_ROW_SIZE, buffer, STREAM_BUFFER);
}

static bool collect_runs(BuilderState *state, Prepared *prepared, int field, Region *input,
                         uint32_t *runs)
{
    Arena *arena = &state->arena;
    uint8_t *row_buffer = arena_take(arena, STREAM_BUFFER);
    uint8_t *tail_buffer = arena_take(arena, STREAM_BUFFER);
    char *text = arena_take(arena, BUILDER_TEXT_CAPACITY);
    char *key = arena_take(arena, SORT_KEY_CAPACITY);
    Tail tail;
    if (arena->failed || !open_tail(state, &tail, tail_buffer)) {
        return false;
    }
    RunReader reader;
    open_rows(state, &reader, row_buffer);
    for (uint32_t track = 0; track < state->track_count; track++) {
        TrackRow row;
        bool new_run = false;
        if (!next_row(&reader, field, &row, &new_run) ||
            (new_run && !emit_run(prepared, &tail, reader.run, &row, field, text, key))) {
            return false;
        }
    }
    *runs = reader.started ? reader.run + 1 : 0;
    return close_tail(state, &tail, input);
}

typedef struct RankTarget {
    uint16_t *rank_of_run;
    uint16_t *spelling_of_run;
    Region display;
    uint32_t ranks;
    uint32_t music_ranks;
} RankTarget;

static bool put_display(Out *out, Region display, uint32_t text, uint32_t length)
{
    uint8_t entry[DISPLAY_SIZE];
    builder_put32(entry, text);
    builder_put16(entry + 4, length);
    builder_put16(entry + 6, 0);
    return display.size == 0 || out_put(out, entry, sizeof entry);
}

static bool assign_ranks(BuilderState *state, Region sorted, RankTarget *target)
{
    Arena *arena = &state->arena;
    uint8_t *buffer = arena_take(arena, STREAM_BUFFER);
    uint8_t *record = arena_take(arena, BUILDER_MAX_RECORD);
    uint8_t *previous = arena_take(arena, BUILDER_MAX_RECORD);
    uint8_t *display_buffer = arena_take(arena, STREAM_BUFFER);
    if (arena->failed) {
        return false;
    }
    In in;
    in_open(&in, &state->store, sorted.offset, sorted.offset + sorted.size, buffer, STREAM_BUFFER);
    Out out;
    out_open(&out, &state->store, target->display, display_buffer, STREAM_BUFFER);
    uint32_t ranks = 0;
    uint32_t spellings = 0;
    uint32_t best_run = 0;
    uint32_t best_text = 0;
    uint32_t best_length = 0;
    bool have_previous = false;
    while (!in_done(&in)) {
        if (!in_record(&in, record)) {
            return false;
        }
        uint32_t run = builder_get16(record + 2);
        bool new_key = !have_previous || compare_keys_only(previous, record) != 0;
        bool new_spelling = new_key || compare_raw_only(previous, record) != 0;
        if (new_key) {
            if (have_previous && !put_display(&out, target->display, best_text, best_length)) {
                return false;
            }
            ranks++;
            target->music_ranks += record[RECORD_HEADER] == 0 ? 1u : 0u;
            best_run = run;
            best_text = builder_get32(record + 4);
            best_length = builder_get16(record + 8);
        } else if (run < best_run) {
            best_run = run;
            best_text = builder_get32(record + 4);
            best_length = builder_get16(record + 8);
        }
        if (new_spelling) {
            spellings++;
            uint32_t length = builder_get16(record);
            for (uint32_t index = 0; index < length; index++) {
                previous[index] = record[index];
            }
            have_previous = true;
        }
        target->rank_of_run[run] = (uint16_t)(ranks - 1);
        if (target->spelling_of_run != NULL) {
            target->spelling_of_run[run] = (uint16_t)(spellings - 1);
        }
    }
    if (have_previous && !put_display(&out, target->display, best_text, best_length)) {
        return false;
    }
    target->ranks = ranks;
    return out_close(&out);
}

static bool write_rank_column(BuilderState *state, int field, const uint16_t *rank_of_run,
                              Region column)
{
    Arena *arena = &state->arena;
    uint32_t mark = arena->used;
    uint8_t *buffer = arena_take(arena, STREAM_BUFFER);
    uint16_t *ranks = arena_take(arena, state->track_count * 2 + 2);
    if (arena->failed) {
        return false;
    }
    RunReader reader;
    open_rows(state, &reader, buffer);
    for (uint32_t track = 0; track < state->track_count; track++) {
        TrackRow row;
        bool new_run = false;
        if (!next_row(&reader, field, &row, &new_run)) {
            return false;
        }
        ranks[track] = rank_of_run[reader.run];
    }
    bool written = builder_write_column(state, column, ranks, state->track_count);
    arena->used = mark;
    return written;
}

static GroupTable *group_of_field(Prepared *prepared, int field)
{
    switch (field) {
    case BUILDER_ARTIST:
        return &prepared->artists;
    case BUILDER_ALBUM_ARTIST:
        return &prepared->album_artists;
    case BUILDER_GENRE:
        return &prepared->genres;
    default:
        return NULL;
    }
}

static bool rank_field(BuilderState *state, Prepared *prepared, int field)
{
    uint32_t count = state->track_count;
    GroupTable *group = group_of_field(prepared, field);
    prepared->columns[field] = store_take(&state->store, count * 2 + 2);
    RankTarget target = {.display = {.offset = 0, .size = 0}, .music_ranks = 0};
    if (group != NULL) {
        target.display = store_take(&state->store, count * DISPLAY_SIZE + DISPLAY_SIZE);
        group->display = target.display;
    }
    if (field == BUILDER_GENRE) {
        prepared->genre_spelling = store_take(&state->store, count * 2 + 2);
    }
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    Region input;
    uint32_t runs = 0;
    Region sorted;
    if (!collect_runs(state, prepared, field, &input, &runs)) {
        return false;
    }
    state->arena.used = arena_mark;
    SortJob job = {.store = &state->store,
                   .arena = &state->arena,
                   .input = input,
                   .compare = compare_field_keys,
                   .context = NULL};
    if (!sort_records(&job, &sorted)) {
        return false;
    }
    target.rank_of_run = arena_take(&state->arena, runs * 2 + 2);
    target.spelling_of_run =
        field == BUILDER_GENRE ? arena_take(&state->arena, runs * 2 + 2) : NULL;
    if (state->arena.failed || !assign_ranks(state, sorted, &target) ||
        !write_rank_column(state, field, target.rank_of_run, prepared->columns[field]) ||
        (field == BUILDER_GENRE &&
         !write_rank_column(state, field, target.spelling_of_run, prepared->genre_spelling))) {
        return false;
    }
    prepared->rank_count[field] = target.ranks;
    prepared->music_ranks[field] = target.music_ranks;
    if (group != NULL) {
        group->count = target.ranks;
    }
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return true;
}

static int compare_folder_names(const uint8_t *left, const uint8_t *right, const void *context)
{
    (void)context;
    uint32_t left_key = builder_get16(left + 4);
    uint32_t right_key = builder_get16(right + 4);
    int key = compare_bytes(left + FOLDER_HEADER, left_key, right + FOLDER_HEADER, right_key);
    if (key != 0) {
        return key;
    }
    return compare_bytes(left + FOLDER_HEADER + left_key,
                         builder_get16(left) - (uint32_t)FOLDER_HEADER - left_key,
                         right + FOLDER_HEADER + right_key,
                         builder_get16(right) - (uint32_t)FOLDER_HEADER - right_key);
}

static bool collect_folders(BuilderState *state, Prepared *prepared, Region *input)
{
    Arena *arena = &state->arena;
    uint8_t *folder_buffer = arena_take(arena, STREAM_BUFFER);
    uint8_t *tail_buffer = arena_take(arena, STREAM_BUFFER);
    char *text = arena_take(arena, BUILDER_TEXT_CAPACITY);
    char *key = arena_take(arena, SORT_KEY_CAPACITY);
    Tail tail;
    if (arena->failed || !open_tail(state, &tail, tail_buffer)) {
        return false;
    }
    In in;
    in_open(&in, &state->store, state->folders.offset,
            state->folders.offset + state->folder_count * BUILDER_FOLDER_SIZE, folder_buffer,
            STREAM_BUFFER);
    for (uint32_t node = 0; node < state->folder_count; node++) {
        uint8_t bytes[BUILDER_FOLDER_SIZE];
        FolderRow row;
        if (!in_get(&in, bytes, sizeof bytes)) {
            return false;
        }
        builder_folder_decode(bytes, &row);
        if (node == 0) {
            continue;
        }
        if (row.length > 0 && !builder_read_text(&prepared->heap, row.name, row.length, text)) {
            return false;
        }
        text[row.length] = '\0';
        uint32_t key_length = (uint32_t)sort_key_make(text, key, SORT_KEY_CAPACITY);
        uint8_t header[FOLDER_HEADER];
        builder_put16(header, FOLDER_HEADER + key_length + row.length);
        builder_put16(header + 2, node);
        builder_put16(header + 4, key_length);
        if (!out_put(&tail.out, header, sizeof header) || !out_put(&tail.out, key, key_length) ||
            !out_put(&tail.out, text, row.length)) {
            return false;
        }
    }
    return close_tail(state, &tail, input);
}

static bool rank_folders(BuilderState *state, Prepared *prepared)
{
    uint32_t count = state->folder_count;
    prepared->folder_order = store_take(&state->store, count * 2 + 2);
    if (state->store.failed) {
        return false;
    }
    uint32_t arena_mark = state->arena.used;
    uint32_t store_mark = state->store.end;
    Region input;
    Region sorted;
    if (!collect_folders(state, prepared, &input)) {
        return false;
    }
    state->arena.used = arena_mark;
    SortJob job = {.store = &state->store,
                   .arena = &state->arena,
                   .input = input,
                   .compare = compare_folder_names,
                   .context = NULL};
    if (!sort_records(&job, &sorted)) {
        return false;
    }
    uint16_t *order = arena_take(&state->arena, count * 2 + 2);
    uint8_t *buffer = arena_take(&state->arena, STREAM_BUFFER);
    uint8_t *record = arena_take(&state->arena, BUILDER_MAX_RECORD);
    uint8_t *previous = arena_take(&state->arena, BUILDER_MAX_RECORD);
    if (state->arena.failed) {
        return false;
    }
    order[0] = 0;
    In in;
    in_open(&in, &state->store, sorted.offset, sorted.offset + sorted.size, buffer, STREAM_BUFFER);
    uint32_t ranks = 0;
    bool have_previous = false;
    while (!in_done(&in)) {
        if (!in_record(&in, record)) {
            return false;
        }
        if (!have_previous || compare_folder_names(previous, record, NULL) != 0) {
            ranks++;
            for (uint32_t index = 0; index < builder_get16(record); index++) {
                previous[index] = record[index];
            }
            have_previous = true;
        }
        order[builder_get16(record + 2)] = (uint16_t)ranks;
    }
    prepared->folder_rank_count = ranks + 1;
    bool written = builder_write_column(state, prepared->folder_order, order, count);
    state->arena.used = arena_mark;
    state->store.end = store_mark;
    return written;
}

bool builder_rank(BuilderState *state, Prepared *prepared)
{
    for (int field = 0; field < BUILDER_FIELDS; field++) {
        if (!rank_field(state, prepared, field)) {
            return false;
        }
    }
    return rank_folders(state, prepared);
}
