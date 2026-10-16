#include "ui/menu_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/list_navigation.h"

static bool is_shown(const MenuItem *item)
{
    return item->shown == NULL || item->shown(item->shown_context);
}

static int item_count(const void *context)
{
    const MenuScreen *menu = context;
    int count = 0;
    for (int index = 0; index < menu->config.item_count; index++) {
        count += is_shown(&menu->config.items[index]) ? 1 : 0;
    }
    return count;
}

static const MenuItem *shown_item(const MenuScreen *menu, int position)
{
    int remaining = position;
    for (int index = 0; index < menu->config.item_count; index++) {
        const MenuItem *item = &menu->config.items[index];
        if (!is_shown(item)) {
            continue;
        }
        if (remaining == 0) {
            return item;
        }
        remaining--;
    }
    return &menu->config.items[menu->config.item_count - 1];
}

static const char *item_label(const void *context, int index)
{
    return shown_item(context, index)->label;
}

static const char *item_value(const void *context, int index)
{
    const MenuItem *item = shown_item(context, index);
    return item->value != NULL ? item->value(item->context) : NULL;
}

static bool item_opens(const void *context, int index)
{
    return shown_item(context, index)->opens;
}

static ScreenAction select_item(void *context, int index)
{
    const MenuItem *item = shown_item(context, index);
    return item->select != NULL ? item->select(item->context) : screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    MenuScreen *menu = context;
    ListSelection selection = {.select = select_item, .hold = NULL, .context = menu};
    return list_navigation_handle(&menu->list, event, now_ms, selection);
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
    const MenuPanel *panel = &menu->config.panel;
    if (panel->width > 0 && panel->update != NULL) {
        panel->update(panel->context, now_ms);
    }
}

static void draw(void *context, Canvas *canvas)
{
    const MenuScreen *menu = context;
    const Theme *theme = menu->config.theme;
    const MenuPanel *panel = &menu->config.panel;
    Rect content = theme_content_area(theme);
    header_draw(canvas, theme, menu->config.title);
    if (panel->width <= 0 || panel->draw == NULL) {
        list_view_draw(&menu->list, canvas, content, menu->now_ms);
        return;
    }
    Rect list = {content.x, content.y, content.width - panel->width, content.height};
    Rect side = {list.x + list.width, content.y, panel->width, content.height};
    list_view_draw(&menu->list, canvas, list, menu->now_ms);
    panel->draw(panel->context, canvas, side);
}

Screen *menu_screen_init(MenuScreen *menu, MenuConfig config)
{
    *menu = (MenuScreen){.config = config};
    menu->model = (ListModel){.context = menu,
                              .count = item_count,
                              .label = item_label,
                              .detail = item_value,
                              .opens = item_opens};
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
