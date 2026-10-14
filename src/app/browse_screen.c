#include "browse_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "hal/clock.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"

enum {
    NO_LEVEL = -1
};

static const char EMPTY_MESSAGE[] = "Nothing here";

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static bool rows_current(const BrowseScreen *browse)
{
    const BrowseRows *rows = &browse->pool->rows;
    return rows->level == browse->level && rows->first == browse->list.top;
}

static void load_row(BrowseScreen *browse, int row, uint32_t position)
{
    BrowseRows *rows = &browse->pool->rows;
    BrowseText text = {rows->labels[row], BROWSE_LABEL_CAPACITY, rows->details[row],
                       BROWSE_DETAIL_CAPACITY};
    BrowseItem item;
    const Library *library = browse->pool->config.library;
    if (!browse_item(library, browse->query, position, &item) ||
        !browse_item_text(library, browse->query, item, text)) {
        rows->labels[row][0] = '\0';
        rows->details[row][0] = '\0';
    }
}

static void refresh_rows(BrowseScreen *browse)
{
    if (rows_current(browse)) {
        return;
    }
    BrowseRows *rows = &browse->pool->rows;
    int top = browse->list.top;
    int count = min_of(min_of(browse->list.visible_rows, BROWSE_ROWS), (int)browse->count - top);
    for (int row = 0; row < count; row++) {
        load_row(browse, row, (uint32_t)(top + row));
    }
    rows->level = browse->level;
    rows->first = top;
    rows->count = count;
}

static const char *cached_text(const BrowseScreen *browse, int index, bool detail)
{
    const BrowseRows *rows = &browse->pool->rows;
    int row = index - rows->first;
    if (rows->level != browse->level || row < 0 || row >= rows->count) {
        return "";
    }
    return detail ? rows->details[row] : rows->labels[row];
}

static int row_count(const void *context)
{
    const BrowseScreen *browse = context;
    return (int)browse->count;
}

static const char *row_label(const void *context, int index)
{
    return cached_text(context, index, false);
}

static const char *row_detail(const void *context, int index)
{
    const char *detail = cached_text(context, index, true);
    return detail[0] != '\0' ? detail : NULL;
}

static ScreenAction play_from(BrowseScreen *browse, int index)
{
    BrowsePoolConfig *config = &browse->pool->config;
    uint32_t seed = hal_clock_microseconds();
    if (!player_play_list(config->player, browse->query, (uint32_t)index, seed)) {
        return screen_action_none();
    }
    return screen_action_push(config->now_playing);
}

static ScreenAction select_row(void *context, int index)
{
    BrowseScreen *browse = context;
    BrowseItem item;
    BrowseQuery child;
    if (!browse_item(browse->pool->config.library, browse->query, (uint32_t)index, &item)) {
        return screen_action_none();
    }
    if (item.kind == BROWSE_ITEM_TRACK) {
        return play_from(browse, index);
    }
    if (!browse_child(browse->query, item, &child)) {
        return screen_action_none();
    }
    Screen *next = browse_pool_open(browse->pool, browse->level + 1, child);
    return next != NULL ? screen_action_push(next) : screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    BrowseScreen *browse = context;
    ListSelection selection = {.select = select_row, .hold = NULL, .context = browse};
    return list_navigation_handle(&browse->list, event, now_ms, selection);
}

static void enter(void *context, uint32_t now_ms)
{
    BrowseScreen *browse = context;
    browse->pool->rows.level = NO_LEVEL;
    browse->now_ms = now_ms;
    list_view_restart_marquee(&browse->list, now_ms);
    refresh_rows(browse);
}

static void update(void *context, uint32_t now_ms)
{
    BrowseScreen *browse = context;
    browse->now_ms = now_ms;
    refresh_rows(browse);
}

static void draw_empty(Canvas *canvas, const Theme *theme, Rect content)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    canvas_fill_rect(canvas, content, theme->colors.background);
    Rect area = label_centered_area(style.font, content, EMPTY_MESSAGE);
    label_draw(canvas, style, (Label){.area = area, .text = EMPTY_MESSAGE});
}

static void draw(void *context, Canvas *canvas)
{
    const BrowseScreen *browse = context;
    const Theme *theme = browse->pool->config.theme;
    Rect content = theme_content_area(theme);
    header_draw(canvas, theme, browse->title);
    if (browse->count == 0) {
        draw_empty(canvas, theme, content);
        return;
    }
    list_view_draw(&browse->list, canvas, content, browse->now_ms);
}

void browse_pool_init(BrowsePool *pool, BrowsePoolConfig config)
{
    pool->config = config;
    pool->rows.level = NO_LEVEL;
}

Screen *browse_pool_open(BrowsePool *pool, int level, BrowseQuery query)
{
    if (level < 0 || level >= BROWSE_DEPTH) {
        return NULL;
    }
    BrowseScreen *browse = &pool->screens[level];
    browse->pool = pool;
    browse->level = level;
    browse->query = query;
    browse->count = browse_count(pool->config.library, query);
    browse_title(pool->config.library, query, browse->title, sizeof browse->title);
    browse->model = (ListModel){
        .context = browse, .count = row_count, .label = row_label, .detail = row_detail};
    list_view_init(&browse->list, &browse->model, pool->config.theme, 0);
    browse->screen = (Screen){
        .title = browse->title,
        .context = browse,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &browse->screen;
}
