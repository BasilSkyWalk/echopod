#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "library/library.h"

typedef enum BrowseKind {
    BROWSE_ARTISTS,
    BROWSE_ALBUM_ARTISTS,
    BROWSE_GENRES,
    BROWSE_ALBUMS,
    BROWSE_ALL_SONGS,
    BROWSE_GROUP_ALBUMS,
    BROWSE_ALBUM_TRACKS,
    BROWSE_FOLDER
} BrowseKind;

typedef struct BrowseQuery {
    BrowseKind kind;
    LibrarySection group;
    uint16_t subject;
    uint16_t artist;
} BrowseQuery;

typedef enum BrowseItemKind {
    BROWSE_ITEM_GROUP,
    BROWSE_ITEM_ALBUM,
    BROWSE_ITEM_TRACK,
    BROWSE_ITEM_FOLDER
} BrowseItemKind;

typedef struct BrowseItem {
    BrowseItemKind kind;
    LibrarySection group;
    uint16_t index;
} BrowseItem;

typedef struct BrowseText {
    char *label;
    size_t label_capacity;
    char *detail;
    size_t detail_capacity;
} BrowseText;

BrowseQuery browse_root_query(BrowseKind kind);
uint32_t browse_count(const Library *library, BrowseQuery query);
bool browse_item(const Library *library, BrowseQuery query, uint32_t position, BrowseItem *item);
bool browse_child(BrowseQuery parent, BrowseItem item, BrowseQuery *child);
bool browse_item_text(const Library *library, BrowseQuery query, BrowseItem item, BrowseText text);
bool browse_title(const Library *library, BrowseQuery query, char *buffer, size_t capacity);
