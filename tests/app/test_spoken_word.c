#include <string.h>

#include "browse_screen.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "library/library_browse.h"
#include "mp4_book.h"
#include "player.h"
#include "screens/now_playing_screen.h"
#include "settings/settings.h"
#include "spoken_library.h"
#include "spoken_store.h"
#include "ui/marquee.h"

enum {
    SEED = 11,
    MIDWAY_MS = 123000,
    NEAR_END_MS = SPOKEN_EPISODE_MS - 5000,
    STRIP_WIDTH = 320,
    STRIP_ROWS = 34,
    FACE_POSITION_MS = 61230
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
    LibraryFolder folder;
    uint16_t chapter = 0;
    BrowseQuery query = browse_root_query(BROWSE_FOLDER);
    library_read_track(library, track, &record);
    library_read_folder(library, record.folder, &folder);
    spoken_chapter_of(library, track, &chapter);
    browse_child(query, (BrowseItem){.kind = BROWSE_ITEM_FOLDER, .index = record.folder}, &query);
    *position = (uint32_t)folder.subfolder_count + chapter;
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

static uint8_t book_file[512];
static const Mp4BookChapter BOOK_CHAPTERS[] = {
    {0, "Part 1"}, {200000, "Part 2"}, {400000, "Part 3"}};
static const char BOOK_FILE_PATH[] = "/Audiobooks/Chaptered Book/Chaptered Book.m4b";

static void provide_book_file(void)
{
    uint32_t size = mp4_book_build(book_file, sizeof book_file, BOOK_CHAPTERS, 3);
    fake_hal_add_file(BOOK_FILE_PATH, book_file, size);
}

static void a_book_with_chapters_reports_the_current_one(void)
{
    start();
    provide_book_file();
    play(SPOKEN_CHAPTERED_BOOK_PATH, true);
    PlayerStatus status = player_status(&player);
    CHECK_EQUAL(3, status.chapters);
    CHECK_EQUAL(0, status.chapter);
    fake_hal_audio_set_position(250000);
    CHECK_EQUAL(1, player_status(&player).chapter);
    fake_hal_audio_set_position(599000);
    CHECK_EQUAL(2, player_status(&player).chapter);
}

static void holding_volume_still_jumps_in_time_in_a_book_with_chapters(void)
{
    start();
    provide_book_file();
    play(SPOKEN_CHAPTERED_BOOK_PATH, true);
    fake_hal_audio_set_position(100000);
    player_skip(&player, 1);
    CHECK_EQUAL(100000 + SPOKEN_JUMP_FORWARD_MS, fake_hal_audio_last_seek());
    fake_hal_audio_set_position(250000);
    player_skip(&player, -1);
    CHECK_EQUAL(250000 - SPOKEN_JUMP_BACK_MS, fake_hal_audio_last_seek());
    fake_hal_audio_set_position(5000);
    player_skip(&player, -1);
    CHECK_EQUAL(0, fake_hal_audio_last_seek());
}

static void a_book_whose_chapters_cannot_be_read_keeps_the_time_jumps(void)
{
    start();
    play(SPOKEN_CHAPTERED_BOOK_PATH, true);
    CHECK_EQUAL(0, player_status(&player).chapters);
    fake_hal_audio_set_position(100000);
    player_skip(&player, 1);
    CHECK_EQUAL(100000 + SPOKEN_JUMP_FORWARD_MS, fake_hal_audio_last_seek());
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
    CHECK_EQUAL(9, library_count(library, LIBRARY_TRACKS));
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
    CHECK_EQUAL(4, books->count);
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

static void a_single_files_chapters_are_counted_on_the_shelf(void)
{
    start();
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    CHECK(strcmp("3 chapters", detail_of(books, 1)) == 0);
}

static void a_started_chaptered_book_shows_its_chapter_on_the_shelf(void)
{
    start();
    provide_book_file();
    play(SPOKEN_CHAPTERED_BOOK_PATH, true);
    follow(0);
    fake_hal_audio_set_position(250000);
    follow(SPOKEN_PUT_INTERVAL_MS + 1000);
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    CHECK(strcmp("Chapter 2 of 3", detail_of(books, 1)) == 0);
}

static uint8_t loose_file[512];
static const Mp4BookChapter LOOSE_CHAPTERS[] = {
    {0, "One"}, {100000, "Two"}, {300000, "Three"}, {500000, "Four"}};
static const char LOOSE_FILE_PATH[] = "/Audiobooks/Loose Book.m4b";

static int loose_row(BrowseScreen *books)
{
    for (int index = 0; index < (int)books->count; index++) {
        if (strstr(detail_of(books, index), "4") != NULL) {
            return index;
        }
    }
    return -1;
}

static void a_loose_chaptered_file_counts_its_chapters_on_the_shelf(void)
{
    start();
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    int row = loose_row(books);
    CHECK(row >= 0);
    CHECK(strcmp("4 chapters", detail_of(books, row)) == 0);
}

static void a_started_loose_file_shows_its_chapter_on_the_shelf(void)
{
    start();
    uint32_t size = mp4_book_build(loose_file, sizeof loose_file, LOOSE_CHAPTERS, 4);
    fake_hal_add_file(LOOSE_FILE_PATH, loose_file, size);
    play(SPOKEN_LOOSE_BOOK_PATH, true);
    follow(0);
    fake_hal_audio_set_position(350000);
    follow(SPOKEN_PUT_INTERVAL_MS + 1000);
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    int row = loose_row(books);
    CHECK(row >= 0);
    CHECK(strcmp("Chapter 3 of 4", detail_of(books, row)) == 0);
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

static void spoken_word_lists_never_offer_shuffle_all(void)
{
    start();
    BrowseScreen *books = shelf(SPOKEN_AUDIOBOOK);
    CHECK_EQUAL(0, books->extra);
    ScreenAction action = press(books, KEY_GESTURE_LONG_PRESS);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    BrowseScreen *chapters = action.target->context;
    CHECK_EQUAL(SPOKEN_AUDIOBOOK, chapters->shelf);
    CHECK_EQUAL(0, chapters->extra);
    BrowseScreen *shows = shelf(SPOKEN_PODCAST);
    CHECK_EQUAL(0, shows->extra);
    action = press(shows, KEY_GESTURE_PRESS);
    BrowseScreen *episodes = action.target->context;
    CHECK_EQUAL(SPOKEN_PODCAST, episodes->shelf);
    CHECK_EQUAL(0, episodes->extra);
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

static Color face_strip[STRIP_WIDTH * STRIP_ROWS];
static NowPlayingScreen face_screen;
static uint8_t face_options[SETTINGS_OPTION_COUNT];

static void show_a_face_at(uint32_t position_ms)
{
    memset(face_options, 0, sizeof face_options);
    face_options[OPTION_NOW_PLAYING_FACE] = NP_FACE_CASSETTE;
    NowPlayingConfig config = {
        .player = &player, .library = library, .theme = &DEFAULT_THEME, .options = face_options};
    Screen *screen = now_playing_screen_init(&face_screen, config);
    fake_hal_audio_set_position(position_ms);
    screen->enter(screen->context, 0);
}

static uint32_t frame_wait_after_drawing_the_face(void)
{
    Canvas canvas = canvas_make(face_strip, (Rect){0, 0, STRIP_WIDTH, STRIP_ROWS});
    marquee_frame_start();
    face_screen.screen.draw(face_screen.screen.context, &canvas);
    return marquee_frame_wait_ms();
}

static uint32_t ticks_to_the_next_hundred(void)
{
    return 100 - player_status(&player).position_ms % 100;
}

static void a_playing_face_asks_for_the_next_tick_of_the_position(void)
{
    start();
    play(SPOKEN_CHAPTER_ONE_PATH, true);
    show_a_face_at(FACE_POSITION_MS);
    CHECK(ticks_to_the_next_hundred() <= 100);
    CHECK_EQUAL(ticks_to_the_next_hundred(), frame_wait_after_drawing_the_face());
    fake_hal_audio_set_position(FACE_POSITION_MS + 40);
    face_screen.screen.update(face_screen.screen.context, 40);
    CHECK_EQUAL(ticks_to_the_next_hundred(), frame_wait_after_drawing_the_face());
}

static void a_paused_face_asks_for_no_frame(void)
{
    start();
    play(SPOKEN_CHAPTER_ONE_PATH, true);
    show_a_face_at(FACE_POSITION_MS);
    CHECK(frame_wait_after_drawing_the_face() > 0);
    player_toggle_pause(&player);
    face_screen.screen.update(face_screen.screen.context, 1);
    CHECK_EQUAL(PLAYER_PAUSED, player_status(&player).state);
    CHECK_EQUAL(0u, frame_wait_after_drawing_the_face());
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
        TEST_CASE(a_book_with_chapters_reports_the_current_one),
        TEST_CASE(holding_volume_still_jumps_in_time_in_a_book_with_chapters),
        TEST_CASE(a_book_whose_chapters_cannot_be_read_keeps_the_time_jumps),
        TEST_CASE(a_single_files_chapters_are_counted_on_the_shelf),
        TEST_CASE(a_started_chaptered_book_shows_its_chapter_on_the_shelf),
        TEST_CASE(a_loose_chaptered_file_counts_its_chapters_on_the_shelf),
        TEST_CASE(a_started_loose_file_shows_its_chapter_on_the_shelf),
        TEST_CASE(a_started_book_shows_its_chapter),
        TEST_CASE(selecting_a_book_resumes_it),
        TEST_CASE(selecting_a_paused_book_carries_on_playing_it),
        TEST_CASE(holding_play_on_a_book_opens_its_chapters),
        TEST_CASE(spoken_word_lists_never_offer_shuffle_all),
        TEST_CASE(a_played_episode_says_so_in_its_show),
        TEST_CASE(a_playing_face_asks_for_the_next_tick_of_the_position),
        TEST_CASE(a_paused_face_asks_for_no_frame),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    spoken_library_close(&spoken);
    return result;
}
