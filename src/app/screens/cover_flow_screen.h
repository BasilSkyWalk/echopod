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
    COVER_FLOW_CAPACITY = 1024,
    COVER_FLOW_TEXT_CAPACITY = 128
};

typedef struct CoverFlowConfig {
    const Library *library;
    const Theme *theme;
    CoverCache *covers;
    FavoritesStore *favorites;
    Toast *toast;
    BrowsePool *browse;
    Player *player;
} CoverFlowConfig;

typedef struct CoverFlowScreen {
    Screen screen;
    CoverFlowConfig config;
    uint16_t albums[COVER_FLOW_CAPACITY];
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
} CoverFlowScreen;

Screen *cover_flow_screen_init(CoverFlowScreen *flow, CoverFlowConfig config);
int cover_flow_screen_build_order(const Library *library, uint16_t *albums, int capacity);
