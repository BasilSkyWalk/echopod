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

enum {
    MANY_COVERS = 200,
    COVER_RECORD_BYTES = 31360,
    MANY_INDEX_BYTES = MANY_COVERS * COVERS_ENTRY_SIZE
};

static uint8_t many_bytes[COVERS_HEADER_SIZE + MANY_INDEX_BYTES + MANY_COVERS * COVER_RECORD_BYTES];
static MemorySource many_memory;

static void put_u16(uint8_t *target, uint32_t offset, uint32_t value)
{
    target[offset] = (uint8_t)value;
    target[offset + 1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *target, uint32_t offset, uint32_t value)
{
    put_u16(target, offset, value & 0xFFFFu);
    put_u16(target, offset + 2, value >> 16);
}

static uint32_t many_key(uint32_t index)
{
    return index * 10 + 5;
}

static uint32_t many_record(uint32_t index)
{
    return COVERS_HEADER_SIZE + MANY_INDEX_BYTES + index * COVER_RECORD_BYTES;
}

static Covers open_many_covers(void)
{
    memcpy(many_bytes, "EMCV", 4);
    put_u16(many_bytes, 4, COVERS_VERSION);
    put_u16(many_bytes, 6, MANY_COVERS);
    put_u16(many_bytes, 8, COVER_FULL_SIZE);
    put_u16(many_bytes, 10, COVER_HALF_SIZE);
    for (uint32_t index = 0; index < MANY_COVERS; index++) {
        put_u32(many_bytes, COVERS_HEADER_SIZE + index * COVERS_ENTRY_SIZE, many_key(index));
        put_u32(many_bytes, COVERS_HEADER_SIZE + index * COVERS_ENTRY_SIZE + 4, many_record(index));
    }
    many_memory = (MemorySource){.bytes = many_bytes, .size = sizeof many_bytes};
    Covers covers;
    CHECK(covers_open(&covers, memory_source(&many_memory), sizeof many_bytes));
    return covers;
}

static void every_cover_of_a_long_index_is_found(void)
{
    Covers covers = open_many_covers();
    uint32_t record = 0;
    for (uint32_t index = 0; index < MANY_COVERS; index++) {
        CHECK(covers_find(&covers, many_key(index), &record));
        CHECK_EQUAL(many_record(index), record);
    }
}

static void keys_between_the_entries_are_not_found(void)
{
    Covers covers = open_many_covers();
    uint32_t record = 0;
    CHECK(!covers_find(&covers, 0, &record));
    CHECK(!covers_find(&covers, many_key(77) + 1, &record));
    CHECK(!covers_find(&covers, many_key(MANY_COVERS), &record));
}

static void a_repeated_search_reads_the_index_only_once_or_twice(void)
{
    Covers covers = open_many_covers();
    uint32_t record = 0;
    CHECK(covers_find(&covers, many_key(30), &record));
    many_memory.reads = 0;
    CHECK(covers_find(&covers, many_key(31), &record));
    CHECK(many_memory.reads <= 2);
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
        TEST_CASE(every_cover_of_a_long_index_is_found),
        TEST_CASE(keys_between_the_entries_are_not_found),
        TEST_CASE(a_repeated_search_reads_the_index_only_once_or_twice),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
