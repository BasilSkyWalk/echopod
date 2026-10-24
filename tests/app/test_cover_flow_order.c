#include <stdbool.h>
#include <stdint.h>

#include "big_library.h"
#include "check.h"
#include "fixture_library.h"
#include "screens/cover_flow_screen.h"

enum {
    BIG_ALBUMS = 2500,
    GROUP_FIRST_LINK = 4,
    GROUP_LINK_COUNT = 8
};

static FixtureLibrary fixture;
static BigLibrary big;
static uint8_t seen[BIG_ALBUMS];

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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(albums_follow_their_album_artists),
        TEST_CASE(each_album_is_found_where_it_is_shown),
        TEST_CASE(every_album_of_a_big_library_is_shown_once),
        TEST_CASE(out_of_order_links_fall_back_to_album_order),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
