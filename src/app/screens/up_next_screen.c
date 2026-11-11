#include "up_next_screen.h"

#include <stdbool.h>

#include "core/format.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"

enum {
    DETAIL_MAX_WIDTH = 110,
    CHAPTER_NUMBER_CAPACITY = 8
};

typedef enum RowKind {
    ROW_PLAYING,
    ROW_ADDED,
    ROW_QUEUE
} RowKind;

typedef struct QueueWalk {
    PlayQueueCursor cursor;
    int steps;
} QueueWalk;

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static RowKind kind_of(const UpNextScreen *up_next, int index)
{
    if (index == 0) {
        return ROW_PLAYING;
    }
    return index <= up_next->added ? ROW_ADDED : ROW_QUEUE;
}

static bool wraps(const Player *player)
{
    return player->repeat == REPEAT_ALL;
}

static bool walk_to(const Player *player, QueueWalk *walk, int steps)
{
    while (walk->steps < steps) {
        PlayQueueCursor next;
        if (!play_queue_step(&player->queue, walk->cursor, 1, wraps(player), &next)) {
            return false;
        }
        walk->cursor = next;
        walk->steps++;
    }
    return true;
}

static QueueWalk start_walk(const Player *player)
{
    return (QueueWalk){.cursor = player->queue.cursor, .steps = 0};
}

static int count_queue_rows(const Player *player)
{
    QueueWalk walk = start_walk(player);
    PlayQueueCursor start = walk.cursor;
    int rows = 0;
    while (rows < UP_NEXT_QUEUE_ROWS && walk_to(player, &walk, rows + 1)) {
        if (walk.cursor.position == start.position &&
            walk.cursor.album_track == start.album_track) {
            break;
        }
        rows++;
    }
    return rows;
}

static void count_rows(UpNextScreen *up_next)
{
    const Player *player = up_next->config.player;
    up_next->chapters = player->has_track && player->chapters.count > 0;
    up_next->added = player->up_next.count;
    if (up_next->chapters) {
        up_next->count = player->chapters.count;
    } else {
        up_next->count = player->has_track ? 1 + up_next->added + count_queue_rows(player) : 0;
    }
    up_next->rows_first = -1;
}

static bool track_at(const UpNextScreen *up_next, int index, QueueWalk *walk, uint16_t *track)
{
    const Player *player = up_next->config.player;
    switch (kind_of(up_next, index)) {
    case ROW_PLAYING:
        *track = player->track;
        return true;
    case ROW_ADDED:
        *track = player->up_next.tracks[index - 1];
        return true;
    case ROW_QUEUE:
        break;
    }
    return walk_to(player, walk, index - up_next->added) &&
           play_queue_track_at(&player->queue, walk->cursor, track);
}

static const char *state_text(const Player *player)
{
    return i18n_text(player->state == PLAYER_PLAYING ? STR_BT_PLAYING : STR_MENU_PAUSED);
}

static void load_chapter_row(UpNextScreen *up_next, int row, int index)
{
    const Player *player = up_next->config.player;
    char number[CHAPTER_NUMBER_CAPACITY];
    char total[CHAPTER_NUMBER_CAPACITY];
    StringBuilder number_digits = string_builder_make(number, sizeof number);
    StringBuilder total_digits = string_builder_make(total, sizeof total);
    string_builder_append_unsigned(&number_digits, (uint32_t)index + 1u);
    string_builder_append_unsigned(&total_digits, player->chapters.count);
    const char *arguments[] = {number, total};
    StringBuilder builder = string_builder_make(up_next->labels[row], UP_NEXT_LABEL_CAPACITY);
    i18n_format(&builder, STR_MENU_CHAPTER_PROGRESS, arguments, 2);
    format_duration(mp4_chapter_start_ms(&player->chapters, (uint16_t)index), up_next->details[row],
                    UP_NEXT_DETAIL_CAPACITY);
}

static void load_row(UpNextScreen *up_next, int row, int index, QueueWalk *walk)
{
    if (up_next->chapters) {
        load_chapter_row(up_next, row, index);
        return;
    }
    const Library *library = up_next->config.library;
    char *label = up_next->labels[row];
    char *detail = up_next->details[row];
    label[0] = '\0';
    detail[0] = '\0';
    uint16_t track_index = 0;
    LibraryTrack track;
    if (!track_at(up_next, index, walk, &track_index) ||
        !library_read_track(library, track_index, &track)) {
        return;
    }
    library_read_string(library, track.title, label, UP_NEXT_LABEL_CAPACITY);
    if (index == 0) {
        StringBuilder builder = string_builder_make(detail, UP_NEXT_DETAIL_CAPACITY);
        string_builder_append(&builder, state_text(up_next->config.player));
        return;
    }
    LibraryGroup artist;
    if (library_read_group(library, LIBRARY_ARTISTS, track.artist, &artist)) {
        library_read_string(library, artist.name, detail, UP_NEXT_DETAIL_CAPACITY);
    }
}

static void refresh_rows(UpNextScreen *up_next)
{
    int top = list_view_top(&up_next->list);
    int count = min_of(min_of(list_view_rows(&up_next->list), UP_NEXT_ROWS), up_next->count - top);
    if (up_next->rows_first == top && up_next->rows_count >= count) {
        return;
    }
    QueueWalk walk = start_walk(up_next->config.player);
    for (int row = 0; row < count; row++) {
        load_row(up_next, row, top + row, &walk);
    }
    up_next->rows_first = top;
    up_next->rows_count = count;
}

static const char *cached_text(const UpNextScreen *up_next, int index, bool detail)
{
    int row = index - up_next->rows_first;
    if (up_next->rows_first < 0 || row < 0 || row >= up_next->rows_count) {
        return "";
    }
    return detail ? up_next->details[row] : up_next->labels[row];
}

static int row_count(const void *context)
{
    const UpNextScreen *up_next = context;
    return up_next->count;
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

static ScreenAction select_row(void *context, int index)
{
    UpNextScreen *up_next = context;
    Player *player = up_next->config.player;
    QueueWalk walk = start_walk(player);
    if (up_next->chapters) {
        player_seek(player, mp4_chapter_start_ms(&player->chapters, (uint16_t)index));
        return screen_action_pop();
    }
    switch (kind_of(up_next, index)) {
    case ROW_PLAYING:
        break;
    case ROW_ADDED:
        player_jump_to_up_next(player, (uint16_t)(index - 1));
        break;
    case ROW_QUEUE:
        if (walk_to(player, &walk, index - up_next->added)) {
            player_jump_to_queue(player, walk.cursor);
        }
        break;
    }
    return screen_action_pop();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    UpNextScreen *up_next = context;
    up_next->now_ms = now_ms;
    ListSelection selection = {.select = select_row, .hold = NULL, .context = up_next};
    ScreenAction action = list_navigation_handle(&up_next->list, event, now_ms, selection);
    refresh_rows(up_next);
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    UpNextScreen *up_next = context;
    up_next->now_ms = now_ms;
    count_rows(up_next);
    list_view_init(&up_next->list, &up_next->model, up_next->config.theme, now_ms);
    if (up_next->chapters) {
        list_view_select(&up_next->list, player_status(up_next->config.player).chapter, now_ms);
    }
    refresh_rows(up_next);
}

static void update(void *context, uint32_t now_ms)
{
    UpNextScreen *up_next = context;
    up_next->now_ms = now_ms;
    refresh_rows(up_next);
}

static void draw(void *context, Canvas *canvas)
{
    const UpNextScreen *up_next = context;
    const Theme *theme = up_next->config.theme;
    Rect content = theme_content_area(theme);
    header_draw(canvas, theme, i18n_text(up_next->chapters ? STR_MENU_CHAPTERS : STR_MENU_UP_NEXT));
    if (up_next->count == 0) {
        TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
        canvas_fill_rect(canvas, content, theme->colors.background);
        const char *message = i18n_text(STR_MENU_NOTHING_IS_PLAYING);
        Rect area = label_centered_area(style.font, content, message);
        label_draw(canvas, style, (Label){.area = area, .text = message});
        return;
    }
    list_view_draw(&up_next->list, canvas, content, up_next->now_ms);
}

Screen *up_next_screen_init(UpNextScreen *up_next, UpNextConfig config)
{
    up_next->config = config;
    up_next->count = 0;
    up_next->added = 0;
    up_next->chapters = false;
    up_next->rows_first = -1;
    up_next->rows_count = 0;
    up_next->now_ms = 0;
    up_next->model = (ListModel){.context = up_next,
                                 .count = row_count,
                                 .label = row_label,
                                 .detail = row_detail,
                                 .detail_max_width = DETAIL_MAX_WIDTH};
    list_view_init(&up_next->list, &up_next->model, config.theme, 0);
    up_next->screen = (Screen){
        .title = i18n_text(STR_MENU_UP_NEXT),
        .context = up_next,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &up_next->screen;
}
