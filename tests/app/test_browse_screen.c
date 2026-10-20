#include "browse_screen.h"
#include "check.h"
#include "default_theme.h"
#include "fixture_library.h"

static FixtureLibrary fixture;
static BrowsePool pool;

static BrowseScreen *opened(BrowseQuery query)
{
    browse_pool_init(&pool, (BrowsePoolConfig){.library = fixture_library_open(&fixture),
                                               .theme = &DEFAULT_THEME});
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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(albums_are_lettered_by_their_sort_key),
        TEST_CASE(lettered_lists_never_go_backwards),
        TEST_CASE(album_tracks_have_no_letters),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
