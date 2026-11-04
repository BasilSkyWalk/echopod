#include "music_quiz_screen.h"

#include <string.h>

#include "core/i18n.h"
#include "core/string_builder.h"
#include "library/library_browse.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"

static uint32_t random_next(MusicQuizScreen *quiz)
{
    uint32_t value = quiz->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    quiz->random_state = value != 0 ? value : 1;
    return quiz->random_state;
}

static int item_count(const void *context)
{
    (void)context;
    return (int)MUSIC_QUIZ_CHOICE_COUNT;
}

static const char *item_label(const void *context, int index)
{
    const MusicQuizScreen *quiz = context;
    return quiz->titles[index];
}

static const char *item_detail(const void *context, int index)
{
    const MusicQuizScreen *quiz = context;
    if (quiz->revealed && quiz->choices[index] == quiz->answer) {
        return i18n_text(STR_MENU_QUIZ_ANSWER);
    }
    return NULL;
}

static bool load_title(MusicQuizScreen *quiz, uint32_t position, uint16_t *track_index,
                       char *title, size_t capacity)
{
    BrowseQuery query = browse_root_query(BROWSE_ALL_SONGS);
    BrowseItem item;
    LibraryTrack track;
    if (!browse_item(quiz->player->library, query, position, &item) ||
        item.kind != BROWSE_ITEM_TRACK ||
        !library_read_track(quiz->player->library, item.index, &track) ||
        !library_read_string(quiz->player->library, track.title, title, capacity)) {
        return false;
    }
    *track_index = item.index;
    return true;
}

static void start_round(MusicQuizScreen *quiz, uint32_t now_ms)
{
    uint32_t count = library_music_count(quiz->player->library, LIBRARY_TITLE_ORDER);
    quiz->has_tracks = count >= MUSIC_QUIZ_CHOICE_COUNT;
    quiz->revealed = false;
    quiz->correct = false;
    if (!quiz->has_tracks) {
        return;
    }
    BrowseQuery query = browse_root_query(BROWSE_ALL_SONGS);
    uint32_t answer_position = random_next(quiz) % count;
    char answer_title[MUSIC_QUIZ_TITLE_CAPACITY];
    if (!load_title(quiz, answer_position, &quiz->answer, answer_title, sizeof answer_title)) {
        quiz->has_tracks = false;
        return;
    }
    uint8_t answer_slot = (uint8_t)(random_next(quiz) % MUSIC_QUIZ_CHOICE_COUNT);
    quiz->choices[answer_slot] = quiz->answer;
    for (uint8_t slot = 0; slot < MUSIC_QUIZ_CHOICE_COUNT; slot++) {
        if (slot == answer_slot) {
            continue;
        }
        uint32_t position = count > 1 ? random_next(quiz) % (count - 1) : answer_position;
        if (count > 1 && position >= answer_position) {
            position++;
        }
        if (!load_title(quiz, position, &quiz->choices[slot], quiz->titles[slot],
                        sizeof quiz->titles[slot])) {
            quiz->has_tracks = false;
            return;
        }
    }
    strcpy(quiz->titles[answer_slot], answer_title);
    player_play_list(quiz->player, query, answer_position, random_next(quiz) ^ now_ms);
    list_view_select(&quiz->list, 0, now_ms);
}

static ScreenAction choose(void *context, int index)
{
    MusicQuizScreen *quiz = context;
    if (quiz->revealed || !quiz->has_tracks) {
        return screen_action_none();
    }
    quiz->correct = quiz->choices[index] == quiz->answer;
    quiz->revealed = true;
    if (quiz->correct) {
        quiz->correct_count++;
    }
    player_toggle_pause(quiz->player);
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    MusicQuizScreen *quiz = context;
    if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
        return screen_action_pop();
    }
    if (quiz->revealed && event.key == KEY_PLAY && event.gesture == KEY_GESTURE_PRESS) {
        start_round(quiz, now_ms);
        return screen_action_none();
    }
    ListSelection selection = {.select = choose, .context = quiz};
    return list_navigation_handle(&quiz->list, event, now_ms, selection);
}

static void enter(void *context, uint32_t now_ms)
{
    MusicQuizScreen *quiz = context;
    quiz->random_state ^= now_ms + 0x9E3779B9u;
    quiz->correct_count = 0;
    start_round(quiz, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    MusicQuizScreen *quiz = context;
    quiz->now_ms = now_ms;
    if (!quiz->has_tracks) {
        return;
    }
    StringBuilder builder = string_builder_make(quiz->footer, sizeof quiz->footer);
    if (quiz->revealed) {
        string_builder_append(&builder,
                              i18n_text(quiz->correct ? STR_MENU_QUIZ_RIGHT : STR_MENU_QUIZ_WRONG));
        string_builder_append(&builder, " · Play: next");
    } else {
        string_builder_append_unsigned(&builder, quiz->correct_count);
        string_builder_append(&builder, " correct");
    }
    (void)now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    const MusicQuizScreen *quiz = context;
    const Theme *theme = quiz->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, quiz->screen.title);
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    const char *prompt = quiz->has_tracks ? i18n_text(STR_MENU_QUIZ_PROMPT)
                                           : i18n_text(STR_MENU_QUIZ_NO_SONGS);
    label_draw(canvas, style,
               (Label){.area = {12, 34, 296, 20}, .text = prompt});
    if (quiz->has_tracks) {
        list_view_draw(&quiz->list, canvas, (Rect){0, 55, 320, 100}, quiz->now_ms);
        label_draw(canvas,
                   (TextStyle){.font = theme->fonts.small, .color = theme->colors.text_tertiary},
                   (Label){.area = {12, 152, 296, 16}, .text = quiz->footer});
    }
}

Screen *music_quiz_screen_init(MusicQuizScreen *quiz, Player *player, const Theme *theme)
{
    *quiz = (MusicQuizScreen){.theme = theme, .player = player, .random_state = 0xA341316Cu};
    quiz->model = (ListModel){.context = quiz,
                              .count = item_count,
                              .label = item_label,
                              .detail = item_detail};
    list_view_init(&quiz->list, &quiz->model, theme, 0);
    quiz->screen = (Screen){.title = i18n_text(STR_MENU_MUSIC_QUIZ),
                            .context = quiz,
                            .enter = enter,
                            .handle_key = handle_key,
                            .update = update,
                            .draw = draw};
    return &quiz->screen;
}
