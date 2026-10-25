#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "library/library.h"
#include "library/library_browse.h"
#include "playback/bookmarks.h"
#include "playback/spoken.h"
#include "player.h"

extern const char SPOKEN_STORE_PATH[];

enum {
    SPOKEN_PUT_INTERVAL_MS = 5000,
    SPOKEN_SAVE_INTERVAL_MS = 30000
};

typedef struct SpokenFollow {
    bool known;
    bool active;
    uint16_t track;
    uint32_t duration_ms;
    Bookmark bookmark;
} SpokenFollow;

typedef struct SpokenStore {
    Bookmarks bookmarks;
    const Library *library;
    SpokenFollow follow;
    PlayerState last_state;
    bool dirty;
    uint32_t last_put_ms;
    uint32_t last_save_ms;
} SpokenStore;

typedef struct SpokenRow {
    SpokenKind shelf;
    BrowseItem item;
    char *detail;
    size_t capacity;
} SpokenRow;

void spoken_store_load(SpokenStore *store, const Library *library);
void spoken_store_follow(SpokenStore *store, PlayerStatus status, uint32_t now_ms);
void spoken_store_flush(SpokenStore *store, PlayerStatus status, uint32_t now_ms);
PlayerResume spoken_store_resume(SpokenStore *store);
bool spoken_store_track(const SpokenStore *store, uint16_t track, Bookmark *bookmark);
bool spoken_store_book(const SpokenStore *store, uint16_t folder, Bookmark *bookmark);
uint16_t spoken_store_resume_chapter(const SpokenStore *store, uint16_t folder);
bool spoken_store_describe(const SpokenStore *store, SpokenRow row);
