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
    COVER_FLOW_TEXT_CAPACITY = 128
};

typedef struct CoverFlowOrder {
    uint32_t first_link;
    uint32_t count;
    bool linked;
} CoverFlowOrder;

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
    CoverFlowOrder order;
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
CoverFlowOrder cover_flow_order_make(const Library *library);
bool cover_flow_order_album(const Library *library, CoverFlowOrder order, uint32_t position,
                            uint16_t *album);
bool cover_flow_order_find(const Library *library, CoverFlowOrder order, uint16_t album,
                           uint32_t *position);
