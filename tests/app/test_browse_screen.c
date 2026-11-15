#include <string.h>

#include "browse_screen.h"
#include "check.h"
#include "core/i18n.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"

static FixtureLibrary fixture;
static BrowsePool pool;
static FavoritesStore favorites;
static Player player;

static BrowseScreen *opened(BrowseQuery query)
{
    fake_hal_reset();
    Library *library = fixture_library_open(&fixture);
    favorites = (FavoritesStore){.library = library};
    browse_pool_init(
        &pool,
        (BrowsePoolConfig){.library = library, .theme = &DEFAULT_THEME, .favorites = &favorites});
    Screen *screen = browse_pool_open(&pool, 0, query);
    screen->enter(screen->context, 0);
    return screen->context;
}

static int section_at(const BrowseScreen *browse, int index)
{
    return browse->model.section(browse->model.context, index);
}

static uint16_t folder_id(const Library *library, bool with_tracks)
{
    for (uint32_t index = 1; index < library_count(library, LIBRARY_FOLDERS); index++) {
        LibraryFolder record;
        if (!library_read_folder(library, index, &record)) {
            return LIBRARY_NONE;
        }
        bool matches = with_tracks ? record.track_count > 0
                                   : record.track_count == 0 && record.subfolder_count > 0;
        if (matches) {
            return (uint16_t)index;
        }
    }
    return LIBRARY_NONE;
}

static BrowseScreen *opened_folder(bool with_tracks)
{
    fake_hal_reset();
    Library *library = fixture_library_open(&fixture);
    BrowseQuery query = browse_root_query(BROWSE_FOLDER);
    BrowseItem item = {.kind = BROWSE_ITEM_FOLDER, .index = folder_id(library, with_tracks)};
    CHECK(browse_child(query, item, &query));
    player_init(&player, library);
    favorites = (FavoritesStore){.library = library};
    browse_pool_init(&pool, (BrowsePoolConfig){.library = library,
                                               .theme = &DEFAULT_THEME,
                                               .player = &player,
                                               .favorites = &favorites});
    Screen *screen = browse_pool_open(&pool, 0, query);
    screen->enter(screen->context, 0);
    return screen->context;
}

static void a_folder_with_tracks_starts_with_shuffle_all(void)
{
    BrowseScreen *browse = opened_folder(true);
    CHECK_EQUAL(1, browse->extra);
    CHECK_EQUAL(1, browse->list.selected);
    CHECK(strcmp(i18n_text(STR_MENU_SHUFFLE_ALL), browse->model.label(browse, 0)) == 0);
    CHECK_EQUAL((int)browse_count(pool.config.library, browse->query) + 1, (int)browse->count);
}

static void a_folder_of_folders_still_starts_with_shuffle_all(void)
{
    BrowseScreen *browse = opened_folder(false);
    CHECK_EQUAL(1, browse->extra);
    CHECK_EQUAL(1, browse->list.selected);
    CHECK(strcmp(i18n_text(STR_MENU_SHUFFLE_ALL), browse->model.label(browse, 0)) == 0);
    CHECK_EQUAL((int)browse_count(pool.config.library, browse->query) + 1, (int)browse->count);
}

static BrowseScreen *opened_root_folder(void)
{
    fake_hal_reset();
    Library *library = fixture_library_open(&fixture);
    player_init(&player, library);
    favorites = (FavoritesStore){.library = library};
    browse_pool_init(&pool, (BrowsePoolConfig){.library = library,
                                               .theme = &DEFAULT_THEME,
                                               .player = &player,
                                               .favorites = &favorites});
    Screen *screen = browse_pool_open(&pool, 0, browse_root_query(BROWSE_FOLDER));
    screen->enter(screen->context, 0);
    return screen->context;
}

static void the_root_folder_has_no_shuffle_all(void)
{
    BrowseScreen *browse = opened_root_folder();
    CHECK_EQUAL(0, browse->extra);
    CHECK_EQUAL(0, browse->list.selected);
    CHECK(strcmp(i18n_text(STR_MENU_SHUFFLE_ALL), browse->model.label(browse, 0)) != 0);
}

static void shuffle_all_plays_the_folders_tracks_shuffled(void)
{
    BrowseScreen *browse = opened_folder(true);
    uint16_t folder = browse->query.subject;
    list_view_select(&browse->list, 0, 0);
    ScreenAction action =
        browse->screen.handle_key(browse, (KeyEvent){KEY_PLAY, KEY_GESTURE_PRESS}, 0);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    PlayerStatus status = player_status(&player);
    CHECK(status.has_track);
    CHECK(status.state != PLAYER_IDLE);
    LibraryTrack anchor;
    CHECK(library_read_track(pool.config.library, status.track, &anchor));
    CHECK_EQUAL(folder, anchor.folder);
    LibraryFolder record;
    CHECK(library_read_folder(pool.config.library, folder, &record));
    CHECK_EQUAL(record.track_count, player.queue.count);
    CHECK_EQUAL(SHUFFLE_SONGS, player.queue.plan.shuffle);
}

static void shuffle_all_plays_tracks_of_subfolders_too(void)
{
    BrowseScreen *browse = opened_folder(false);
    uint16_t folder = browse->query.subject;
    list_view_select(&browse->list, 0, 0);
    ScreenAction action =
        browse->screen.handle_key(browse, (KeyEvent){KEY_PLAY, KEY_GESTURE_PRESS}, 0);
    CHECK_EQUAL(SCREEN_ACTION_PUSH, action.type);
    PlayerStatus status = player_status(&player);
    CHECK(status.has_track);
    LibraryTrack anchor;
    CHECK(library_read_track(pool.config.library, status.track, &anchor));
    CHECK(anchor.folder != folder);
    CHECK_EQUAL(BROWSE_FOLDER_TRACKS, player.queue.plan.source.kind);
    CHECK_EQUAL(3, player.queue.count);
    CHECK_EQUAL(SHUFFLE_SONGS, player.queue.plan.shuffle);
}

static void albums_are_lettered_by_their_sort_key(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    CHECK(albums->model.section != NULL);
    CHECK_EQUAL(LIST_SECTION_FIRST_LETTER + 'b' - 'a', section_at(albums, 0));
    CHECK_EQUAL(LIST_SECTION_FIRST_LETTER + 'b' - 'a', section_at(albums, 1));
    CHECK_EQUAL(LIST_SECTION_FIRST_LETTER + 'h' - 'a', section_at(albums, 2));
    CHECK_EQUAL(LIST_SECTION_FIRST_LETTER + 't' - 'a', section_at(albums, 4));
}

static void lettered_lists_never_go_backwards(void)
{
    static const BrowseKind LETTERED[] = {BROWSE_ARTISTS, BROWSE_ALBUM_ARTISTS, BROWSE_GENRES,
                                          BROWSE_ALBUMS, BROWSE_ALL_SONGS};
    for (size_t kind = 0; kind < sizeof LETTERED / sizeof LETTERED[0]; kind++) {
        BrowseScreen *browse = opened(browse_root_query(LETTERED[kind]));
        CHECK(browse->model.section != NULL);
        for (int index = 1; index < (int)browse->count; index++) {
            CHECK(section_at(browse, index - 1) <= section_at(browse, index));
        }
    }
}

static void album_tracks_have_no_letters(void)
{
    BrowseQuery tracks = {.kind = BROWSE_ALBUM_TRACKS,
                          .group = LIBRARY_ARTISTS,
                          .subject = 0,
                          .artist = LIBRARY_NONE};
    CHECK(opened(tracks)->model.section == NULL);
}

static void check_rows_match_the_library(BrowseScreen *browse, int rows)
{
    int top = list_view_top(&browse->list);
    for (int row = 0; row < rows; row++) {
        char label[BROWSE_LABEL_CAPACITY];
        char detail[BROWSE_DETAIL_CAPACITY];
        BrowseText text = {label, sizeof label, detail, sizeof detail};
        BrowseItem item;
        CHECK(
            browse_item(browse->pool->config.library, browse->query, (uint32_t)(top + row), &item));
        CHECK(browse_item_text(browse->pool->config.library, browse->query, item, text));
        CHECK(strcmp(label, browse->model.label(browse, top + row)) == 0);
    }
}

static void scrolling_one_row_at_a_time_keeps_every_row_right(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    albums->list.visible_rows = 3;
    albums->screen.update(albums, 0);
    for (int step = 0; step < 4; step++) {
        list_view_move(&albums->list, 1, false, 0);
        albums->screen.update(albums, 0);
        check_rows_match_the_library(albums, 3);
    }
    for (int step = 0; step < 4; step++) {
        list_view_move(&albums->list, -1, false, 0);
        albums->screen.update(albums, 0);
        check_rows_match_the_library(albums, 3);
    }
}

static void a_jump_longer_than_the_window_reloads_every_row(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    albums->list.visible_rows = 2;
    albums->screen.update(albums, 0);
    list_view_select(&albums->list, 4, 0);
    albums->screen.update(albums, 0);
    check_rows_match_the_library(albums, 1);
    list_view_select(&albums->list, 0, 0);
    albums->screen.update(albums, 0);
    check_rows_match_the_library(albums, 2);
}

static void scrolling_one_row_loads_only_the_new_row(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    albums->list.visible_rows = 3;
    albums->screen.update(albums, 0);
    favorites.revision++;
    fixture.memory.reads = 0;
    albums->screen.update(albums, 0);
    int full_reload = fixture.memory.reads;
    list_view_select(&albums->list, 3, 0);
    fixture.memory.reads = 0;
    albums->screen.update(albums, 0);
    CHECK(fixture.memory.reads < full_reload);
}

static void coming_back_to_a_list_reads_nothing(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    albums->screen.update(albums, 0);
    fixture.memory.reads = 0;
    albums->screen.enter(albums, 0);
    albums->screen.update(albums, 0);
    CHECK_EQUAL(0, fixture.memory.reads);
    check_rows_match_the_library(albums, 3);
}

static bool row_has_star(const BrowseScreen *browse, int index)
{
    const char *detail = browse->model.detail(browse, index);
    return detail != NULL && strstr(detail, "\xE2\x98\x85") != NULL;
}

static void a_favorite_added_elsewhere_shows_when_the_list_comes_back(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    BrowseItem item;
    CHECK(browse_item(pool.config.library, albums->query, 0, &item));
    CHECK(!row_has_star(albums, 0));
    favorites_store_toggle_album(&favorites, item.index);
    albums->screen.enter(albums, 0);
    CHECK(row_has_star(albums, 0));
}

static void opening_another_list_at_the_same_level_shows_its_own_rows(void)
{
    BrowseScreen *albums = opened(browse_root_query(BROWSE_ALBUMS));
    albums->screen.update(albums, 0);
    Screen *artists = browse_pool_open(&pool, 0, browse_root_query(BROWSE_ARTISTS));
    artists->enter(artists->context, 0);
    check_rows_match_the_library(artists->context, 3);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(albums_are_lettered_by_their_sort_key),
        TEST_CASE(lettered_lists_never_go_backwards),
        TEST_CASE(album_tracks_have_no_letters),
        TEST_CASE(a_folder_with_tracks_starts_with_shuffle_all),
        TEST_CASE(a_folder_of_folders_still_starts_with_shuffle_all),
        TEST_CASE(the_root_folder_has_no_shuffle_all),
        TEST_CASE(shuffle_all_plays_the_folders_tracks_shuffled),
        TEST_CASE(shuffle_all_plays_tracks_of_subfolders_too),
        TEST_CASE(scrolling_one_row_at_a_time_keeps_every_row_right),
        TEST_CASE(a_jump_longer_than_the_window_reloads_every_row),
        TEST_CASE(scrolling_one_row_loads_only_the_new_row),
        TEST_CASE(coming_back_to_a_list_reads_nothing),
        TEST_CASE(a_favorite_added_elsewhere_shows_when_the_list_comes_back),
        TEST_CASE(opening_another_list_at_the_same_level_shows_its_own_rows),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
