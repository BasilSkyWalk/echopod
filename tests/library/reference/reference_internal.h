#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "reference_builder.h"

#define REFERENCE_UNSET 0xFFFFFFFFu

enum {
    REFERENCE_NONE = 0xFFFF,
    REFERENCE_STRING_HEADER = 8,
    REFERENCE_TRACK_SIZE = 36,
    REFERENCE_FOLDER_SIZE = 8
};

typedef struct TrackRecord {
    uint32_t title;
    uint32_t file_name;
    uint32_t artist;
    uint32_t album_artist;
    uint32_t album;
    uint32_t genre;
    uint16_t folder;
    uint16_t year;
    uint8_t disc;
    uint8_t number;
    uint16_t guessed;
    uint32_t duration_ms;
} TrackRecord;

typedef struct FolderNode {
    uint32_t name;
    uint16_t parent;
    uint16_t reserved;
} FolderNode;

typedef struct Work {
    uint8_t *memory;
    uint32_t used;
    uint32_t top;
    bool failed;
} Work;

typedef struct PlaylistModel {
    uint32_t name;
    uint32_t name_output;
    uint32_t first_link;
    uint32_t count;
} PlaylistModel;

typedef struct PathIndex {
    uint16_t *exact;
    uint32_t exact_mask;
    uint16_t *by_name;
    uint32_t by_name_mask;
    uint16_t *next_same_name;
    void *scratch;
} PathIndex;

typedef int (*IndexCompare)(const void *context, uint16_t left, uint16_t right);

typedef struct NameIndex {
    uint16_t *group_of_track;
    uint32_t *display;
    uint16_t count;
} NameIndex;

typedef struct GroupLinks {
    uint32_t *first;
    uint16_t *count;
    uint16_t *albums;
    uint32_t total;
} GroupLinks;

typedef struct AlbumModel {
    uint32_t title;
    uint16_t album_artist;
    uint16_t genre;
    uint16_t year;
    uint16_t first_track;
    uint16_t track_count;
    uint16_t member_start;
    uint16_t first_seen;
} AlbumModel;

typedef struct FolderModel {
    uint32_t name;
    uint16_t parent;
    uint16_t subfolder_count;
    uint16_t track_count;
    uint16_t node;
    uint32_t first_link;
} FolderModel;

typedef struct LibraryModel {
    uint32_t track_count;
    uint16_t *final_tracks;
    uint16_t *track_album;
    uint16_t *track_folder;
    NameIndex artists;
    NameIndex album_artists;
    NameIndex genres;
    uint16_t album_count;
    AlbumModel *albums;
    uint16_t *album_members;
    GroupLinks artist_links;
    GroupLinks album_artist_links;
    GroupLinks genre_links;
    uint16_t folder_count;
    FolderModel *folders;
    uint16_t *folder_links;
    uint32_t folder_link_count;
    uint16_t *title_order;
    PathIndex paths;
    PlaylistModel *playlists;
    uint32_t playlist_count;
    uint32_t playlist_capacity;
    uint16_t *playlist_links;
    uint32_t playlist_link_count;
    uint32_t playlist_folder;
} LibraryModel;

const char *reference_text(const ReferenceBuilder *builder, uint32_t string);
TrackRecord *reference_track(const ReferenceBuilder *builder, uint32_t index);
FolderNode *reference_folder(const ReferenceBuilder *builder, uint32_t index);
uint32_t *reference_output_offset(const ReferenceBuilder *builder, uint32_t string);
int reference_compare_keys(const ReferenceBuilder *builder, uint32_t left, uint32_t right);
int reference_compare_raw(const ReferenceBuilder *builder, uint32_t left, uint32_t right);
void *reference_work_alloc(Work *work, uint32_t bytes);
void *reference_work_alloc_top(Work *work, uint32_t bytes);
uint32_t reference_find(const ReferenceBuilder *builder, const char *text, uint32_t length);
bool reference_index_paths(const ReferenceBuilder *builder, Work *work, LibraryModel *model);
uint32_t reference_resolve_entry(const ReferenceBuilder *builder, const LibraryModel *model,
                                 const char *folder, const char *entry);
void reference_stable_sort(uint16_t *items, uint16_t *scratch, uint32_t count, IndexCompare compare,
                           const void *context);
bool reference_build_model(ReferenceBuilder *builder, Work *work, LibraryModel *model);
typedef struct EncodeTarget {
    ReferenceSink sink;
    uint32_t capacity;
} EncodeTarget;

ReferenceResult reference_encode(ReferenceBuilder *builder, const LibraryModel *model, Work *work,
                                 EncodeTarget target, uint32_t *length);
