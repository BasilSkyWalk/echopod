#include <stdbool.h>

#include "check.h"
#include "fixture_library.h"
#include "playback/play_queue.h"

enum {
    HELLS_BELLS = 0,
    SHOOT_TO_THRILL = 1,
    HELP = 2,
    ARMY_OF_ME = 3,
    HUNTER = 5,
    JOGA = 6,
    SHOOTS = 7,
    BACK_IN_BLACK = 0,
    HOMOGENIC = 3,
    BACK_IN_BLACK_FOLDER = 2,
    ALBUM_COUNT = 5
};

static FixtureLibrary fixture;
static PlayQueue queue;

static BrowseQuery album_tracks(uint16_t album)
{
    return (BrowseQuery){.kind = BROWSE_ALBUM_TRACKS,
                         .group = LIBRARY_ARTISTS,
                         .subject = album,
                         .artist = LIBRARY_NONE};
}

static PlayQueue *built(BrowseQuery source, uint16_t anchor, ShuffleMode shuffle, uint32_t seed)
{
    Library *library = fixture_library_open(&fixture);
    PlayQueuePlan plan = {
        .source = source, .anchor_track = anchor, .shuffle = shuffle, .seed = seed};
    CHECK(play_queue_build(&queue, library, plan));
    return &queue;
}

static int current_track(const PlayQueue *built_queue)
{
    uint16_t track = 0;
    return play_queue_current(built_queue, &track) ? track : -1;
}

static int track_after(const PlayQueue *built_queue, RepeatMode repeat)
{
    PlayQueueCursor next;
    uint16_t track = 0;
    if (!play_queue_upcoming(built_queue, repeat, &next) ||
        !play_queue_track_at(built_queue, next, &track)) {
        return -1;
    }
    return track;
}

static void advance(PlayQueue *built_queue)
{
    PlayQueueCursor next;
    CHECK(play_queue_upcoming(built_queue, REPEAT_OFF, &next));
    play_queue_move_to(built_queue, next);
}

static bool holds_each_value_once(const PlayQueue *built_queue, int values)
{
    int seen[PLAY_QUEUE_CAPACITY] = {0};
    for (int index = 0; index < built_queue->count; index++) {
        seen[built_queue->entries[index]]++;
    }
    for (int value = 0; value < values; value++) {
        if (seen[value] != 1) {
            return false;
        }
    }
    return built_queue->count == values;
}

static void a_list_queue_starts_at_the_anchor(void)
{
    PlayQueue *list = built(album_tracks(BACK_IN_BLACK), SHOOT_TO_THRILL, SHUFFLE_OFF, 1);
    CHECK_EQUAL(2, list->count);
    CHECK_EQUAL(SHOOT_TO_THRILL, current_track(list));
}

static void all_songs_play_in_title_order(void)
{
    PlayQueue *songs = built(browse_root_query(BROWSE_ALL_SONGS), ARMY_OF_ME, SHUFFLE_OFF, 1);
    CHECK_EQUAL(ARMY_OF_ME, current_track(songs));
    CHECK_EQUAL(HELLS_BELLS, track_after(songs, REPEAT_OFF));
}

static void folders_queue_only_their_tracks(void)
{
    BrowseQuery folder = {.kind = BROWSE_FOLDER,
                          .group = LIBRARY_ARTISTS,
                          .subject = BACK_IN_BLACK_FOLDER,
                          .artist = LIBRARY_NONE};
    PlayQueue *tracks = built(folder, HELLS_BELLS, SHUFFLE_OFF, 1);
    CHECK_EQUAL(2, tracks->count);
    CHECK_EQUAL(SHOOT_TO_THRILL, track_after(tracks, REPEAT_OFF));
}

static void an_anchor_outside_the_list_plays_first(void)
{
    PlayQueue *list = built(album_tracks(BACK_IN_BLACK), SHOOTS, SHUFFLE_OFF, 1);
    CHECK_EQUAL(3, list->count);
    CHECK_EQUAL(SHOOTS, current_track(list));
    CHECK_EQUAL(HELLS_BELLS, track_after(list, REPEAT_OFF));
}

static void the_end_of_the_list_stops_without_repeat(void)
{
    PlayQueue *list = built(album_tracks(BACK_IN_BLACK), SHOOT_TO_THRILL, SHUFFLE_OFF, 1);
    CHECK_EQUAL(-1, track_after(list, REPEAT_OFF));
}

static void repeat_all_wraps_and_repeat_one_stays(void)
{
    PlayQueue *list = built(album_tracks(BACK_IN_BLACK), SHOOT_TO_THRILL, SHUFFLE_OFF, 1);
    CHECK_EQUAL(HELLS_BELLS, track_after(list, REPEAT_ALL));
    CHECK_EQUAL(SHOOT_TO_THRILL, track_after(list, REPEAT_ONE));
}

static void previous_wraps_only_when_asked(void)
{
    PlayQueue *list = built(album_tracks(BACK_IN_BLACK), HELLS_BELLS, SHUFFLE_OFF, 1);
    PlayQueueCursor previous;
    uint16_t track = 0;
    CHECK(!play_queue_neighbor(list, -1, false, &previous));
    CHECK(play_queue_neighbor(list, -1, true, &previous));
    CHECK(play_queue_track_at(list, previous, &track));
    CHECK_EQUAL(SHOOT_TO_THRILL, track);
}

static void shuffled_songs_start_with_the_anchor_and_keep_every_track(void)
{
    PlayQueue *songs = built(browse_root_query(BROWSE_ALL_SONGS), HUNTER, SHUFFLE_SONGS, 11);
    CHECK_EQUAL(HUNTER, current_track(songs));
    CHECK_EQUAL(0, songs->cursor.position);
    CHECK(holds_each_value_once(songs, FIXTURE_TRACKS));
}

static void the_same_seed_rebuilds_the_same_order(void)
{
    uint16_t first[FIXTURE_TRACKS];
    PlayQueue *songs = built(browse_root_query(BROWSE_ALL_SONGS), HUNTER, SHUFFLE_SONGS, 11);
    for (int index = 0; index < FIXTURE_TRACKS; index++) {
        first[index] = songs->entries[index];
    }
    songs = built(browse_root_query(BROWSE_ALL_SONGS), HUNTER, SHUFFLE_SONGS, 11);
    for (int index = 0; index < FIXTURE_TRACKS; index++) {
        CHECK_EQUAL(first[index], songs->entries[index]);
    }
}

static void different_seeds_shuffle_differently(void)
{
    uint16_t first[FIXTURE_TRACKS];
    PlayQueue *songs = built(browse_root_query(BROWSE_ALL_SONGS), HUNTER, SHUFFLE_SONGS, 11);
    for (int index = 0; index < FIXTURE_TRACKS; index++) {
        first[index] = songs->entries[index];
    }
    songs = built(browse_root_query(BROWSE_ALL_SONGS), HUNTER, SHUFFLE_SONGS, 12345);
    int differences = 0;
    for (int index = 0; index < FIXTURE_TRACKS; index++) {
        differences += first[index] != songs->entries[index] ? 1 : 0;
    }
    CHECK(differences > 0);
}

static void album_shuffle_starts_inside_the_anchor_album(void)
{
    PlayQueue *albums = built(album_tracks(BACK_IN_BLACK), JOGA, SHUFFLE_ALBUMS, 3);
    CHECK_EQUAL(JOGA, current_track(albums));
    CHECK_EQUAL(HOMOGENIC, albums->entries[0]);
    CHECK(holds_each_value_once(albums, ALBUM_COUNT));
}

static void album_shuffle_finishes_an_album_before_the_next(void)
{
    PlayQueue *albums = built(album_tracks(BACK_IN_BLACK), HUNTER, SHUFFLE_ALBUMS, 3);
    CHECK_EQUAL(JOGA, track_after(albums, REPEAT_OFF));
    advance(albums);
    uint16_t next_album = albums->entries[1];
    LibraryAlbum album;
    library_read_album(albums->library, next_album, &album);
    CHECK_EQUAL(album.first_track, track_after(albums, REPEAT_OFF));
}

static void album_shuffle_previous_returns_to_the_last_track_of_the_album_before(void)
{
    PlayQueue *albums = built(album_tracks(BACK_IN_BLACK), HUNTER, SHUFFLE_ALBUMS, 3);
    advance(albums);
    advance(albums);
    PlayQueueCursor previous;
    uint16_t track = 0;
    CHECK(play_queue_neighbor(albums, -1, false, &previous));
    CHECK(play_queue_track_at(albums, previous, &track));
    CHECK_EQUAL(JOGA, track);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_list_queue_starts_at_the_anchor),
        TEST_CASE(all_songs_play_in_title_order),
        TEST_CASE(folders_queue_only_their_tracks),
        TEST_CASE(an_anchor_outside_the_list_plays_first),
        TEST_CASE(the_end_of_the_list_stops_without_repeat),
        TEST_CASE(repeat_all_wraps_and_repeat_one_stays),
        TEST_CASE(previous_wraps_only_when_asked),
        TEST_CASE(shuffled_songs_start_with_the_anchor_and_keep_every_track),
        TEST_CASE(the_same_seed_rebuilds_the_same_order),
        TEST_CASE(different_seeds_shuffle_differently),
        TEST_CASE(album_shuffle_starts_inside_the_anchor_album),
        TEST_CASE(album_shuffle_finishes_an_album_before_the_next),
        TEST_CASE(album_shuffle_previous_returns_to_the_last_track_of_the_album_before),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
