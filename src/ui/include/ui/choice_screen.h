#pragma once

#include <stdint.h>

#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

typedef struct ChoiceConfig {
    const char *title;
    const char *const *options;
    int option_count;
    int (*current)(const void *context);
    void (*choose)(void *context, int index);
    void *context;
    const Theme *theme;
} ChoiceConfig;

typedef struct ChoiceScreen {
    Screen screen;
    ChoiceConfig config;
    ListModel model;
    ListView list;
    uint32_t now_ms;
} ChoiceScreen;

Screen *choice_screen_init(ChoiceScreen *choice, ChoiceConfig config);
