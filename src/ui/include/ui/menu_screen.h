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
    int min_height;
    void (*update)(void *context, uint32_t now_ms);
    void (*draw)(void *context, Canvas *canvas, Rect area);
    void *context;
} MenuPanel;

typedef struct MenuScreen MenuScreen;

typedef struct MenuScene {
    bool (*update)(void *context, uint32_t now_ms);
    bool (*draw)(void *context, Canvas *canvas, MenuScreen *menu);
    void *context;
    bool on;
} MenuScene;

typedef struct MenuConfig {
    const char *title;
    const MenuItem *items;
    int item_count;
    const Theme *theme;
    MenuPanel panel;
    MenuScene *scene;
} MenuConfig;

struct MenuScreen {
    Screen screen;
    MenuConfig config;
    ListModel model;
    ListView list;
    uint32_t now_ms;
};

Screen *menu_screen_init(MenuScreen *menu, MenuConfig config);
ScreenAction menu_open_screen(void *screen);
void menu_screen_scene_list(void *context, Canvas *canvas, Rect area);
