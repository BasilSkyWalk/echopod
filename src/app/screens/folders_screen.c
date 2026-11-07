#include "folders_screen.h"

#include <stddef.h>

#include "core/i18n.h"
#include "core/string_builder.h"
#include "core/volume_path.h"
#include "folders_store.h"
#include "hal/clock.h"
#include "hal/storage.h"
#include "library/music_fingerprint.h"
#include "library/spoken_path.h"
#include "ui/header.h"
#include "ui/list_navigation.h"

enum {
    NUMBER_CAPACITY = 12,
    MINIMUM_ROOM = 8
};

static const char CARD_ROOT[] = "/";
static const char INTERNAL_ROOT[] = "/" VOLUME_INTERNAL_NAME;
static const char CHECK_MARK[] = "\xE2\x9C\x93";

static size_t text_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int compare_texts(const char *left, const char *right)
{
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)*a - (int)*b;
}

static const char *label_of(const FoldersScreen *folders, int index)
{
    return folders->names + folders->rows[index].label;
}

static void add_row(FoldersScreen *folders, const MusicFolders *stored, const char *root,
                    const char *name, int first)
{
    uint32_t room = FOLDER_NAMES_CAPACITY - folders->used;
    if (folders->count == FOLDERS_SCREEN_ROWS || room < MINIMUM_ROOM) {
        return;
    }
    FolderRow row = {.key = 0, .label = (uint16_t)folders->used};
    (void)music_folders_key(root, name, &row.key);
    row.off = music_folders_has(stored, row.key);
    row.was_off = row.off;
    StringBuilder label = string_builder_make(folders->names + folders->used, room);
    if (root != CARD_ROOT) {
        string_builder_append(&label, i18n_text(STR_LIBRARY_INTERNAL_STORAGE));
        string_builder_append(&label, "/");
    }
    string_builder_append(&label, name);
    folders->used += (uint32_t)label.length + 1;
    int at = folders->count++;
    while (at > first && compare_texts(label_of(folders, at - 1), folders->names + row.label) > 0) {
        folders->rows[at] = folders->rows[at - 1];
        at--;
    }
    folders->rows[at] = row;
}

static bool list_root(FoldersScreen *folders, const MusicFolders *stored, const char *root)
{
    HalDir dir;
    HalDirEntry entry;
    bool internal = false;
    int first = folders->count;
    if (!hal_dir_open(&dir, root)) {
        return false;
    }
    while (hal_dir_next(&dir, &entry)) {
        size_t length = text_length(entry.name);
        bool card = root == CARD_ROOT;
        if (!entry.directory || entry.hidden || entry.name[0] == '.' ||
            !music_fingerprint_counts(entry.name, true)) {
            continue;
        }
        if (card && volume_is_internal_name(entry.name, length)) {
            internal = true;
        } else if (!card || spoken_kind_of_name(entry.name, length) == SPOKEN_NONE) {
            add_row(folders, stored, root, entry.name, first);
        }
    }
    hal_dir_close(&dir);
    return internal;
}

static void list_folders(FoldersScreen *folders)
{
    MusicFolders stored;
    folders_store_load(&stored);
    folders->count = 0;
    folders->used = 0;
    if (list_root(folders, &stored, CARD_ROOT)) {
        (void)list_root(folders, &stored, INTERNAL_ROOT);
    }
    folders->listed = true;
}

static int row_count(const void *context)
{
    const FoldersScreen *folders = context;
    return folders->count > 0 ? folders->count : 1;
}

static const char *row_label(const void *context, int index)
{
    const FoldersScreen *folders = context;
    return folders->count > 0 ? label_of(folders, index) : i18n_text(STR_SETTINGS_NO_FOLDERS);
}

static const char *row_mark(const void *context, int index)
{
    const FoldersScreen *folders = context;
    return folders->count > 0 && !folders->rows[index].off ? CHECK_MARK : NULL;
}

static ScreenAction toggle(void *context, int index)
{
    FoldersScreen *folders = context;
    if (index < folders->count) {
        folders->rows[index].off = !folders->rows[index].off;
    }
    return screen_action_none();
}

static bool shown_key(const FoldersScreen *folders, uint32_t key)
{
    for (int index = 0; index < folders->count; index++) {
        if (folders->rows[index].key == key) {
            return true;
        }
    }
    return false;
}

static bool rows_changed(const FoldersScreen *folders)
{
    for (int index = 0; index < folders->count; index++) {
        if (folders->rows[index].off != folders->rows[index].was_off) {
            return true;
        }
    }
    return false;
}

static void save(FoldersScreen *folders)
{
    MusicFolders stored;
    MusicFolders kept = {.count = 0};
    folders_store_load(&stored);
    for (int index = 0; index < folders->count; index++) {
        if (folders->rows[index].off) {
            kept.keys[kept.count++] = folders->rows[index].key;
        }
    }
    for (uint32_t index = 0; index < stored.count; index++) {
        if (kept.count < MUSIC_FOLDERS_MAX && !shown_key(folders, stored.keys[index])) {
            kept.keys[kept.count++] = stored.keys[index];
        }
    }
    if (!folders_store_save(&kept)) {
        toast_show(folders->config.toast, i18n_text(STR_SETTINGS_SAVE_FAILED),
                   hal_clock_milliseconds());
        return;
    }
    for (int index = 0; index < folders->count; index++) {
        folders->rows[index].was_off = folders->rows[index].off;
    }
    if (folders->config.rescan_requested != NULL) {
        *folders->config.rescan_requested = true;
    }
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    FoldersScreen *folders = context;
    ListSelection selection = {.select = toggle, .hold = NULL, .context = folders};
    ScreenAction action = list_navigation_handle(&folders->list, event, now_ms, selection);
    bool leaving = action.type == SCREEN_ACTION_POP || action.type == SCREEN_ACTION_POP_TO_ROOT;
    if (leaving && rows_changed(folders)) {
        save(folders);
    }
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    FoldersScreen *folders = context;
    folders->now_ms = now_ms;
    list_view_select(&folders->list, 0, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    FoldersScreen *folders = context;
    folders->now_ms = now_ms;
}

static void draw(void *context, Canvas *canvas)
{
    const FoldersScreen *folders = context;
    const Theme *theme = folders->config.theme;
    header_draw(canvas, theme, folders->screen.title);
    list_view_draw(&folders->list, canvas, theme_content_area(theme), folders->now_ms);
}

Screen *folders_screen_init(FoldersScreen *folders, FoldersConfig config)
{
    *folders = (FoldersScreen){.self = folders, .config = config};
    folders->model =
        (ListModel){.context = folders, .count = row_count, .label = row_label, .detail = row_mark};
    list_view_init(&folders->list, &folders->model, config.theme, 0);
    folders->screen = (Screen){
        .title = i18n_text(STR_SETTINGS_MUSIC_FOLDERS),
        .context = folders,
        .select_on_press = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &folders->screen;
}

ScreenAction folders_screen_open(void *context)
{
    FoldersScreen *folders = context;
    list_folders(folders);
    return screen_action_push(&folders->screen);
}

static void number_text(char *text, uint32_t value)
{
    StringBuilder builder = string_builder_make(text, NUMBER_CAPACITY);
    string_builder_append_unsigned(&builder, value);
}

const char *folders_screen_summary(const void *context)
{
    FoldersScreen *folders = ((const FoldersScreen *)context)->self;
    if (!folders->listed) {
        list_folders(folders);
    }
    uint32_t shown = 0;
    for (int index = 0; index < folders->count; index++) {
        shown += folders->rows[index].off ? 0u : 1u;
    }
    if (shown == (uint32_t)folders->count) {
        return i18n_text(STR_SETTINGS_ALL);
    }
    char included[NUMBER_CAPACITY];
    char total[NUMBER_CAPACITY];
    number_text(included, shown);
    number_text(total, (uint32_t)folders->count);
    const char *arguments[] = {included, total};
    StringBuilder builder = string_builder_make(folders->summary, sizeof folders->summary);
    i18n_format(&builder, STR_SETTINGS_FOLDERS_SHOWN, arguments, 2);
    return folders->summary;
}
