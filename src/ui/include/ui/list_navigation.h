#pragma once

#include <stdint.h>

#include "core/key_tracker.h"
#include "ui/list_view.h"
#include "ui/screen.h"

typedef struct ListSelection {
    ScreenAction (*select)(void *context, int index);
    void *context;
} ListSelection;

ScreenAction list_navigation_handle(ListView *view, KeyEvent event, uint32_t now_ms,
                                    ListSelection selection);
