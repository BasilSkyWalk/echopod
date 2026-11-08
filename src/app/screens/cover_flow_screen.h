#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "browse_screen.h"
#include "cover_cache.h"
#include "favorites_store.h"
#include "library/library.h"
#include "player.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    COVER_FLOW_TEXT_CAPACITY = 128,
    COVER_FLOW_TILES = COVER_CACHE_SLOTS
};

typedef enum CoverFlowKind {
    COVER_FLOW_ALBUMS,
    COVER_FLOW_PLAYLIST,
    COVER_FLOW_FOLDERS
} CoverFlowKind;

typedef struct CoverFlowOrder {
    uint32_t first_link;
    uint32_t count;
    bool linked;
    CoverFlowKind kind;
    uint32_t links;
    uint16_t folder;
} CoverFlowOrder;

typedef struct CoverFlowTile {
    int position;
    uint16_t album;
    uint16_t item;
    uint32_t link;
} CoverFlowTile;

typedef struct CoverFlowConfig {
    const Library *library;
    const Theme *theme;
    CoverCache *covers;
    FavoritesStore *favorites;
    Toast *toast;
    BrowsePool *browse;
    Player *player;
    const uint8_t *options;
} CoverFlowConfig;

typedef struct CoverFlowScreen {
    Screen screen;
    CoverFlowConfig config;
    CoverFlowOrder order;
    CoverFlowOrder artist_order;
    BrowseQuery parent;
    int32_t order_key;
    uint16_t origin;
    int first;
    int album_level;
    int held_repeats;
    int count;
    int target;
    int32_t position;
    bool positioned;
    uint32_t last_ms;
    uint32_t now_ms;
    int text_position;
    uint32_t text_changed_ms;
    char title[COVER_FLOW_TEXT_CAPACITY];
    char artist[COVER_FLOW_TEXT_CAPACITY];
    CoverFlowTile tiles[COVER_FLOW_TILES];
    uint16_t playing_track;
    CoverFlowTile playing_tile;
} CoverFlowScreen;

Screen *cover_flow_screen_init(CoverFlowScreen *flow, CoverFlowConfig config);
Screen *cover_flow_screen_open_all(CoverFlowScreen *flow);
Screen *cover_flow_screen_open_group(CoverFlowScreen *flow, LibrarySection group, uint16_t subject,
                                     int album_level);
CoverFlowOrder cover_flow_order_make(const Library *library);
bool cover_flow_order_album(const Library *library, CoverFlowOrder order, uint32_t position,
                            uint16_t *album);
bool cover_flow_order_find(const Library *library, CoverFlowOrder order, uint16_t album,
                           uint32_t *position);
