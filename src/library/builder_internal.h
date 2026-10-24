#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/library_builder.h"

#define BUILDER_UNSET 0xFFFFFFFFu

enum {
    BUILDER_NONE = 0xFFFF,
    BUILDER_MAX_INDEX = 0xFFFF,
    BUILDER_FIELDS = 6,
    BUILDER_TITLE = 0,
    BUILDER_ARTIST = 1,
    BUILDER_ALBUM_ARTIST = 2,
    BUILDER_ALBUM = 3,
    BUILDER_GENRE = 4,
    BUILDER_FILE = 5,
    BUILDER_ROW_SIZE = 48,
    BUILDER_FOLDER_SIZE = 8,
    BUILDER_MAX_DEPTH = 64,
    BUILDER_TEXT_CAPACITY = 1024,
    BUILDER_MAX_RECORD = 2 * BUILDER_TEXT_CAPACITY + 64,
    BUILDER_MAX_PLAYLISTS = 1024,
    BUILDER_ALIGNMENT = 8
};

typedef struct Arena {
    uint8_t *base;
    uint32_t size;
    uint32_t used;
    bool failed;
} Arena;

typedef struct Store {
    BuilderScratch io;
    uint32_t end;
    bool failed;
    bool io_failed;
} Store;

typedef struct Out {
    Store *store;
    uint32_t start;
    uint32_t limit;
    uint32_t written;
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t used;
} Out;

typedef struct In {
    Store *store;
    uint32_t position;
    uint32_t end;
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t filled;
    uint32_t taken;
} In;

typedef struct Cache {
    Store *store;
    uint8_t *blocks;
    uint32_t *tags;
    uint32_t *ages;
    uint32_t block_size;
    uint32_t count;
    uint32_t clock;
} Cache;

typedef struct Region {
    uint32_t offset;
    uint32_t size;
} Region;

typedef struct TrackRow {
    uint32_t text[BUILDER_FIELDS];
    uint16_t length[BUILDER_FIELDS];
    uint16_t folder;
    uint16_t year;
    uint8_t disc;
    uint8_t number;
    uint32_t duration_ms;
} TrackRow;

typedef struct FolderRow {
    uint32_t name;
    uint16_t length;
    uint16_t parent;
} FolderRow;

typedef struct ChainLevel {
    uint16_t node;
    uint16_t start;
    uint16_t length;
} ChainLevel;

typedef struct ScanTime {
    Out rows;
    Out folders;
    Out heap;
    char chain_text[BUILDER_TEXT_CAPACITY];
    ChainLevel chain[BUILDER_MAX_DEPTH];
    uint32_t chain_depth;
    uint32_t chain_path_length;
    char previous[BUILDER_FIELDS][BUILDER_TEXT_CAPACITY];
    uint32_t previous_text[BUILDER_FIELDS];
    uint16_t previous_length[BUILDER_FIELDS];
    uint32_t *folder_first;
    uint32_t *folder_second;
    uint16_t *folder_parent;
    uint16_t *folder_slots;
    uint32_t folder_capacity;
    uint32_t folder_mask;
} ScanTime;

typedef struct GroupTable {
    uint32_t count;
    Region display;
    Region groups;
    Region links;
    uint32_t link_count;
} GroupTable;

typedef struct PlaylistRow {
    uint32_t name;
    uint16_t length;
    uint16_t reserved;
    uint32_t first_link;
    uint32_t count;
} PlaylistRow;

typedef struct Prepared {
    uint32_t album_count;
    uint32_t folder_rank_count;
    Region columns[BUILDER_FIELDS];
    uint32_t rank_count[BUILDER_FIELDS];
    Region genre_spelling;
    Region folder_order;
    Region final_tracks;
    Region track_album;
    Region albums;
    GroupTable artists;
    GroupTable album_artists;
    GroupTable genres;
    Region folder_new_index;
    Region folder_path_length;
    Region folders_final;
    Region folder_links;
    uint32_t folder_link_count;
    Region title_order;
    uint32_t index_mark;
    uint16_t *positions;
    uint16_t *exact;
    uint32_t exact_mask;
    uint16_t *by_name;
    uint32_t by_name_mask;
    uint16_t *next_same_name;
    void *resolve;
    Cache rows;
    Cache folders;
    Cache heap;
    PlaylistRow *playlists;
    uint32_t playlist_count;
    Region playlist_names;
    Out playlist_name_out;
    Region playlist_links;
    Out playlist_link_out;
    uint32_t playlist_link_count;
    char playlist_folder[BUILDER_TEXT_CAPACITY];
} Prepared;

typedef struct BuilderState {
    Arena arena;
    Store store;
    BuilderResult error;
    uint32_t track_count;
    uint32_t folder_count;
    uint32_t heap_start;
    uint32_t text_bytes;
    Region rows;
    Region folders;
    ScanTime *scan;
    Prepared *prepared;
    bool prepare_done;
    BuilderResult prepare_result;
} BuilderState;

typedef int (*SortCompare)(const uint8_t *left, const uint8_t *right, const void *context);

typedef struct SortJob {
    Store *store;
    Arena *arena;
    Region input;
    SortCompare compare;
    const void *context;
} SortJob;

uint16_t builder_get16(const uint8_t *bytes);
uint32_t builder_get32(const uint8_t *bytes);
void builder_put16(uint8_t *bytes, uint32_t value);
void builder_put32(uint8_t *bytes, uint32_t value);
uint32_t builder_text_length(const char *text);

void arena_init(Arena *arena, void *memory, uint32_t size);
void *arena_take(Arena *arena, uint32_t bytes);
uint32_t arena_left(const Arena *arena);

void store_init(Store *store, BuilderScratch io, uint32_t start);
Region store_take(Store *store, uint32_t bytes);
bool store_read(Store *store, uint32_t offset, void *bytes, uint32_t length);
bool store_write(Store *store, uint32_t offset, const void *bytes, uint32_t length);

void out_open(Out *out, Store *store, Region region, uint8_t *buffer, uint32_t capacity);
bool out_put(Out *out, const void *bytes, uint32_t length);
bool out_close(Out *out);
uint32_t out_position(const Out *out);

void in_open(In *in, Store *store, uint32_t start, uint32_t end, uint8_t *buffer,
             uint32_t capacity);
bool in_get(In *in, void *bytes, uint32_t length);
bool in_record(In *in, uint8_t *record);
bool in_done(const In *in);

bool cache_init(Cache *cache, Store *store, Arena *arena, uint32_t block_size, uint32_t count);
bool cache_read(Cache *cache, uint32_t offset, void *bytes, uint32_t length);

bool sort_records(const SortJob *job, Region *output);

void builder_row_decode(const uint8_t *bytes, TrackRow *row);
void builder_row_encode(const TrackRow *row, uint8_t *bytes);
void builder_folder_decode(const uint8_t *bytes, FolderRow *row);
void builder_folder_encode(const FolderRow *row, uint8_t *bytes);

bool builder_rank(BuilderState *state, Prepared *prepared);
bool builder_model(BuilderState *state, Prepared *prepared);
bool builder_index_paths(BuilderState *state, Prepared *prepared);
uint32_t builder_resolve_entry(BuilderState *state, Prepared *prepared, const char *folder,
                               const char *entry);
uint32_t builder_folded_path(BuilderState *state, Prepared *prepared, uint32_t position, char *out);
BuilderResult builder_encode(BuilderState *state, Prepared *prepared, ByteSink sink,
                             uint32_t capacity, uint32_t *length);
bool builder_read_column(BuilderState *state, Region column, uint16_t *values, uint32_t count);
bool builder_write_column(BuilderState *state, Region column, const uint16_t *values,
                          uint32_t count);
bool builder_read_text(Cache *heap, uint32_t offset, uint32_t length, char *text);
bool builder_read_row(Cache *rows, const BuilderState *state, uint32_t track, TrackRow *row);
bool builder_read_folder(Cache *folders, const BuilderState *state, uint32_t node, FolderRow *row);
uint16_t *builder_load_column(BuilderState *state, Region column, uint32_t count);
