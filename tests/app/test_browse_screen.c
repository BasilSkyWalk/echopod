#include <string.h>

#include "browse_screen.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"

static FixtureLibrary fixture;
static BrowsePool pool;
static FavoritesStore favorites;

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
        TEST_CASE(scrolling_one_row_at_a_time_keeps_every_row_right),
        TEST_CASE(a_jump_longer_than_the_window_reloads_every_row),
        TEST_CASE(scrolling_one_row_loads_only_the_new_row),
        TEST_CASE(coming_back_to_a_list_reads_nothing),
        TEST_CASE(a_favorite_added_elsewhere_shows_when_the_list_comes_back),
        TEST_CASE(opening_another_list_at_the_same_level_shows_its_own_rows),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
