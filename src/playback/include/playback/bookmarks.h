#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    BOOKMARKS_CAPACITY = 128,
    BOOKMARK_RECORD_SIZE = 16,
    BOOKMARKS_FILE_CAPACITY = 12 + BOOKMARKS_CAPACITY * BOOKMARK_RECORD_SIZE + 4,
    BOOKMARK_FINISHED = 0x01
};

typedef struct Bookmark {
    uint32_t key;
    uint32_t track_key;
    uint32_t position_ms;
    uint16_t chapter;
    uint8_t flags;
} Bookmark;

typedef struct Bookmarks {
    Bookmark entries[BOOKMARKS_CAPACITY];
    uint16_t count;
} Bookmarks;

void bookmarks_clear(Bookmarks *bookmarks);
bool bookmarks_find(const Bookmarks *bookmarks, uint32_t key, Bookmark *bookmark);
void bookmarks_put(Bookmarks *bookmarks, Bookmark bookmark);
bool bookmark_finished(Bookmark bookmark);
uint32_t bookmarks_encode(const Bookmarks *bookmarks, uint8_t *bytes);
bool bookmarks_decode(const uint8_t *bytes, uint32_t length, Bookmarks *bookmarks);
