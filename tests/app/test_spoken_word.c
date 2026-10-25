#include <string.h>

#include "browse_screen.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "library/library_browse.h"
#include "player.h"
#include "spoken_library.h"
#include "spoken_store.h"

enum {
    SEED = 11,
    MIDWAY_MS = 123000,
    NEAR_END_MS = SPOKEN_EPISODE_MS - 5000
};

static SpokenLibrary spoken;
static Library *library;
static Player player;
static SpokenStore store;
static BrowsePool pool;
static Screen now_playing = {.title = "Now Playing"};

static bool path_is(const char *actual, const char *expected)
{
    return actual != NULL && actual[0] == '/' && strcmp(expected, actual + 1) == 0;
}

static void start(void)
{
    fake_hal_reset();
    player_init(&player, library);
    spoken_store_load(&store, library);
    player_set_resume(&player, spoken_store_resume(&store));
}

static BrowseQuery folder_of(const char *path, uint32_t *position)
{
    uint16_t track = spoken_library_track(library, path);
    LibraryTrack record;
    uint16_t chapter = 0;
    BrowseQuery query = browse_root_query(BROWSE_FOLDER);
    library_read_track(library, track, &record);
    spoken_chapter_of(library, track, &chapter);
    browse_child(query, (BrowseItem){.kind = BROWSE_ITEM_FOLDER, .index = record.folder}, &query);
    *position = chapter;
    return query;
}

static void play(const char *path, bool in_order)
{
    uint32_t position = 0;
    BrowseQuery source = folder_of(path, &position);
    PlayerListStart list = {
        .source = source, .position = position, .seed = SEED, .in_order = in_order};
    CHECK(player_play_from(&player, list));
}

static void follow(uint32_t now_ms)
{
    spoken_store_follow(&store, player_status(&player), now_ms);
}

static void books_play_in_order_even_with_shuffle_on(void)
{
    start();
    player_set_shuffle(&player, SHUFFLE_SONGS, SEED);
    play(SPOKEN_CHAPTER_ONE_PATH, true);
    CHECK(player_status(&player).spoken);
    CHECK(path_is(fake_hal_audio_queued_path(), SPOKEN_CHAPTER_TWO_PATH));
    player_set_shuffle(&player, SHUFFLE_SONGS, SEED + 1);
    CHECK(path_is(fake_hal_audio_queued_path(), SPOKEN_CHAPTER_TWO_PATH));
}

static void holding_volume_jumps_within_a_chapter(void)
{
    start();
    play(SPOKEN_CHAPTER_ONE_PATH, true);
    fake_hal_audio_set_position(100000);
    player_skip(&player, 1);
    CHECK_EQUAL(100000 + SPOKEN_JUMP_FORWARD_MS, fake_hal_audio_last_seek());
    fake_hal_audio_set_position(100000);
    player_skip(&player, -1);
    CHECK_EQUAL(100000 - SPOKEN_JUMP_BACK_MS, fake_hal_audio_last_seek());
    fake_hal_audio_set_position(5000);
    player_skip(&player, -1);
    CHECK_EQUAL(0, fake_hal_audio_last_seek());
}

static void jumping_past_the_end_starts_the_next_chapter(void)
{
    start();
    play(SPOKEN_CHAPTER_ONE_PATH, true);
    fake_hal_audio_set_position(SPOKEN_CHAPTER_MS - 10000);
    player_skip(&player, 1);
    CHECK(path_is(fake_hal_audio_playing_path(), SPOKEN_CHAPTER_TWO_PATH));
}

static void music_still_skips_tracks(void)
{
    start();
    play(SPOKEN_SONG_PATH, false);
    CHECK(!player_status(&player).spoken);
    fake_hal_audio_set_position(100000);
    player_skip(&player, -1);
    CHECK_EQUAL(0, fake_hal_audio_last_seek());
}

static void a_chapter_resumes_where_it_stopped(void)
{
    start();
    play(SPOKEN_CHAPTER_TWO_PATH, true);
    follow(0);
    fake_hal_audio_set_position(MIDWAY_MS);
    follow(1000);
    play(SPOKEN_SONG_PATH, false);
    follow(2000);
    play(SPOKEN_CHAPTER_TWO_PATH, true);
    CHECK_EQUAL(MIDWAY_MS, fake_hal_audio_play_start());
}

static void the_bookmark_is_saved_to_the_card(void)
{
    start();
    play(SPOKEN_CHAPTER_TWO_PATH, true);
    follow(0);
    fake_hal_audio_set_position(MIDWAY_MS);
    player_toggle_pause(&player);
    follow(1000);
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(SPOKEN_STORE_PATH, &size);
    Bookmarks saved;
    Bookmark found;
    CHECK(bytes != NULL && bookmarks_decode(bytes, size, &saved));
    CHECK(bookmarks_find(&saved, spoken_bookmark_key(SPOKEN_CHAPTER_TWO_PATH), &found));
    CHECK_EQUAL(MIDWAY_MS, found.position_ms);
    CHECK_EQUAL(1, found.chapter);
}

static void a_finished_episode_is_marked_played(void)
{
    start();
    play(SPOKEN_EPISODE_A_PATH, true);
    follow(0);
    fake_hal_audio_set_position(NEAR_END_MS);
    follow(1000);
    fake_hal_audio_finish_track();
    player_update(&player, 2000);
    follow(2000);
    Bookmark found;
    CHECK(spoken_store_track(&store, spoken_library_track(library, SPOKEN_EPISODE_A_PATH), &found));
    CHECK(bookmark_finished(found));
}

static void a_played_episode_starts_from_the_top(void)
{
    start();
    play(SPOKEN_EPISODE_A_PATH, true);
    follow(0);
    fake_hal_audio_set_position(NEAR_END_MS);
    follow(1000);
    play(SPOKEN_SONG_PATH, false);
    follow(2000);
    play(SPOKEN_EPISODE_A_PATH, true);
    CHECK_EQUAL(0, fake_hal_audio_play_start());
}

static BrowseScreen *shelf(SpokenKind kind)
{
    browse_pool_init(&pool, (BrowsePoolConfig){.library = library,
                                               .theme = &DEFAULT_THEME,
                                               .player = &player,
                                               .now_playing = &now_playing,
                                               .spoken = &store});
    Screen *screen = browse_pool_open_shelf(&pool, 0, kind);
    screen->enter(screen->context, 0);
    return screen->context;
}

static const char *detail_of(BrowseScreen *browse, int index)
{
    const char *detail = browse->model.detail(browse->model.context, index);
    return detail != NULL ? detail : "";
}

static ScreenAction press(BrowseScreen *browse, KeyGesture gesture)
{
    KeyEvent event = {.key = KEY_PLAY, .gesture = gesture};
    return browse->screen.handle_key(browse, event, 0);
}

static uint32_t music_count(BrowseKind kind)
{
    return browse_count(library, (BrowseQuery){.kind = kind});
}

static void the_music_lists_leave_out_spoken_word(void)
{
    start();
    CHECK_EQUAL(7, library_count(library, LIBRARY_TRACKS));
    CHECK_EQUAL(1, library_music_count(library, LIBRARY_TRACKS));
    CHECK_EQUAL(1, music_count(BROWSE_ALBUMS));
    CHECK_EQUAL(1, music_count(BROWSE_ALL_SONGS));
    CHECK_EQUAL(1, music_count(BROWSE_ARTISTS));
    CHECK_EQUAL(1, music_count(BROWSE_ALBUM_ARTISTS));
    CHECK_EQUAL(1, music_count(BROWSE_GENRES));
    CHECK_EQUAL(spoken_library_track(library, SPOKEN_SONG_PATH), 0);
}

static void the_audiobook_shelf_lists_books(void)
{
    start();
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    CHECK_EQUAL(2, books->count);
    CHECK(strcmp("Audiobooks", books->title) == 0);
    CHECK(strcmp("3 chapters", detail_of(books, 0)) == 0);
}

static void a_started_book_shows_its_chapter(void)
{
    start();
    play(SPOKEN_CHAPTER_TWO_PATH, true);
    follow(0);
    fake_hal_audio_set_position(MIDWAY_MS);
    follow(1000);
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    CHECK(strcmp("Chapter 2 of 3", detail_of(books, 0)) == 0);
}

static void selecting_a_book_resumes_it(void)
{
    start();
    play(SPOKEN_CHAPTER_TWO_PATH, true);
    follow(0);
    fake_hal_audio_set_position(MIDWAY_MS);
    follow(1000);
    play(SPOKEN_SONG_PATH, false);
    follow(2000);
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    ScreenAction action = press(books, KEY_GESTURE_PRESS);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    CHECK(action.target == &now_playing);
    CHECK(path_is(fake_hal_audio_playing_path(), SPOKEN_CHAPTER_TWO_PATH));
    CHECK_EQUAL(MIDWAY_MS, fake_hal_audio_play_start());
}

static void selecting_a_paused_book_carries_on_playing_it(void)
{
    start();
    play(SPOKEN_CHAPTER_TWO_PATH, true);
    player_toggle_pause(&player);
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    ScreenAction action = press(books, KEY_GESTURE_PRESS);
    CHECK(action.target == &now_playing);
    CHECK_EQUAL(PLAYER_PLAYING, player_status(&player).state);
    CHECK_EQUAL(1, fake_hal_audio_play_count());
}

static void holding_play_on_a_book_opens_its_chapters(void)
{
    start();
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    ScreenAction action = press(books, KEY_GESTURE_LONG_PRESS);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    BrowseScreen *chapters = action.target->context;
    CHECK_EQUAL(3, chapters->count);
    CHECK_EQUAL(SPOKEN_AUDIOBOOK, chapters->shelf);
}

static void a_played_episode_says_so_in_its_show(void)
{
    start();
    play(SPOKEN_EPISODE_A_PATH, true);
    follow(0);
    fake_hal_audio_set_position(NEAR_END_MS);
    follow(1000);
    play(SPOKEN_SONG_PATH, false);
    follow(2000);
    BrowseScreen *shows = shelf(SPOKEN_PODCAST);
    CHECK(strcmp("2 episodes", detail_of(shows, 0)) == 0);
    ScreenAction action = press(shows, KEY_GESTURE_PRESS);
    BrowseScreen *episodes = action.target->context;
    episodes->screen.enter(episodes, 0);
    CHECK(strcmp("Played", detail_of(episodes, 0)) == 0);
}

int main(void)
{
    library = spoken_library_open(&spoken);
    static const TestCase cases[] = {
        TEST_CASE(books_play_in_order_even_with_shuffle_on),
        TEST_CASE(holding_volume_jumps_within_a_chapter),
        TEST_CASE(jumping_past_the_end_starts_the_next_chapter),
        TEST_CASE(music_still_skips_tracks),
        TEST_CASE(a_chapter_resumes_where_it_stopped),
        TEST_CASE(the_bookmark_is_saved_to_the_card),
        TEST_CASE(a_finished_episode_is_marked_played),
        TEST_CASE(a_played_episode_starts_from_the_top),
        TEST_CASE(the_music_lists_leave_out_spoken_word),
        TEST_CASE(the_audiobook_shelf_lists_books),
        TEST_CASE(a_started_book_shows_its_chapter),
        TEST_CASE(selecting_a_book_resumes_it),
        TEST_CASE(selecting_a_paused_book_carries_on_playing_it),
        TEST_CASE(holding_play_on_a_book_opens_its_chapters),
        TEST_CASE(a_played_episode_says_so_in_its_show),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    spoken_library_close(&spoken);
    return result;
}
