#include <stdbool.h>
#include <stdint.h>

#include "big_library.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "screens/cover_flow_screen.h"

enum {
    BIG_ALBUMS = 2500,
    GROUP_FIRST_LINK = 4,
    GROUP_LINK_COUNT = 8,
    PLAYLIST_TRACKS = 1000,
    PLAYLIST_TRACKS_PER_ALBUM = 2,
    PLAYLIST_ALBUMS = PLAYLIST_TRACKS / PLAYLIST_TRACKS_PER_ALBUM,
    FEW_READS = 64
};

static FixtureLibrary fixture;
static BigLibrary big;
static uint8_t seen[BIG_ALBUMS];
static Player player;
static FavoritesStore favorites;
static CoverCache covers;
static CoverFlowScreen flow;

static int album_at(const Library *library, CoverFlowOrder order, uint32_t position)
{
    uint16_t album = 0;
    return cover_flow_order_album(library, order, position, &album) ? album : -1;
}

static void albums_follow_their_album_artists(void)
{
    Library *library = fixture_library_open(&fixture);
    CoverFlowOrder order = cover_flow_order_make(library);
    CHECK_EQUAL(FIXTURE_ALBUMS, order.count);
    uint32_t position = 0;
    for (uint32_t index = 0; index < library_count(library, LIBRARY_ALBUM_ARTISTS); index++) {
        LibraryGroup group;
        CHECK(library_read_group(library, LIBRARY_ALBUM_ARTISTS, index, &group));
        for (uint32_t link = 0; link < group.link_count; link++) {
            uint16_t album = 0;
            CHECK(library_read_index(library, LIBRARY_LINKS, group.first_link + link, &album));
            CHECK_EQUAL(album, album_at(library, order, position++));
        }
    }
    CHECK_EQUAL(-1, album_at(library, order, position));
}

static void each_album_is_found_where_it_is_shown(void)
{
    Library *library = fixture_library_open(&fixture);
    CoverFlowOrder order = cover_flow_order_make(library);
    for (uint32_t position = 0; position < order.count; position++) {
        uint32_t found = 0;
        CHECK(cover_flow_order_find(library, order, (uint16_t)album_at(library, order, position),
                                    &found));
        CHECK_EQUAL(position, found);
    }
    uint32_t found = 0;
    CHECK(!cover_flow_order_find(library, order, FIXTURE_ALBUMS, &found));
}

static void every_album_of_a_big_library_is_shown_once(void)
{
    Library *library = big_library_open(&big, BIG_ALBUMS * 2, 2);
    CoverFlowOrder order = cover_flow_order_make(library);
    CHECK_EQUAL(BIG_ALBUMS, order.count);
    for (uint32_t position = 0; position < order.count; position++) {
        int album = album_at(library, order, position);
        CHECK(album >= 0 && album < BIG_ALBUMS && seen[album] == 0);
        seen[album] = 1;
        uint32_t found = 0;
        CHECK(cover_flow_order_find(library, order, (uint16_t)album, &found));
        CHECK_EQUAL(position, found);
    }
    big_library_close(&big);
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    for (int shift = 0; shift < 4; shift++) {
        bytes[shift] = (uint8_t)(value >> (shift * 8));
    }
}

static uint32_t get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static void swap_first_two_album_artists(Library *library)
{
    LibrarySectionInfo info = library->sections[LIBRARY_ALBUM_ARTISTS];
    uint8_t *first = fixture.bytes + info.offset;
    uint8_t *second = first + info.record_size;
    uint32_t first_link = get_u32(first + GROUP_FIRST_LINK);
    uint32_t first_count = first[GROUP_LINK_COUNT] | (uint32_t)first[GROUP_LINK_COUNT + 1] << 8;
    uint32_t second_count = second[GROUP_LINK_COUNT] | (uint32_t)second[GROUP_LINK_COUNT + 1] << 8;
    put_u32(first + GROUP_FIRST_LINK, first_link + second_count);
    put_u32(second + GROUP_FIRST_LINK, first_link);
    first[GROUP_LINK_COUNT] = (uint8_t)second_count;
    second[GROUP_LINK_COUNT] = (uint8_t)first_count;
}

static void out_of_order_links_fall_back_to_album_order(void)
{
    Library *library = fixture_library_open(&fixture);
    swap_first_two_album_artists(library);
    CoverFlowOrder order = cover_flow_order_make(library);
    CHECK(!order.linked);
    CHECK_EQUAL(FIXTURE_ALBUMS, order.count);
    for (uint32_t position = 0; position < FIXTURE_ALBUMS; position++) {
        CHECK_EQUAL(position, album_at(library, order, position));
        uint32_t found = 0;
        CHECK(cover_flow_order_find(library, order, (uint16_t)position, &found));
        CHECK_EQUAL(position, found);
    }
}

static Library *playlist_library(void)
{
    Library *library =
        big_library_open_with_playlist(&big, PLAYLIST_TRACKS, PLAYLIST_TRACKS_PER_ALBUM);
    fake_hal_reset();
    player_init(&player, library);
    return library;
}

static int reads_to_open_the_playlist(const Library *library)
{
    cover_flow_screen_init(&flow, (CoverFlowConfig){.library = library,
                                                    .theme = &DEFAULT_THEME,
                                                    .covers = &covers,
                                                    .favorites = &favorites,
                                                    .player = &player});
    big.memory.reads = 0;
    Screen *screen = cover_flow_screen_open_group(&flow, LIBRARY_PLAYLISTS, 0, 1);
    CHECK(screen != NULL);
    screen->enter(screen->context, 0);
    return big.memory.reads;
}

static void play_playlist_track(uint32_t position)
{
    BrowseQuery everything = {.kind = BROWSE_PLAYLIST_TRACKS,
                              .group = LIBRARY_PLAYLISTS,
                              .subject = 0,
                              .artist = LIBRARY_NONE};
    CHECK(player_play_list(&player, everything, position, 1));
}

static void a_playlist_flow_finds_the_playing_album_while_counting_its_tiles(void)
{
    const Library *library = playlist_library();
    int idle = reads_to_open_the_playlist(library);
    CHECK_EQUAL(PLAYLIST_ALBUMS + 1, flow.count);
    CHECK_EQUAL(1, flow.target);
    play_playlist_track(PLAYLIST_TRACKS - 1);
    int playing = reads_to_open_the_playlist(library);
    CHECK_EQUAL(PLAYLIST_ALBUMS, flow.target);
    CHECK(playing < idle + FEW_READS);
    big_library_close(&big);
}

static void a_playlist_flow_starts_at_the_first_tile_of_the_playing_album(void)
{
    const Library *library = playlist_library();
    play_playlist_track(PLAYLIST_TRACKS / 2 + 1);
    reads_to_open_the_playlist(library);
    CHECK_EQUAL(PLAYLIST_ALBUMS / 2 + 1, flow.target);
    const CoverFlowTile *tile = &flow.tiles[flow.target % COVER_FLOW_TILES];
    CHECK_EQUAL(flow.target, tile->position);
    CHECK_EQUAL(flow.order.first_link + PLAYLIST_TRACKS / 2, tile->link);
    big_library_close(&big);
}

static void opening_a_long_playlist_flow_keeps_the_audio_fed(void)
{
    const Library *library = playlist_library();
    int before = fake_hal_audio_service_count();
    reads_to_open_the_playlist(library);
    CHECK(fake_hal_audio_service_count() > before);
    big_library_close(&big);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(albums_follow_their_album_artists),
        TEST_CASE(each_album_is_found_where_it_is_shown),
        TEST_CASE(every_album_of_a_big_library_is_shown_once),
        TEST_CASE(out_of_order_links_fall_back_to_album_order),
        TEST_CASE(a_playlist_flow_finds_the_playing_album_while_counting_its_tiles),
        TEST_CASE(a_playlist_flow_starts_at_the_first_tile_of_the_playing_album),
        TEST_CASE(opening_a_long_playlist_flow_keeps_the_audio_fed),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
