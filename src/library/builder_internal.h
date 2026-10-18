#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/library_builder.h"

#define BUILDER_UNSET 0xFFFFFFFFu

enum {
    BUILDER_NONE = 0xFFFF,
    BUILDER_STRING_HEADER = 8,
    BUILDER_TRACK_SIZE = 36,
    BUILDER_FOLDER_SIZE = 8
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
    uint16_t reserved;
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
    uint32_t size;
    bool failed;
} Work;

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
} LibraryModel;

const char *builder_text(const LibraryBuilder *builder, uint32_t string);
TrackRecord *builder_track(const LibraryBuilder *builder, uint32_t index);
FolderNode *builder_folder(const LibraryBuilder *builder, uint32_t index);
uint32_t *builder_output_offset(const LibraryBuilder *builder, uint32_t string);
int builder_compare_keys(const LibraryBuilder *builder, uint32_t left, uint32_t right);
int builder_compare_raw(const LibraryBuilder *builder, uint32_t left, uint32_t right);
void *work_alloc(Work *work, uint32_t bytes);
void stable_sort(uint16_t *items, uint16_t *scratch, uint32_t count, IndexCompare compare,
                 const void *context);
bool builder_build_model(LibraryBuilder *builder, Work *work, LibraryModel *model);
BuilderResult builder_encode(LibraryBuilder *builder, const LibraryModel *model, Work *work,
                             ByteSink sink, uint32_t *length);
