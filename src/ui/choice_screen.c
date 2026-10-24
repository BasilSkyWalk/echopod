#include "ui/choice_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/list_navigation.h"

static const char CHECK_MARK[] = "\xE2\x9C\x93";

static int option_count(const void *context)
{
    const ChoiceScreen *choice = context;
    return choice->config.option_count;
}

static const char *option_label(const void *context, int index)
{
    const ChoiceScreen *choice = context;
    return choice->config.options[index];
}

static const char *option_mark(const void *context, int index)
{
    const ChoiceScreen *choice = context;
    return choice->config.current(choice->config.context) == index ? CHECK_MARK : NULL;
}

static ScreenAction choose_option(void *context, int index)
{
    ChoiceScreen *choice = context;
    choice->config.choose(choice->config.context, index);
    return screen_action_pop();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    ChoiceScreen *choice = context;
    ListSelection selection = {.select = choose_option, .hold = NULL, .context = choice};
    return list_navigation_handle(&choice->list, event, now_ms, selection);
}

static void enter(void *context, uint32_t now_ms)
{
    ChoiceScreen *choice = context;
    choice->now_ms = now_ms;
    list_view_select(&choice->list, choice->config.current(choice->config.context), now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    ChoiceScreen *choice = context;
    choice->now_ms = now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    const ChoiceScreen *choice = context;
    const Theme *theme = choice->config.theme;
    header_draw(canvas, theme, choice->config.title);
    list_view_draw(&choice->list, canvas, theme_content_area(theme), choice->now_ms);
}

Screen *choice_screen_init(ChoiceScreen *choice, ChoiceConfig config)
{
    choice->config = config;
    choice->now_ms = 0;
    choice->model = (ListModel){
        .context = choice, .count = option_count, .label = option_label, .detail = option_mark};
    list_view_init(&choice->list, &choice->model, config.theme, 0);
    choice->screen = (Screen){
        .title = config.title,
        .context = choice,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &choice->screen;
}
