#include <string.h>

#include "check.h"
#include "fixture_library.h"
#include "library/library_browse.h"

enum {
    TEXT_CAPACITY = 64,
    BJORK = 3,
    VARIOUS_ARTISTS = 3,
    METAL = 1,
    BACK_IN_BLACK = 0,
    BEST_OF = 1,
    HIGH_VOLTAGE = 2,
    HOMOGENIC = 3,
    HELLS_BELLS = 0,
    HELP = 2,
    ARMY_OF_ME = 3,
    HUNTER = 5,
    JOGA = 6,
    BJORK_FOLDER = 6,
    HOMOGENIC_FOLDER = 7
};

static FixtureLibrary fixture;

static Library *open_fixture(void)
{
    return fixture_library_open(&fixture);
}

static BrowseItem item_at(Library *library, BrowseQuery query, uint32_t position)
{
    BrowseItem item = {.kind = BROWSE_ITEM_GROUP, .group = LIBRARY_ARTISTS, .index = 0xFFFF};
    CHECK(browse_item(library, query, position, &item));
    return item;
}

static BrowseQuery child_of(BrowseQuery parent, BrowseItem item)
{
    BrowseQuery child = browse_root_query(BROWSE_ALBUMS);
    CHECK(browse_child(parent, item, &child));
    return child;
}

static bool label_is(Library *library, BrowseQuery query, BrowseItem item, const char *expected)
{
    char label[TEXT_CAPACITY];
    char detail[TEXT_CAPACITY];
    BrowseText text = {label, sizeof label, detail, sizeof detail};
    return browse_item_text(library, query, item, text) && strcmp(expected, label) == 0;
}

static bool detail_is(Library *library, BrowseQuery query, BrowseItem item, const char *expected)
{
    char label[TEXT_CAPACITY];
    char detail[TEXT_CAPACITY];
    BrowseText text = {label, sizeof label, detail, sizeof detail};
    return browse_item_text(library, query, item, text) && strcmp(expected, detail) == 0;
}

static bool title_is(Library *library, BrowseQuery query, const char *expected)
{
    char title[TEXT_CAPACITY];
    return browse_title(library, query, title, sizeof title) && strcmp(expected, title) == 0;
}

static void top_level_views_count_their_tables(void)
{
    Library *library = open_fixture();
    CHECK_EQUAL(4, browse_count(library, browse_root_query(BROWSE_ARTISTS)));
    CHECK_EQUAL(4, browse_count(library, browse_root_query(BROWSE_ALBUM_ARTISTS)));
    CHECK_EQUAL(4, browse_count(library, browse_root_query(BROWSE_GENRES)));
    CHECK_EQUAL(5, browse_count(library, browse_root_query(BROWSE_ALBUMS)));
    CHECK_EQUAL(8, browse_count(library, browse_root_query(BROWSE_ALL_SONGS)));
    CHECK_EQUAL(4, browse_count(library, browse_root_query(BROWSE_FOLDER)));
}

static void top_level_items_point_at_their_records(void)
{
    Library *library = open_fixture();
    BrowseItem artist = item_at(library, browse_root_query(BROWSE_ARTISTS), BJORK);
    CHECK_EQUAL(BROWSE_ITEM_GROUP, artist.kind);
    CHECK_EQUAL(LIBRARY_ARTISTS, artist.group);
    CHECK_EQUAL(BJORK, artist.index);
    BrowseItem genre = item_at(library, browse_root_query(BROWSE_GENRES), METAL);
    CHECK_EQUAL(LIBRARY_GENRES, genre.group);
    CHECK_EQUAL(BROWSE_ITEM_ALBUM, item_at(library, browse_root_query(BROWSE_ALBUMS), 1).kind);
    BrowseItem first_song = item_at(library, browse_root_query(BROWSE_ALL_SONGS), 0);
    CHECK_EQUAL(BROWSE_ITEM_TRACK, first_song.kind);
    CHECK_EQUAL(ARMY_OF_ME, first_song.index);
}

static void an_artist_opens_their_albums_in_order(void)
{
    Library *library = open_fixture();
    BrowseQuery artists = browse_root_query(BROWSE_ARTISTS);
    BrowseQuery albums = child_of(artists, item_at(library, artists, BJORK));
    CHECK_EQUAL(BROWSE_GROUP_ALBUMS, albums.kind);
    CHECK_EQUAL(2, browse_count(library, albums));
    CHECK_EQUAL(HOMOGENIC, item_at(library, albums, 0).index);
    CHECK_EQUAL(BEST_OF, item_at(library, albums, 1).index);
}

static void an_album_reached_through_an_artist_lists_only_their_tracks(void)
{
    Library *library = open_fixture();
    BrowseQuery artists = browse_root_query(BROWSE_ARTISTS);
    BrowseQuery albums = child_of(artists, item_at(library, artists, BJORK));
    BrowseQuery tracks = child_of(albums, item_at(library, albums, 1));
    CHECK_EQUAL(BROWSE_ALBUM_TRACKS, tracks.kind);
    CHECK_EQUAL(1, browse_count(library, tracks));
    CHECK_EQUAL(ARMY_OF_ME, item_at(library, tracks, 0).index);
}

static void an_album_reached_directly_lists_every_track(void)
{
    Library *library = open_fixture();
    BrowseQuery albums = browse_root_query(BROWSE_ALBUMS);
    BrowseQuery tracks = child_of(albums, item_at(library, albums, BEST_OF));
    CHECK_EQUAL(2, browse_count(library, tracks));
    CHECK_EQUAL(HELP, item_at(library, tracks, 0).index);
    CHECK_EQUAL(ARMY_OF_ME, item_at(library, tracks, 1).index);
}

static void album_artists_and_genres_open_their_albums(void)
{
    Library *library = open_fixture();
    BrowseQuery album_artists = browse_root_query(BROWSE_ALBUM_ARTISTS);
    BrowseQuery various = child_of(album_artists, item_at(library, album_artists, VARIOUS_ARTISTS));
    CHECK_EQUAL(1, browse_count(library, various));
    CHECK_EQUAL(BEST_OF, item_at(library, various, 0).index);
    BrowseQuery genres = browse_root_query(BROWSE_GENRES);
    BrowseQuery metal = child_of(genres, item_at(library, genres, METAL));
    CHECK_EQUAL(2, browse_count(library, metal));
    CHECK_EQUAL(BACK_IN_BLACK, item_at(library, metal, 0).index);
    CHECK_EQUAL(HIGH_VOLTAGE, item_at(library, metal, 1).index);
}

static void folders_list_subfolders_before_tracks(void)
{
    Library *library = open_fixture();
    BrowseQuery root = browse_root_query(BROWSE_FOLDER);
    BrowseItem bjork = item_at(library, root, 2);
    CHECK_EQUAL(BROWSE_ITEM_FOLDER, bjork.kind);
    CHECK_EQUAL(BJORK_FOLDER, bjork.index);
    BrowseQuery bjork_folder = child_of(root, bjork);
    BrowseQuery homogenic = child_of(bjork_folder, item_at(library, bjork_folder, 0));
    CHECK_EQUAL(HOMOGENIC_FOLDER, homogenic.subject);
    CHECK_EQUAL(2, browse_count(library, homogenic));
    CHECK_EQUAL(BROWSE_ITEM_TRACK, item_at(library, homogenic, 0).kind);
    CHECK_EQUAL(HUNTER, item_at(library, homogenic, 0).index);
    CHECK_EQUAL(JOGA, item_at(library, homogenic, 1).index);
}

static void tracks_open_nothing(void)
{
    BrowseQuery child;
    BrowseItem track = {.kind = BROWSE_ITEM_TRACK, .group = LIBRARY_ARTISTS, .index = 0};
    CHECK(!browse_child(browse_root_query(BROWSE_ALL_SONGS), track, &child));
}

static void positions_past_the_end_are_refused(void)
{
    Library *library = open_fixture();
    BrowseItem item;
    CHECK(!browse_item(library, browse_root_query(BROWSE_ALBUMS), 5, &item));
    CHECK(!browse_item(library, browse_root_query(BROWSE_FOLDER), 4, &item));
}

static void items_show_names_years_and_durations(void)
{
    Library *library = open_fixture();
    BrowseQuery artists = browse_root_query(BROWSE_ARTISTS);
    BrowseQuery albums = browse_root_query(BROWSE_ALBUMS);
    BrowseQuery songs = browse_root_query(BROWSE_ALL_SONGS);
    CHECK(label_is(library, artists, item_at(library, artists, BJORK), "Björk"));
    CHECK(detail_is(library, artists, item_at(library, artists, BJORK), ""));
    CHECK(label_is(library, albums, item_at(library, albums, BEST_OF), "The Best of Something"));
    CHECK(detail_is(library, albums, item_at(library, albums, BEST_OF), "2001"));
    CHECK(label_is(library, songs, item_at(library, songs, 1), "Hells Bells"));
    CHECK(detail_is(library, songs, item_at(library, songs, 1), "1:00"));
}

static void folder_views_show_folder_names_and_file_names(void)
{
    Library *library = open_fixture();
    BrowseQuery root = browse_root_query(BROWSE_FOLDER);
    CHECK(label_is(library, root, item_at(library, root, 0), "AC DC"));
    BrowseQuery folder = {
        .kind = BROWSE_FOLDER, .group = LIBRARY_ARTISTS, .subject = 2, .artist = LIBRARY_NONE};
    BrowseItem hells_bells = item_at(library, folder, 0);
    CHECK_EQUAL(HELLS_BELLS, hells_bells.index);
    CHECK(label_is(library, folder, hells_bells, "1-01 - Hells Bells"));
}

static void views_have_titles(void)
{
    Library *library = open_fixture();
    BrowseQuery artists = browse_root_query(BROWSE_ARTISTS);
    BrowseQuery albums = browse_root_query(BROWSE_ALBUMS);
    CHECK(title_is(library, artists, "Artists"));
    CHECK(title_is(library, browse_root_query(BROWSE_ALBUM_ARTISTS), "Album Artists"));
    CHECK(title_is(library, browse_root_query(BROWSE_GENRES), "Genres"));
    CHECK(title_is(library, albums, "Albums"));
    CHECK(title_is(library, browse_root_query(BROWSE_ALL_SONGS), "All Songs"));
    CHECK(title_is(library, browse_root_query(BROWSE_FOLDER), "Folders"));
    CHECK(title_is(library, child_of(artists, item_at(library, artists, BJORK)), "Björk"));
    CHECK(title_is(library, child_of(albums, item_at(library, albums, HOMOGENIC)), "Homogenic"));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(top_level_views_count_their_tables),
        TEST_CASE(top_level_items_point_at_their_records),
        TEST_CASE(an_artist_opens_their_albums_in_order),
        TEST_CASE(an_album_reached_through_an_artist_lists_only_their_tracks),
        TEST_CASE(an_album_reached_directly_lists_every_track),
        TEST_CASE(album_artists_and_genres_open_their_albums),
        TEST_CASE(folders_list_subfolders_before_tracks),
        TEST_CASE(tracks_open_nothing),
        TEST_CASE(positions_past_the_end_are_refused),
        TEST_CASE(items_show_names_years_and_durations),
        TEST_CASE(folder_views_show_folder_names_and_file_names),
        TEST_CASE(views_have_titles),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
