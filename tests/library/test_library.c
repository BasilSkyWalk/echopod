#include <string.h>

#include "check.h"
#include "fixture_library.h"

enum {
    STRING_CAPACITY = 64,
    VERSION_OFFSET = 4,
    TRACKS_OFFSET_FIELD = 8,
    BJORK_ARTIST = 3,
    HOMOGENIC_FOLDER = 7,
    BACK_IN_BLACK_FOLDER = 2,
    BEST_OF_ALBUM = 1,
    VARIOUS_ARTISTS = 3,
    POP = 2
};

static FixtureLibrary fixture;

static bool string_is(Library *library, uint32_t offset, const char *expected)
{
    char buffer[STRING_CAPACITY];
    return library_read_string(library, offset, buffer, sizeof buffer) &&
           strcmp(expected, buffer) == 0;
}

static uint32_t fixture_size(void)
{
    uint32_t size = 0;
    fixture_bytes(&size);
    return size;
}

static LibraryStatus open_modified(uint32_t offset, uint8_t value, uint32_t size)
{
    fixture_library_open(&fixture);
    fixture.bytes[offset] = value;
    fixture.memory.size = size;
    Library library;
    return library_open(&library, memory_source(&fixture.memory), size);
}

static void the_fixture_opens_with_its_counts(void)
{
    Library *library = fixture_library_open(&fixture);
    CHECK_EQUAL(LIBRARY_OK, library_open(library, memory_source(&fixture.memory), fixture.size));
    CHECK_EQUAL(FIXTURE_TRACKS, library_count(library, LIBRARY_TRACKS));
    CHECK_EQUAL(FIXTURE_ALBUMS, library_count(library, LIBRARY_ALBUMS));
    CHECK_EQUAL(FIXTURE_GROUPS, library_count(library, LIBRARY_ARTISTS));
    CHECK_EQUAL(FIXTURE_GROUPS, library_count(library, LIBRARY_ALBUM_ARTISTS));
    CHECK_EQUAL(FIXTURE_GROUPS, library_count(library, LIBRARY_GENRES));
    CHECK_EQUAL(FIXTURE_FOLDERS, library_count(library, LIBRARY_FOLDERS));
    CHECK_EQUAL(FIXTURE_TRACKS, library_count(library, LIBRARY_TITLE_ORDER));
    CHECK_EQUAL(1, library_count(library, LIBRARY_PLAYLISTS));
}

static void a_wrong_magic_is_rejected(void)
{
    CHECK_EQUAL(LIBRARY_BAD_MAGIC, open_modified(0, 'X', fixture_size()));
}

static void a_newer_version_is_rejected(void)
{
    CHECK_EQUAL(LIBRARY_BAD_VERSION, open_modified(VERSION_OFFSET, 3, fixture_size()));
}

static void a_truncated_file_is_rejected(void)
{
    CHECK_EQUAL(LIBRARY_BAD_LAYOUT, open_modified(0, 'E', fixture_size() - 10));
}

static void a_section_inside_the_header_is_rejected(void)
{
    CHECK_EQUAL(LIBRARY_BAD_LAYOUT, open_modified(TRACKS_OFFSET_FIELD, 16, fixture_size()));
}

static void an_unreadable_source_is_reported(void)
{
    fixture_library_open(&fixture);
    fixture.memory.failing = true;
    Library library;
    CHECK_EQUAL(LIBRARY_UNREADABLE,
                library_open(&library, memory_source(&fixture.memory), fixture.size));
}

static void tracks_decode_every_field(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryTrack track;
    CHECK(library_read_track(library, 0, &track));
    CHECK(string_is(library, track.title, "Hells Bells"));
    CHECK(string_is(library, track.path, "AC DC/Back In Black/1-01 - Hells Bells.flac"));
    CHECK_EQUAL(0, track.album);
    CHECK_EQUAL(0, track.artist);
    CHECK_EQUAL(1, track.genre);
    CHECK_EQUAL(BACK_IN_BLACK_FOLDER, track.folder);
    CHECK_EQUAL(60000, track.duration_ms);
    CHECK_EQUAL(1, track.disc);
    CHECK_EQUAL(1, track.number);
}

static void albums_decode_every_field(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryAlbum album;
    CHECK(library_read_album(library, BEST_OF_ALBUM, &album));
    CHECK(string_is(library, album.title, "The Best of Something"));
    CHECK_EQUAL(VARIOUS_ARTISTS, album.album_artist);
    CHECK_EQUAL(POP, album.genre);
    CHECK_EQUAL(2001, album.year);
    CHECK_EQUAL(2, album.first_track);
    CHECK_EQUAL(2, album.track_count);
    CHECK_EQUAL(LIBRARY_NONE, album.cover);
}

static void groups_link_to_their_albums(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryGroup bjork;
    uint16_t first = 0;
    uint16_t second = 0;
    CHECK(library_read_group(library, LIBRARY_ARTISTS, BJORK_ARTIST, &bjork));
    CHECK(string_is(library, bjork.name, "Björk"));
    CHECK_EQUAL(2, bjork.link_count);
    CHECK(library_read_index(library, LIBRARY_LINKS, bjork.first_link, &first));
    CHECK(library_read_index(library, LIBRARY_LINKS, bjork.first_link + 1, &second));
    CHECK_EQUAL(3, first);
    CHECK_EQUAL(BEST_OF_ALBUM, second);
}

static void folders_list_subfolders_then_tracks(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryFolder root;
    LibraryFolder homogenic;
    uint16_t first_track = 0;
    CHECK(library_read_folder(library, 0, &root));
    CHECK(string_is(library, root.name, ""));
    CHECK_EQUAL(LIBRARY_NONE, root.parent);
    CHECK_EQUAL(4, root.subfolder_count);
    CHECK_EQUAL(0, root.track_count);
    CHECK(library_read_folder(library, HOMOGENIC_FOLDER, &homogenic));
    CHECK(string_is(library, homogenic.name, "Homogenic"));
    CHECK_EQUAL(2, homogenic.track_count);
    CHECK(library_read_index(library, LIBRARY_LINKS, homogenic.first_link, &first_track));
    CHECK_EQUAL(5, first_track);
}

static void the_title_order_starts_with_the_first_title(void)
{
    Library *library = fixture_library_open(&fixture);
    uint16_t first = 0;
    LibraryTrack track;
    CHECK(library_read_index(library, LIBRARY_TITLE_ORDER, 0, &first));
    CHECK(library_read_track(library, first, &track));
    CHECK(string_is(library, track.title, "Army of Me"));
}

static void short_buffers_cut_strings_at_a_character_boundary(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryGroup bjork;
    char buffer[4];
    library_read_group(library, LIBRARY_ARTISTS, BJORK_ARTIST, &bjork);
    CHECK(library_read_string(library, bjork.name, buffer, sizeof buffer));
    CHECK(strcmp("Bj", buffer) == 0);
}

static void indices_out_of_range_are_refused(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryTrack track;
    LibraryAlbum album;
    uint16_t value;
    CHECK(!library_read_track(library, FIXTURE_TRACKS, &track));
    CHECK(!library_read_album(library, FIXTURE_ALBUMS, &album));
    CHECK(!library_read_index(library, LIBRARY_TITLE_ORDER, FIXTURE_TRACKS, &value));
    CHECK(!library_read_index(library, LIBRARY_TRACKS, 0, &value));
}

static uint32_t fingerprint_with_track_count(uint8_t track_count)
{
    fixture_library_open(&fixture);
    fixture.bytes[TRACKS_OFFSET_FIELD + 4] = track_count;
    Library library;
    library_open(&library, memory_source(&fixture.memory), fixture.size);
    return library_fingerprint(&library);
}

static void the_fingerprint_is_stable_for_the_same_library(void)
{
    uint32_t first = fingerprint_with_track_count(FIXTURE_TRACKS);
    CHECK(first != 0);
    CHECK_EQUAL(first, fingerprint_with_track_count(FIXTURE_TRACKS));
}

static void the_fingerprint_changes_when_the_library_changes(void)
{
    uint32_t original = fingerprint_with_track_count(FIXTURE_TRACKS);
    CHECK(original != fingerprint_with_track_count(FIXTURE_TRACKS - 1));
}

static void playlists_link_to_their_tracks_in_order(void)
{
    Library *library = fixture_library_open(&fixture);
    LibraryGroup road_trip;
    uint16_t tracks[3] = {0};
    CHECK(library_read_group(library, LIBRARY_PLAYLISTS, 0, &road_trip));
    CHECK(string_is(library, road_trip.name, "Road Trip"));
    CHECK_EQUAL(3, road_trip.link_count);
    for (uint32_t index = 0; index < 3; index++) {
        CHECK(library_read_index(library, LIBRARY_LINKS, road_trip.first_link + index,
                                 &tracks[index]));
    }
    CHECK_EQUAL(7, tracks[0]);
    CHECK_EQUAL(0, tracks[1]);
    CHECK_EQUAL(6, tracks[2]);
}

static void album_keys_are_stable_and_distinct(void)
{
    Library *library = fixture_library_open(&fixture);
    uint32_t first = library_album_key(library, 0);
    CHECK(first != 0);
    CHECK_EQUAL(first, library_album_key(library, 0));
    for (uint32_t album = 1; album < FIXTURE_ALBUMS; album++) {
        CHECK(first != library_album_key(library, album));
    }
    CHECK_EQUAL(0, library_album_key(library, FIXTURE_ALBUMS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_fixture_opens_with_its_counts),
        TEST_CASE(a_wrong_magic_is_rejected),
        TEST_CASE(a_newer_version_is_rejected),
        TEST_CASE(a_truncated_file_is_rejected),
        TEST_CASE(a_section_inside_the_header_is_rejected),
        TEST_CASE(an_unreadable_source_is_reported),
        TEST_CASE(tracks_decode_every_field),
        TEST_CASE(albums_decode_every_field),
        TEST_CASE(groups_link_to_their_albums),
        TEST_CASE(folders_list_subfolders_then_tracks),
        TEST_CASE(the_title_order_starts_with_the_first_title),
        TEST_CASE(short_buffers_cut_strings_at_a_character_boundary),
        TEST_CASE(indices_out_of_range_are_refused),
        TEST_CASE(playlists_link_to_their_tracks_in_order),
        TEST_CASE(album_keys_are_stable_and_distinct),
        TEST_CASE(the_fingerprint_is_stable_for_the_same_library),
        TEST_CASE(the_fingerprint_changes_when_the_library_changes),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
