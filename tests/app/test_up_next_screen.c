#include <string.h>

#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "mp4_book.h"
#include "player.h"
#include "screens/up_next_screen.h"
#include "spoken_library.h"

enum {
    BACK_IN_BLACK = 0,
    HELP = 2,
    ARMY_OF_ME = 3,
    SEED = 7
};

static const char ARMY_OF_ME_PATH[] = "/Various Artists/The Best of Something/02 - Army of Me.flac";
static const char SHOOT_TO_THRILL_PATH[] = "/AC DC/Back In Black/1-02 - Shoot to Thrill.flac";

static FixtureLibrary fixture;
static Player player;
static UpNextScreen up_next;
static Screen *screen;

static BrowseQuery back_in_black(void)
{
    return (BrowseQuery){.kind = BROWSE_ALBUM_TRACKS,
                         .group = LIBRARY_ARTISTS,
                         .subject = BACK_IN_BLACK,
                         .artist = LIBRARY_NONE};
}

static void play_album_from(uint32_t position)
{
    fake_hal_reset();
    player_init(&player, fixture_library_open(&fixture));
    CHECK(player_play_list(&player, back_in_black(), position, SEED));
}

static void open_up_next(void)
{
    screen = up_next_screen_init(
        &up_next,
        (UpNextConfig){.library = &fixture.library, .theme = &DEFAULT_THEME, .player = &player});
    screen->enter(screen->context, 100);
}

static int row_count(void)
{
    return up_next.model.count(up_next.model.context);
}

static bool row_is(int index, const char *label, const char *detail)
{
    const char *shown_label = up_next.model.label(up_next.model.context, index);
    const char *shown_detail = up_next.model.detail(up_next.model.context, index);
    return strcmp(label, shown_label) == 0 && shown_detail != NULL &&
           strcmp(detail, shown_detail) == 0;
}

static ScreenAction press(Key key)
{
    return screen->handle_key(screen->context, (KeyEvent){.key = key, .gesture = KEY_GESTURE_PRESS},
                              200);
}

static ScreenAction choose_row(int index)
{
    for (int step = 0; step < index; step++) {
        press(KEY_VOLUME_UP);
    }
    return press(KEY_PLAY);
}

static bool playing_path_is(const char *expected)
{
    const char *path = fake_hal_audio_playing_path();
    return path != NULL && strcmp(expected, path) == 0;
}

static SpokenLibrary spoken;
static uint8_t book_file[512];
static const Mp4BookChapter CHAPTERS[] = {{0, "Part 1"}, {200000, "Part 2"}, {400000, "Part 3"}};

static void play_chaptered_book(void)
{
    fake_hal_reset();
    Library *library = spoken_library_open(&spoken);
    player_init(&player, library);
    uint32_t size = mp4_book_build(book_file, sizeof book_file, CHAPTERS, 3);
    fake_hal_add_file("/Audiobooks/Chaptered Book/Chaptered Book.m4b", book_file, size);
    LibraryTrack record;
    BrowseQuery query = browse_root_query(BROWSE_FOLDER);
    library_read_track(library, spoken_library_track(library, SPOKEN_CHAPTERED_BOOK_PATH), &record);
    browse_child(query, (BrowseItem){.kind = BROWSE_ITEM_FOLDER, .index = record.folder}, &query);
    CHECK(player_play_from(&player, (PlayerListStart){.source = query, .in_order = true}));
}

static void open_chapters(void)
{
    screen = up_next_screen_init(
        &up_next,
        (UpNextConfig){.library = &spoken.library, .theme = &DEFAULT_THEME, .player = &player});
    screen->enter(screen->context, 100);
}

static void a_book_with_chapters_lists_them_with_their_start_times(void)
{
    play_chaptered_book();
    open_chapters();
    CHECK_EQUAL(3, row_count());
    CHECK(row_is(0, "Chapter 1 of 3", "0:00"));
    CHECK(row_is(1, "Chapter 2 of 3", "3:20"));
    CHECK(row_is(2, "Chapter 3 of 3", "6:40"));
}

static void the_chapter_list_starts_on_the_chapter_being_played(void)
{
    play_chaptered_book();
    fake_hal_audio_set_position(250000);
    open_chapters();
    CHECK_EQUAL(1, up_next.list.selected);
}

static void choosing_a_chapter_seeks_to_it_and_leaves(void)
{
    play_chaptered_book();
    open_chapters();
    ScreenAction action = choose_row(2);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(400000, fake_hal_audio_last_seek());
}

static void the_chapter_list_follows_a_track_change_while_it_is_open(void)
{
    play_chaptered_book();
    open_chapters();
    player.chapters.count = 0;
    screen->update(screen->context, 150);
    CHECK(!up_next.chapters);
    const char *detail = up_next.model.detail(up_next.model.context, 0);
    CHECK(detail != NULL && strcmp("Playing", detail) == 0);
}

static void a_stale_chapter_row_never_seeks(void)
{
    play_chaptered_book();
    open_chapters();
    uint32_t before = fake_hal_audio_last_seek();
    player.chapters.count = 1;
    ScreenAction action = choose_row(2);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(before, fake_hal_audio_last_seek());
}

static void the_playing_song_comes_first_then_the_list(void)
{
    play_album_from(0);
    open_up_next();
    CHECK_EQUAL(2, row_count());
    CHECK(row_is(0, "Hells Bells", "Playing"));
    CHECK(row_is(1, "Shoot to Thrill", "AC/DC"));
}

static void a_paused_song_says_so(void)
{
    play_album_from(0);
    player_toggle_pause(&player);
    open_up_next();
    CHECK(row_is(0, "Hells Bells", "Paused"));
}

static void added_songs_come_before_the_list(void)
{
    play_album_from(0);
    player_add_to_up_next(&player, HELP);
    open_up_next();
    CHECK_EQUAL(3, row_count());
    CHECK(row_is(1, "Help!", "The Beatles"));
    CHECK(row_is(2, "Shoot to Thrill", "AC/DC"));
}

static void the_last_song_has_nothing_after_it(void)
{
    play_album_from(1);
    open_up_next();
    CHECK_EQUAL(1, row_count());
}

static void repeat_all_shows_the_list_wrapping_round(void)
{
    play_album_from(1);
    player_set_repeat(&player, REPEAT_ALL);
    open_up_next();
    CHECK_EQUAL(2, row_count());
    CHECK(row_is(1, "Hells Bells", "AC/DC"));
}

static void choosing_an_added_song_plays_it_and_returns(void)
{
    play_album_from(0);
    player_add_to_up_next(&player, HELP);
    player_add_to_up_next(&player, ARMY_OF_ME);
    open_up_next();
    ScreenAction action = choose_row(2);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK(playing_path_is(ARMY_OF_ME_PATH));
    CHECK_EQUAL(0, player.up_next.count);
}

static void choosing_a_list_song_skips_the_added_ones(void)
{
    play_album_from(0);
    player_add_to_up_next(&player, HELP);
    open_up_next();
    ScreenAction action = choose_row(2);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(0, player.up_next.count);
}

static void choosing_the_playing_song_just_returns(void)
{
    play_album_from(0);
    open_up_next();
    int plays = fake_hal_audio_play_count();
    ScreenAction action = choose_row(0);
    CHECK_EQUAL(SCREEN_ACTION_POP, action.type);
    CHECK_EQUAL(plays, fake_hal_audio_play_count());
}

static void back_returns_to_now_playing(void)
{
    play_album_from(0);
    open_up_next();
    CHECK_EQUAL(SCREEN_ACTION_POP, press(KEY_BACK).type);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_book_with_chapters_lists_them_with_their_start_times),
        TEST_CASE(the_chapter_list_starts_on_the_chapter_being_played),
        TEST_CASE(choosing_a_chapter_seeks_to_it_and_leaves),
        TEST_CASE(the_chapter_list_follows_a_track_change_while_it_is_open),
        TEST_CASE(a_stale_chapter_row_never_seeks),
        TEST_CASE(the_playing_song_comes_first_then_the_list),
        TEST_CASE(a_paused_song_says_so),
        TEST_CASE(added_songs_come_before_the_list),
        TEST_CASE(the_last_song_has_nothing_after_it),
        TEST_CASE(repeat_all_shows_the_list_wrapping_round),
        TEST_CASE(choosing_an_added_song_plays_it_and_returns),
        TEST_CASE(choosing_a_list_song_skips_the_added_ones),
        TEST_CASE(choosing_the_playing_song_just_returns),
        TEST_CASE(back_returns_to_now_playing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
