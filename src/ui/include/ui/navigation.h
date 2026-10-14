#pragma once

#include <stdbool.h>

#include "core/key_tracker.h"
#include "ui/screen.h"

typedef enum NavigationIntent {
    NAVIGATION_NONE,
    NAVIGATION_PREVIOUS,
    NAVIGATION_NEXT,
    NAVIGATION_SELECT,
    NAVIGATION_BACK,
    NAVIGATION_HOME
} NavigationIntent;

typedef struct Navigation {
    NavigationIntent intent;
    bool repeated;
} Navigation;

Navigation navigation_from_key(KeyEvent event);
ScreenAction navigation_exit_action(KeyEvent event);
