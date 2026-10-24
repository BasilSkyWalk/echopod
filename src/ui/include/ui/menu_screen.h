#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

typedef struct MenuItem {
    const char *label;
    ScreenAction (*select)(void *context);
    void *context;
    const char *(*value)(const void *context);
    bool opens;
    bool (*shown)(const void *context);
    const void *shown_context;
} MenuItem;

typedef struct MenuPanel {
    int width;
    void (*update)(void *context, uint32_t now_ms);
    void (*draw)(void *context, Canvas *canvas, Rect area);
    void *context;
} MenuPanel;

typedef struct MenuConfig {
    const char *title;
    const MenuItem *items;
    int item_count;
    const Theme *theme;
    MenuPanel panel;
} MenuConfig;

typedef struct MenuScreen {
    Screen screen;
    MenuConfig config;
    ListModel model;
    ListView list;
    uint32_t now_ms;
} MenuScreen;

Screen *menu_screen_init(MenuScreen *menu, MenuConfig config);
ScreenAction menu_open_screen(void *screen);
