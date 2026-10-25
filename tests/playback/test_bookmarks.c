#include "check.h"
#include "playback/bookmarks.h"

static Bookmarks bookmarks;
static uint8_t bytes[BOOKMARKS_FILE_CAPACITY];

static Bookmark mark(uint32_t key, uint32_t position_ms)
{
    return (Bookmark){.key = key, .track_key = key + 100, .position_ms = position_ms, .chapter = 2};
}

static Bookmarks *empty(void)
{
    bookmarks_clear(&bookmarks);
    return &bookmarks;
}

static void a_new_list_finds_nothing(void)
{
    Bookmark found;
    CHECK(!bookmarks_find(empty(), 1, &found));
}

static void a_put_bookmark_is_found_by_its_key(void)
{
    Bookmarks *list = empty();
    bookmarks_put(list, mark(7, 1234));
    Bookmark found;
    CHECK(bookmarks_find(list, 7, &found));
    CHECK_EQUAL(1234, found.position_ms);
    CHECK_EQUAL(107, found.track_key);
    CHECK_EQUAL(2, found.chapter);
}

static void putting_the_same_key_replaces_it(void)
{
    Bookmarks *list = empty();
    bookmarks_put(list, mark(7, 1000));
    bookmarks_put(list, mark(7, 2000));
    Bookmark found;
    CHECK_EQUAL(1, list->count);
    CHECK(bookmarks_find(list, 7, &found));
    CHECK_EQUAL(2000, found.position_ms);
}

static void the_latest_bookmark_comes_first(void)
{
    Bookmarks *list = empty();
    bookmarks_put(list, mark(1, 0));
    bookmarks_put(list, mark(2, 0));
    bookmarks_put(list, mark(1, 5));
    CHECK_EQUAL(1, list->entries[0].key);
    CHECK_EQUAL(2, list->entries[1].key);
}

static void a_full_list_forgets_the_oldest_bookmark(void)
{
    Bookmarks *list = empty();
    for (uint32_t key = 1; key <= BOOKMARKS_CAPACITY; key++) {
        bookmarks_put(list, mark(key, key));
    }
    bookmarks_put(list, mark(9999, 1));
    Bookmark found;
    CHECK_EQUAL(BOOKMARKS_CAPACITY, list->count);
    CHECK(!bookmarks_find(list, 1, &found));
    CHECK(bookmarks_find(list, 2, &found));
    CHECK(bookmarks_find(list, 9999, &found));
}

static void finished_bookmarks_say_so(void)
{
    Bookmark done = mark(3, 0);
    done.flags = BOOKMARK_FINISHED;
    CHECK(bookmark_finished(done));
    CHECK(!bookmark_finished(mark(3, 0)));
}

static void bookmarks_survive_a_round_trip(void)
{
    Bookmarks *list = empty();
    Bookmark done = mark(5, 0);
    done.flags = BOOKMARK_FINISHED;
    bookmarks_put(list, mark(4, 4321));
    bookmarks_put(list, done);
    uint32_t length = bookmarks_encode(list, bytes);
    Bookmarks decoded;
    Bookmark found;
    CHECK(bookmarks_decode(bytes, length, &decoded));
    CHECK_EQUAL(2, decoded.count);
    CHECK(bookmarks_find(&decoded, 4, &found));
    CHECK_EQUAL(4321, found.position_ms);
    CHECK(bookmarks_find(&decoded, 5, &found));
    CHECK(bookmark_finished(found));
}

static void a_full_list_fits_the_file(void)
{
    Bookmarks *list = empty();
    for (uint32_t key = 1; key <= BOOKMARKS_CAPACITY; key++) {
        bookmarks_put(list, mark(key, key));
    }
    CHECK_EQUAL(BOOKMARKS_FILE_CAPACITY, bookmarks_encode(list, bytes));
    CHECK(BOOKMARKS_FILE_CAPACITY <= 4096);
}

static void a_damaged_or_empty_file_is_rejected(void)
{
    Bookmarks *list = empty();
    bookmarks_put(list, mark(4, 4321));
    uint32_t length = bookmarks_encode(list, bytes);
    Bookmarks decoded;
    bytes[14] ^= 0xFF;
    CHECK(!bookmarks_decode(bytes, length, &decoded));
    CHECK_EQUAL(0, decoded.count);
    uint8_t zeros[64] = {0};
    CHECK(!bookmarks_decode(zeros, sizeof zeros, &decoded));
    CHECK(!bookmarks_decode(bytes, 3, &decoded));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_list_finds_nothing),
        TEST_CASE(a_put_bookmark_is_found_by_its_key),
        TEST_CASE(putting_the_same_key_replaces_it),
        TEST_CASE(the_latest_bookmark_comes_first),
        TEST_CASE(a_full_list_forgets_the_oldest_bookmark),
        TEST_CASE(finished_bookmarks_say_so),
        TEST_CASE(bookmarks_survive_a_round_trip),
        TEST_CASE(a_full_list_fits_the_file),
        TEST_CASE(a_damaged_or_empty_file_is_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
