#include <string.h>

#include "check.h"
#include "fixture_library.h"
#include "library/covers.h"

enum {
    RED_ALBUM = 0,
    BARE_ALBUM = 1,
    BLUE_ALBUM = 2,
    RED = 0xF800,
    BLUE = 0x001F
};

static FixtureLibrary fixture;
static uint8_t bytes[FIXTURE_COVERS_CAPACITY];
static MemorySource memory;
static uint16_t pixels[COVER_FULL_PIXELS];

static uint32_t load_covers(void)
{
    uint32_t size = 0;
    const uint8_t *source = fixture_covers_bytes(&size);
    memcpy(bytes, source, size);
    memory = (MemorySource){.bytes = bytes, .size = size};
    return size;
}

static Covers open_covers(void)
{
    uint32_t size = load_covers();
    Covers covers;
    CHECK(covers_open(&covers, memory_source(&memory), size));
    return covers;
}

static bool all_pixels_are(int count, uint16_t color)
{
    for (int index = 0; index < count; index++) {
        if (pixels[index] != color) {
            return false;
        }
    }
    return true;
}

static uint32_t key_of(uint32_t album)
{
    return library_album_key(fixture_library_open(&fixture), album);
}

static void the_fixture_holds_two_covers(void)
{
    Covers covers = open_covers();
    CHECK_EQUAL(2, covers.count);
}

static void a_cover_is_found_by_album_key_in_both_sizes(void)
{
    Covers covers = open_covers();
    uint32_t record = 0;
    CHECK(covers_find(&covers, key_of(RED_ALBUM), &record));
    CHECK(covers_read(&covers, record, COVER_FULL, pixels));
    CHECK(all_pixels_are(COVER_FULL_PIXELS, RED));
    CHECK(covers_read(&covers, record, COVER_HALF, pixels));
    CHECK(all_pixels_are(COVER_HALF_PIXELS, RED));
    CHECK(covers_find(&covers, key_of(BLUE_ALBUM), &record));
    CHECK(covers_read(&covers, record, COVER_HALF, pixels));
    CHECK(all_pixels_are(COVER_HALF_PIXELS, BLUE));
}

static void an_album_without_art_is_not_found(void)
{
    Covers covers = open_covers();
    uint32_t record = 0;
    CHECK(!covers_find(&covers, key_of(BARE_ALBUM), &record));
}

static void a_file_with_the_wrong_magic_is_rejected(void)
{
    uint32_t size = load_covers();
    bytes[0] = 'X';
    Covers covers;
    CHECK(!covers_open(&covers, memory_source(&memory), size));
}

static void a_file_with_other_cover_sizes_is_rejected(void)
{
    uint32_t size = load_covers();
    bytes[8] = 100;
    Covers covers;
    CHECK(!covers_open(&covers, memory_source(&memory), size));
}

static void a_truncated_file_is_rejected(void)
{
    uint32_t size = load_covers();
    Covers covers;
    CHECK(!covers_open(&covers, memory_source(&memory), size - 1));
    CHECK(!covers_open(&covers, memory_source(&memory), 4));
}

static void a_failing_read_is_reported(void)
{
    Covers covers = open_covers();
    uint32_t record = 0;
    CHECK(covers_find(&covers, key_of(RED_ALBUM), &record));
    memory.failing = true;
    CHECK(!covers_read(&covers, record, COVER_FULL, pixels));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_fixture_holds_two_covers),
        TEST_CASE(a_cover_is_found_by_album_key_in_both_sizes),
        TEST_CASE(an_album_without_art_is_not_found),
        TEST_CASE(a_file_with_the_wrong_magic_is_rejected),
        TEST_CASE(a_file_with_other_cover_sizes_is_rejected),
        TEST_CASE(a_truncated_file_is_rejected),
        TEST_CASE(a_failing_read_is_reported),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
