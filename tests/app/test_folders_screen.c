#include <stdio.h>
#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "folders_store.h"
#include "screens/folders_screen.h"

enum {
    START_MS = 1000,
    MANY_FOLDERS = 40,
    LONG_FOLDERS = 12,
    LONG_NAME = 70
};

static Toast toast;
static bool rescan_requested;
static FoldersScreen folders;

static void add_folder(const char *root, const char *name)
{
    fake_hal_add_directory_entry(root, name, true, false);
}

static void start_empty(void)
{
    fake_hal_reset();
    toast = (Toast){0};
    rescan_requested = false;
}

static void start_card(void)
{
    start_empty();
    add_folder("/", "Rock");
    add_folder("/", "Jazz");
    add_folder("/", ".echo");
    add_folder("/", "Audiobooks");
    add_folder("/", "podcast");
    add_folder("/", "System Volume Information");
    fake_hal_add_directory_entry("/", "$RECYCLE.BIN", true, true);
    fake_hal_add_directory_entry("/", "Root Song.flac", false, false);
    add_folder("/", "Classical");
}

static Screen *init_screen(void)
{
    return folders_screen_init(&folders, (FoldersConfig){.toast = &toast,
                                                         .rescan_requested = &rescan_requested,
                                                         .theme = &DEFAULT_THEME});
}

static Screen *open_screen(void)
{
    Screen *screen = init_screen();
    ScreenAction open = folders_screen_open(&folders);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, open.type);
    CHECK(open.target == screen);
    screen->enter(screen->context, START_MS);
    return screen;
}

static ScreenAction press(Key key, KeyGesture gesture)
{
    return folders.screen.handle_key(folders.screen.context,
                                     (KeyEvent){.key = key, .gesture = gesture}, START_MS);
}

static ScreenAction tap(Key key)
{
    return press(key, KEY_GESTURE_PRESS);
}

static int rows(void)
{
    return folders.model.count(folders.model.context);
}

static const char *label(int index)
{
    return folders.model.label(folders.model.context, index);
}

static bool ticked(int index)
{
    const char *mark = folders.model.detail(folders.model.context, index);
    return mark != NULL && mark[0] != '\0';
}

static uint32_t key_of(const char *folder, const char *name)
{
    uint32_t key = 0;
    CHECK(music_folders_key(folder, name, &key));
    return key;
}

static MusicFolders saved(void)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(FOLDERS_STORE_PATH, &size);
    MusicFolders stored = {.count = 0};
    CHECK(bytes != NULL);
    CHECK(music_folders_decode(bytes, size, &stored));
    return stored;
}

static void store(const MusicFolders *stored)
{
    CHECK(folders_store_save(stored));
}

static bool summary_is(const char *expected)
{
    return strcmp(expected, folders_screen_summary(&folders)) == 0;
}

static void the_card_folders_are_listed_by_name_and_all_ticked(void)
{
    start_card();
    open_screen();
    CHECK_EQUAL(3, rows());
    CHECK_EQUAL(0, strcmp("Classical", label(0)));
    CHECK_EQUAL(0, strcmp("Jazz", label(1)));
    CHECK_EQUAL(0, strcmp("Rock", label(2)));
    for (int index = 0; index < 3; index++) {
        CHECK(ticked(index));
    }
    CHECK(summary_is("All"));
}

static void the_summary_lists_the_card_once_before_the_screen_opens(void)
{
    start_card();
    init_screen();
    int opens = fake_hal_dir_open_count();
    CHECK(summary_is("All"));
    CHECK_EQUAL(opens + 1, fake_hal_dir_open_count());
    CHECK(summary_is("All"));
    CHECK_EQUAL(opens + 1, fake_hal_dir_open_count());
}

static void internal_storage_folders_follow_the_cards_with_its_name(void)
{
    start_card();
    add_folder("/", ":internal");
    add_folder("/:internal", "Podcasts");
    add_folder("/:internal", "Albums");
    add_folder("/:internal", "System Volume Information");
    open_screen();
    CHECK_EQUAL(5, rows());
    CHECK_EQUAL(0, strcmp("Rock", label(2)));
    CHECK_EQUAL(0, strcmp("Internal storage/Albums", label(3)));
    CHECK_EQUAL(0, strcmp("Internal storage/Podcasts", label(4)));
    tap(KEY_VOLUME_UP);
    tap(KEY_VOLUME_UP);
    tap(KEY_VOLUME_UP);
    tap(KEY_PLAY);
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(KEY_BACK).type);
    MusicFolders stored = saved();
    CHECK_EQUAL(1u, stored.count);
    CHECK_EQUAL(key_of("/:internal", "Albums"), stored.keys[0]);
    CHECK(key_of("/", "Albums") != stored.keys[0]);
}

static void unticking_a_folder_saves_it_and_asks_for_a_rescan(void)
{
    start_card();
    open_screen();
    tap(KEY_VOLUME_UP);
    tap(KEY_PLAY);
    CHECK(!ticked(1));
    CHECK(!rescan_requested);
    CHECK(summary_is("2 of 3"));
    CHECK_EQUAL(SCREEN_ACTION_POP, tap(KEY_BACK).type);
    CHECK(rescan_requested);
    MusicFolders stored = saved();
    CHECK_EQUAL(1u, stored.count);
    CHECK_EQUAL(key_of("/", "Jazz"), stored.keys[0]);
    open_screen();
    CHECK(ticked(0));
    CHECK(!ticked(1));
    CHECK(summary_is("2 of 3"));
}

static void leaving_without_a_change_saves_nothing(void)
{
    start_card();
    open_screen();
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    int writes = fake_hal_write_count();
    CHECK_EQUAL(SCREEN_ACTION_POP_TO_ROOT, press(KEY_BACK, KEY_GESTURE_LONG_PRESS).type);
    CHECK_EQUAL(writes, fake_hal_write_count());
    CHECK(!rescan_requested);
}

static void ticking_a_folder_again_brings_it_back(void)
{
    start_card();
    MusicFolders stored = {.count = 1, .keys = {key_of("/", "Rock")}};
    store(&stored);
    open_screen();
    CHECK(!ticked(2));
    CHECK(summary_is("2 of 3"));
    tap(KEY_VOLUME_UP);
    tap(KEY_VOLUME_UP);
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(rescan_requested);
    CHECK_EQUAL(0u, saved().count);
}

static void folders_not_on_the_list_keep_their_choice(void)
{
    start_card();
    MusicFolders stored = {.count = 2, .keys = {key_of("/:internal", "Albums"), 7u}};
    store(&stored);
    open_screen();
    CHECK(summary_is("All"));
    tap(KEY_PLAY);
    tap(KEY_BACK);
    MusicFolders kept = saved();
    CHECK_EQUAL(3u, kept.count);
    CHECK_EQUAL(key_of("/", "Classical"), kept.keys[0]);
    CHECK_EQUAL(key_of("/:internal", "Albums"), kept.keys[1]);
    CHECK_EQUAL(7u, kept.keys[2]);
}

static void at_most_thirty_two_folders_are_listed(void)
{
    static char names[MANY_FOLDERS][8];
    start_empty();
    for (int index = 0; index < MANY_FOLDERS; index++) {
        snprintf(names[index], sizeof names[index], "F%02d", MANY_FOLDERS - 1 - index);
        add_folder("/", names[index]);
    }
    open_screen();
    CHECK_EQUAL(FOLDERS_SCREEN_ROWS, rows());
    CHECK_EQUAL(0, strcmp("F08", label(0)));
    CHECK_EQUAL(0, strcmp("F39", label(FOLDERS_SCREEN_ROWS - 1)));
}

static void long_names_share_the_name_space(void)
{
    static char names[LONG_FOLDERS][LONG_NAME + 1];
    start_empty();
    for (int index = 0; index < LONG_FOLDERS; index++) {
        memset(names[index], 'a' + index, LONG_NAME);
        names[index][LONG_NAME] = '\0';
        add_folder("/", names[index]);
    }
    open_screen();
    CHECK(rows() >= FOLDER_NAMES_CAPACITY / (LONG_NAME + 1));
    CHECK(rows() < LONG_FOLDERS);
    CHECK_EQUAL((size_t)LONG_NAME, strlen(label(0)));
    for (int index = 0; index < rows(); index++) {
        CHECK(label(index)[0] == 'a' + index);
    }
}

static void an_empty_card_says_so(void)
{
    start_empty();
    fake_hal_add_directory_entry("/", "Root Song.flac", false, false);
    open_screen();
    CHECK_EQUAL(1, rows());
    CHECK_EQUAL(0, strcmp("No folders on the card", label(0)));
    CHECK(!ticked(0));
    CHECK_EQUAL(SCREEN_ACTION_NONE, tap(KEY_PLAY).type);
    tap(KEY_BACK);
    CHECK(!rescan_requested);
    CHECK(summary_is("All"));
}

static void a_failed_save_says_so_and_keeps_the_library(void)
{
    start_card();
    open_screen();
    tap(KEY_PLAY);
    fake_hal_set_card_missing(true);
    tap(KEY_BACK);
    CHECK(!rescan_requested);
    CHECK(toast.text != NULL);
    CHECK_EQUAL(0, strcmp("Could not save settings", toast.text));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_card_folders_are_listed_by_name_and_all_ticked),
        TEST_CASE(the_summary_lists_the_card_once_before_the_screen_opens),
        TEST_CASE(internal_storage_folders_follow_the_cards_with_its_name),
        TEST_CASE(unticking_a_folder_saves_it_and_asks_for_a_rescan),
        TEST_CASE(leaving_without_a_change_saves_nothing),
        TEST_CASE(ticking_a_folder_again_brings_it_back),
        TEST_CASE(folders_not_on_the_list_keep_their_choice),
        TEST_CASE(at_most_thirty_two_folders_are_listed),
        TEST_CASE(long_names_share_the_name_space),
        TEST_CASE(an_empty_card_says_so),
        TEST_CASE(a_failed_save_says_so_and_keeps_the_library),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
