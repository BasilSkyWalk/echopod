#include "check.h"
#include "library/favorites.h"

enum {
    HEADER_BYTES = 8
};

static Favorites favorites;

static Favorites *empty_favorites(void)
{
    favorites_clear(&favorites);
    return &favorites;
}

static void toggling_adds_then_removes(void)
{
    Favorites *list = empty_favorites();
    CHECK(favorites_toggle(list, 42));
    CHECK(favorites_contains(list, 42));
    CHECK(!favorites_toggle(list, 42));
    CHECK(!favorites_contains(list, 42));
    CHECK_EQUAL(0, list->count);
}

static void removing_keeps_the_other_favorites(void)
{
    Favorites *list = empty_favorites();
    favorites_toggle(list, 1);
    favorites_toggle(list, 2);
    favorites_toggle(list, 3);
    favorites_toggle(list, 2);
    CHECK(favorites_contains(list, 1));
    CHECK(!favorites_contains(list, 2));
    CHECK(favorites_contains(list, 3));
    CHECK_EQUAL(2, list->count);
}

static void a_full_list_refuses_new_favorites(void)
{
    Favorites *list = empty_favorites();
    for (uint32_t key = 1; key <= FAVORITES_CAPACITY; key++) {
        favorites_toggle(list, key);
    }
    CHECK(!favorites_toggle(list, 99999));
    CHECK(!favorites_contains(list, 99999));
    CHECK(!favorites_toggle(list, 1));
    CHECK(favorites_toggle(list, 99999));
}

static void favorites_survive_a_round_trip(void)
{
    Favorites *list = empty_favorites();
    favorites_toggle(list, 0xDEADBEEFu);
    favorites_toggle(list, 7);
    uint8_t bytes[FAVORITES_FILE_CAPACITY];
    uint32_t length = favorites_encode(list, bytes);
    CHECK_EQUAL(HEADER_BYTES + 2 * 4 + 4, length);
    Favorites decoded;
    CHECK(favorites_decode(bytes, length, &decoded));
    CHECK_EQUAL(2, decoded.count);
    CHECK(favorites_contains(&decoded, 0xDEADBEEFu));
    CHECK(favorites_contains(&decoded, 7));
}

static void an_empty_list_survives_a_round_trip(void)
{
    uint8_t bytes[FAVORITES_FILE_CAPACITY];
    uint32_t length = favorites_encode(empty_favorites(), bytes);
    Favorites decoded;
    CHECK(favorites_decode(bytes, length, &decoded));
    CHECK_EQUAL(0, decoded.count);
}

static void a_corrupted_file_is_rejected(void)
{
    Favorites *list = empty_favorites();
    favorites_toggle(list, 7);
    uint8_t bytes[FAVORITES_FILE_CAPACITY];
    uint32_t length = favorites_encode(list, bytes);
    bytes[HEADER_BYTES] ^= 0x01;
    Favorites decoded;
    CHECK(!favorites_decode(bytes, length, &decoded));
    CHECK(!favorites_decode(bytes, 3, &decoded));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(toggling_adds_then_removes),
        TEST_CASE(removing_keeps_the_other_favorites),
        TEST_CASE(a_full_list_refuses_new_favorites),
        TEST_CASE(favorites_survive_a_round_trip),
        TEST_CASE(an_empty_list_survives_a_round_trip),
        TEST_CASE(a_corrupted_file_is_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
