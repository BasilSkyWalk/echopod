#include "ui/menu_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/navigation.h"

static int item_count(const void *context)
{
    const MenuScreen *menu = context;
    return menu->config.item_count;
}

static const char *item_label(const void *context, int index)
{
    const MenuScreen *menu = context;
    return menu->config.items[index].label;
}

static const char *item_value(const void *context, int index)
{
    const MenuScreen *menu = context;
    const MenuItem *item = &menu->config.items[index];
    return item->value != NULL ? item->value(item->context) : NULL;
}

static ScreenAction select_current(MenuScreen *menu)
{
    if (menu->config.item_count == 0) {
        return screen_action_none();
    }
    const MenuItem *item = &menu->config.items[menu->list.selected];
    return item->select != NULL ? item->select(item->context) : screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    MenuScreen *menu = context;
    Navigation navigation = navigation_from_key(event);
    switch (navigation.intent) {
    case NAVIGATION_PREVIOUS:
        list_view_move(&menu->list, -1, !navigation.repeated, now_ms);
        return screen_action_none();
    case NAVIGATION_NEXT:
        list_view_move(&menu->list, 1, !navigation.repeated, now_ms);
        return screen_action_none();
    case NAVIGATION_SELECT:
        return select_current(menu);
    case NAVIGATION_BACK:
        return screen_action_pop();
    case NAVIGATION_HOME:
        return screen_action_pop_to_root();
    case NAVIGATION_NONE:
        return screen_action_none();
    }
    return screen_action_none();
}

static void enter(void *context, uint32_t now_ms)
{
    MenuScreen *menu = context;
    menu->now_ms = now_ms;
    list_view_restart_marquee(&menu->list, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    MenuScreen *menu = context;
    menu->now_ms = now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    const MenuScreen *menu = context;
    const Theme *theme = menu->config.theme;
    header_draw(canvas, theme, menu->config.title);
    list_view_draw(&menu->list, canvas, theme_content_area(theme), menu->now_ms);
}

Screen *menu_screen_init(MenuScreen *menu, MenuConfig config)
{
    *menu = (MenuScreen){.config = config};
    menu->model = (ListModel){
        .context = menu, .count = item_count, .label = item_label, .detail = item_value};
    list_view_init(&menu->list, &menu->model, config.theme, 0);
    menu->screen = (Screen){
        .title = config.title,
        .context = menu,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &menu->screen;
}

ScreenAction menu_open_screen(void *screen)
{
    return screen_action_push(screen);
}
