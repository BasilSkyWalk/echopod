#pragma once

#include <stdint.h>

#include "favorites_store.h"
#include "library/favorites.h"
#include "library/library_browse.h"
#include "player.h"
#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    BROWSE_DEPTH = 6,
    BROWSE_ROWS = 8,
    BROWSE_LABEL_CAPACITY = 128,
    BROWSE_DETAIL_CAPACITY = 16,
    BROWSE_TITLE_CAPACITY = 96
};

typedef struct BrowseRows {
    int level;
    int first;
    int count;
    char labels[BROWSE_ROWS][BROWSE_LABEL_CAPACITY];
    char details[BROWSE_ROWS][BROWSE_DETAIL_CAPACITY];
} BrowseRows;

typedef struct BrowsePool BrowsePool;

typedef struct BrowseScreen {
    Screen screen;
    BrowsePool *pool;
    int level;
    BrowseQuery query;
    uint32_t count;
    bool favorites_only;
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
} BrowsePoolConfig;

struct BrowsePool {
    BrowsePoolConfig config;
    BrowseRows rows;
    BrowseScreen screens[BROWSE_DEPTH];
    uint16_t favorite_albums[FAVORITES_CAPACITY];
};

void browse_pool_init(BrowsePool *pool, BrowsePoolConfig config);
Screen *browse_pool_open(BrowsePool *pool, int level, BrowseQuery query);
Screen *browse_pool_open_favorites(BrowsePool *pool, int level);
