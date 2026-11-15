#include <string.h>

#include "check.h"
#include "cover_cache.h"
#include "fake_hal.h"
#include "fixture_library.h"

enum {
    RED_ALBUM = 0,
    BARE_ALBUM = 1,
    BLUE_ALBUM = 2,
    RED = 0xF800,
    BLUE = 0x001F,
    GREEN = 0x07E0,
    SYNTHETIC_RECORD_BYTES = (COVER_FULL_PIXELS + COVER_HALF_PIXELS) * 2,
    SYNTHETIC_CAPACITY = COVERS_HEADER_SIZE + 2 * COVERS_ENTRY_SIZE + 2 * SYNTHETIC_RECORD_BYTES
};

static FixtureLibrary fixture;
static CoverCache cache;

static CoverCache *cache_with_covers(void)
{
    uint32_t size = 0;
    const uint8_t *bytes = fixture_covers_bytes(&size);
    fake_hal_reset();
    fake_hal_add_file("/.echo/covers.bin", bytes, size);
    cover_cache_open(&cache, fixture_library_open(&fixture));
    return &cache;
}

static uint8_t synthetic_covers[SYNTHETIC_CAPACITY];

static void put_le16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *bytes, uint32_t value)
{
    put_le16(bytes, (uint16_t)value);
    put_le16(bytes + 2, (uint16_t)(value >> 16));
}

static void fill_pixels(uint8_t *bytes, uint32_t count, uint16_t color)
{
    for (uint32_t index = 0; index < count; index++) {
        put_le16(bytes + index * 2, color);
    }
}

static CoverCache *cache_with_default(bool with_album_art)
{
    Library *library = fixture_library_open(&fixture);
    uint32_t keys[2];
    uint32_t count = 0;
    if (with_album_art) {
        keys[count++] = library_album_key(library, RED_ALBUM);
    }
    keys[count++] = ECHO_MINI_DEFAULT_COVER;
    if (count == 2 && keys[0] > keys[1]) {
        uint32_t swap = keys[0];
        keys[0] = keys[1];
        keys[1] = swap;
    }
    memset(synthetic_covers, 0, sizeof synthetic_covers);
    memcpy(synthetic_covers, "EMCV", 4);
    put_le16(synthetic_covers + 4, COVERS_VERSION);
    put_le16(synthetic_covers + 6, (uint16_t)count);
    put_le16(synthetic_covers + 8, COVER_FULL_SIZE);
    put_le16(synthetic_covers + 10, COVER_HALF_SIZE);
    uint32_t records = COVERS_HEADER_SIZE + count * COVERS_ENTRY_SIZE;
    put_le32(synthetic_covers + 12, records);
    for (uint32_t index = 0; index < count; index++) {
        uint32_t record = records + index * SYNTHETIC_RECORD_BYTES;
        put_le32(synthetic_covers + COVERS_HEADER_SIZE + index * COVERS_ENTRY_SIZE, keys[index]);
        put_le32(synthetic_covers + COVERS_HEADER_SIZE + index * COVERS_ENTRY_SIZE + 4, record);
        uint16_t color = keys[index] == ECHO_MINI_DEFAULT_COVER ? GREEN : RED;
        fill_pixels(synthetic_covers + record, COVER_FULL_PIXELS + COVER_HALF_PIXELS, color);
    }
    uint32_t size = records + count * SYNTHETIC_RECORD_BYTES;
    fake_hal_reset();
    fake_hal_add_file("/.echo/covers.bin", synthetic_covers, size);
    cover_cache_open(&cache, library);
    CHECK(cache.available);
    return &cache;
}

static void without_a_covers_file_every_cover_is_missing(void)
{
    fake_hal_reset();
    cover_cache_open(&cache, fixture_library_open(&fixture));
    CHECK(!cache.available);
    Image image;
    cover_cache_load_half(&cache, 0, RED_ALBUM);
    CHECK_EQUAL(COVER_MISSING, cover_cache_half(&cache, 0, RED_ALBUM, &image));
}

static void without_a_covers_file_loading_reads_nothing_from_the_library(void)
{
    fake_hal_reset();
    cover_cache_open(&cache, fixture_library_open(&fixture));
    fixture.memory.reads = 0;
    cover_cache_load_full(&cache, RED_ALBUM);
    cover_cache_load_half(&cache, 0, RED_ALBUM);
    CHECK(!cover_cache_has_cover(&cache, RED_ALBUM));
    CHECK_EQUAL(0, fixture.memory.reads);
}

static void an_album_has_a_cover_only_when_the_covers_file_holds_one(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    CHECK(cover_cache_has_cover(covers, RED_ALBUM));
    CHECK(cover_cache_has_cover(covers, BLUE_ALBUM));
    CHECK(!cover_cache_has_cover(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_EMPTY, cover_cache_full(covers, RED_ALBUM, &image));
}

static void an_unloaded_cover_is_empty(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    CHECK(covers->available);
    CHECK_EQUAL(COVER_EMPTY, cover_cache_half(covers, 0, RED_ALBUM, &image));
    CHECK_EQUAL(COVER_EMPTY, cover_cache_full(covers, RED_ALBUM, &image));
}

static void a_loaded_half_cover_has_its_pixels(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    CHECK(cover_cache_load_half(covers, 3, BLUE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_half(covers, 3, BLUE_ALBUM, &image));
    CHECK_EQUAL(COVER_HALF_SIZE, image.width);
    CHECK_EQUAL(BLUE, image.pixels[0]);
    CHECK(!cover_cache_load_half(covers, 3, BLUE_ALBUM));
}

static void a_loaded_full_cover_has_its_pixels(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    CHECK(cover_cache_load_full(covers, RED_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, RED_ALBUM, &image));
    CHECK_EQUAL(COVER_FULL_SIZE, image.height);
    CHECK_EQUAL(RED, image.pixels[COVER_FULL_PIXELS - 1]);
    CHECK_EQUAL(COVER_EMPTY, cover_cache_full(covers, BLUE_ALBUM, &image));
}

static void an_album_without_art_is_missing(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    CHECK(cover_cache_load_half(covers, 1, BARE_ALBUM));
    CHECK_EQUAL(COVER_MISSING, cover_cache_half(covers, 1, BARE_ALBUM, &image));
}

static void a_position_reuses_the_slot_of_one_a_window_away(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    cover_cache_load_half(covers, 0, RED_ALBUM);
    cover_cache_load_half(covers, COVER_CACHE_SLOTS, BLUE_ALBUM);
    CHECK_EQUAL(COVER_EMPTY, cover_cache_half(covers, 0, RED_ALBUM, &image));
    CHECK_EQUAL(COVER_LOADED, cover_cache_half(covers, COVER_CACHE_SLOTS, BLUE_ALBUM, &image));
}

static void an_album_without_art_falls_back_to_the_default_cover(void)
{
    CoverCache *covers = cache_with_default(false);
    Image image;
    CHECK(!cover_cache_has_cover(covers, BARE_ALBUM));
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK_EQUAL(GREEN, image.pixels[0]);
    CHECK(!cover_cache_load_full_or_default(covers, BARE_ALBUM));
}

static void the_default_cover_does_not_replace_album_art(void)
{
    CoverCache *covers = cache_with_default(true);
    Image image;
    CHECK(cover_cache_load_full_or_default(covers, RED_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, RED_ALBUM, &image));
    CHECK_EQUAL(RED, image.pixels[0]);
    CHECK(!cover_cache_load_full_or_default(covers, RED_ALBUM));
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK_EQUAL(GREEN, image.pixels[0]);
}

static void without_a_default_entry_a_missing_cover_stays_missing(void)
{
    CoverCache *covers = cache_with_covers();
    Image image;
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_MISSING, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK(!cover_cache_load_full_or_default(covers, BARE_ALBUM));
}

static void a_plain_load_never_shows_the_default_cover(void)
{
    CoverCache *covers = cache_with_default(false);
    Image image;
    CHECK(cover_cache_load_full(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_MISSING, cover_cache_full(covers, BARE_ALBUM, &image));
}

static void a_plain_load_clears_a_cached_default(void)
{
    CoverCache *covers = cache_with_default(false);
    Image image;
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK(cover_cache_load_full(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_MISSING, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK(!cover_cache_load_full(covers, BARE_ALBUM));
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK_EQUAL(GREEN, image.pixels[0]);
}

static void a_cached_missing_album_reloads_the_default_after_real_art(void)
{
    CoverCache *covers = cache_with_default(true);
    Image image;
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK_EQUAL(GREEN, image.pixels[0]);
    CHECK(cover_cache_load_full_or_default(covers, RED_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, RED_ALBUM, &image));
    CHECK_EQUAL(RED, image.pixels[0]);
    CHECK(cover_cache_load_full_or_default(covers, BARE_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, BARE_ALBUM, &image));
    CHECK_EQUAL(GREEN, image.pixels[0]);
}

static void real_art_loaded_with_the_default_is_not_read_again(void)
{
    CoverCache *covers = cache_with_default(true);
    Image image;
    CHECK(cover_cache_load_full_or_default(covers, RED_ALBUM));
    CHECK_EQUAL(COVER_LOADED, cover_cache_full(covers, RED_ALBUM, &image));
    CHECK_EQUAL(RED, image.pixels[0]);
    fixture.memory.reads = 0;
    CHECK(!cover_cache_load_full(covers, RED_ALBUM));
    CHECK_EQUAL(0, fixture.memory.reads);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(without_a_covers_file_every_cover_is_missing),
        TEST_CASE(without_a_covers_file_loading_reads_nothing_from_the_library),
        TEST_CASE(an_album_has_a_cover_only_when_the_covers_file_holds_one),
        TEST_CASE(an_unloaded_cover_is_empty),
        TEST_CASE(a_loaded_half_cover_has_its_pixels),
        TEST_CASE(a_loaded_full_cover_has_its_pixels),
        TEST_CASE(an_album_without_art_is_missing),
        TEST_CASE(a_position_reuses_the_slot_of_one_a_window_away),
        TEST_CASE(an_album_without_art_falls_back_to_the_default_cover),
        TEST_CASE(the_default_cover_does_not_replace_album_art),
        TEST_CASE(without_a_default_entry_a_missing_cover_stays_missing),
        TEST_CASE(a_plain_load_never_shows_the_default_cover),
        TEST_CASE(a_plain_load_clears_a_cached_default),
        TEST_CASE(a_cached_missing_album_reloads_the_default_after_real_art),
        TEST_CASE(real_art_loaded_with_the_default_is_not_read_again),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
