#include "check.h"
#include "playback/up_next.h"

static const uint32_t FINGERPRINT = 0xCAFEF00Du;

static UpNext with_tracks(const uint16_t *tracks, int count)
{
    UpNext up_next;
    up_next_clear(&up_next);
    for (int index = 0; index < count; index++) {
        CHECK(up_next_add(&up_next, tracks[index]));
    }
    return up_next;
}

static void a_new_list_is_empty(void)
{
    UpNext up_next;
    up_next_clear(&up_next);
    uint16_t track = 0;
    CHECK_EQUAL(0, up_next.count);
    CHECK(!up_next_first(&up_next, &track));
}

static void added_songs_keep_their_order(void)
{
    static const uint16_t TRACKS[] = {7, 3, 9};
    UpNext up_next = with_tracks(TRACKS, 3);
    CHECK_EQUAL(3, up_next.count);
    CHECK_EQUAL(7, up_next.tracks[0]);
    CHECK_EQUAL(3, up_next.tracks[1]);
    CHECK_EQUAL(9, up_next.tracks[2]);
}

static void play_next_goes_to_the_front(void)
{
    static const uint16_t TRACKS[] = {7, 3};
    UpNext up_next = with_tracks(TRACKS, 2);
    CHECK(up_next_play_next(&up_next, 11));
    uint16_t track = 0;
    CHECK(up_next_first(&up_next, &track));
    CHECK_EQUAL(11, track);
    CHECK_EQUAL(3, up_next.count);
    CHECK_EQUAL(7, up_next.tracks[1]);
    CHECK_EQUAL(3, up_next.tracks[2]);
}

static void a_full_list_refuses_more(void)
{
    UpNext up_next;
    up_next_clear(&up_next);
    for (int index = 0; index < UP_NEXT_CAPACITY; index++) {
        CHECK(up_next_add(&up_next, (uint16_t)index));
    }
    CHECK(!up_next_add(&up_next, 500));
    CHECK(!up_next_play_next(&up_next, 500));
    CHECK_EQUAL(UP_NEXT_CAPACITY, up_next.count);
    CHECK_EQUAL(UP_NEXT_CAPACITY - 1, up_next.tracks[UP_NEXT_CAPACITY - 1]);
}

static void dropping_removes_from_the_front(void)
{
    static const uint16_t TRACKS[] = {7, 3, 9, 4};
    UpNext up_next = with_tracks(TRACKS, 4);
    up_next_drop(&up_next, 2);
    CHECK_EQUAL(2, up_next.count);
    CHECK_EQUAL(9, up_next.tracks[0]);
    CHECK_EQUAL(4, up_next.tracks[1]);
    up_next_drop(&up_next, 5);
    CHECK_EQUAL(0, up_next.count);
}

static void a_list_survives_a_round_trip(void)
{
    static const uint16_t TRACKS[] = {7, 3, 1200};
    UpNext saved = with_tracks(TRACKS, 3);
    uint8_t bytes[UP_NEXT_STATE_SIZE];
    up_next_encode(&saved, FINGERPRINT, bytes);
    UpNext loaded;
    CHECK(up_next_decode(bytes, sizeof bytes, FINGERPRINT, &loaded));
    CHECK_EQUAL(3, loaded.count);
    CHECK_EQUAL(7, loaded.tracks[0]);
    CHECK_EQUAL(3, loaded.tracks[1]);
    CHECK_EQUAL(1200, loaded.tracks[2]);
}

static void another_library_is_rejected(void)
{
    static const uint16_t TRACKS[] = {7};
    UpNext saved = with_tracks(TRACKS, 1);
    uint8_t bytes[UP_NEXT_STATE_SIZE];
    up_next_encode(&saved, FINGERPRINT, bytes);
    UpNext loaded;
    CHECK(!up_next_decode(bytes, sizeof bytes, FINGERPRINT + 1, &loaded));
}

static void a_corrupted_or_blank_file_is_rejected(void)
{
    static const uint16_t TRACKS[] = {7, 3};
    UpNext saved = with_tracks(TRACKS, 2);
    uint8_t bytes[UP_NEXT_STATE_SIZE];
    up_next_encode(&saved, FINGERPRINT, bytes);
    bytes[16] ^= 1;
    UpNext loaded;
    CHECK(!up_next_decode(bytes, sizeof bytes, FINGERPRINT, &loaded));
    uint8_t blank[UP_NEXT_STATE_SIZE] = {0};
    CHECK(!up_next_decode(blank, sizeof blank, FINGERPRINT, &loaded));
    CHECK(!up_next_decode(bytes, UP_NEXT_STATE_SIZE - 1, FINGERPRINT, &loaded));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_list_is_empty),
        TEST_CASE(added_songs_keep_their_order),
        TEST_CASE(play_next_goes_to_the_front),
        TEST_CASE(a_full_list_refuses_more),
        TEST_CASE(dropping_removes_from_the_front),
        TEST_CASE(a_list_survives_a_round_trip),
        TEST_CASE(another_library_is_rejected),
        TEST_CASE(a_corrupted_or_blank_file_is_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
