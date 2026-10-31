#pragma once

#include <stdint.h>

#include "favorites_store.h"
#include "library/favorites.h"
#include "library/library_browse.h"
#include "player.h"
#include "spoken_store.h"
#include "ui/list_view.h"
#include "ui/menu_screen.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    BROWSE_DEPTH = 6,
    BROWSE_ROWS = 11,
    BROWSE_ROW_CACHES = 2,
    BROWSE_LABEL_CAPACITY = 128,
    BROWSE_DETAIL_CAPACITY = 16,
    BROWSE_TITLE_CAPACITY = 96,
    SONG_ACTION_COUNT = 2
};

typedef struct BrowseRows {
    int level;
    uint32_t used;
    int first;
    int count;
    char labels[BROWSE_ROWS][BROWSE_LABEL_CAPACITY];
    char details[BROWSE_ROWS][BROWSE_DETAIL_CAPACITY];
    bool opens[BROWSE_ROWS];
} BrowseRows;

typedef struct BrowsePool BrowsePool;

typedef struct BrowseScreen {
    Screen screen;
    BrowsePool *pool;
    int level;
    BrowseQuery query;
    uint32_t count;
    bool favorites_only;
    SpokenKind shelf;
    bool shelf_root;
    char title[BROWSE_TITLE_CAPACITY];
    ListModel model;
    ListView list;
    uint32_t now_ms;
} BrowseScreen;

typedef struct BrowsePoolConfig {
    const Library *library;
    const Theme *theme;
    Player *player;
    Screen *now_playing;
    FavoritesStore *favorites;
    Toast *toast;
    SpokenStore *spoken;
} BrowsePoolConfig;

typedef struct SongActions {
    MenuScreen menu;
    MenuItem items[SONG_ACTION_COUNT];
    uint16_t track;
    uint32_t now_ms;
    char title[BROWSE_LABEL_CAPACITY];
} SongActions;

struct BrowsePool {
    BrowsePoolConfig config;
    BrowseRows rows[BROWSE_ROW_CACHES];
    uint32_t row_uses;
    BrowseScreen screens[BROWSE_DEPTH];
    uint16_t favorite_albums[FAVORITES_CAPACITY];
    uint16_t favorite_count;
    bool favorites_collected;
    uint32_t favorites_revision;
    SongActions song_actions;
};

void browse_pool_init(BrowsePool *pool, BrowsePoolConfig config);
Screen *browse_pool_open(BrowsePool *pool, int level, BrowseQuery query);
Screen *browse_pool_open_favorites(BrowsePool *pool, int level);
Screen *browse_pool_open_shelf(BrowsePool *pool, int level, SpokenKind shelf);
