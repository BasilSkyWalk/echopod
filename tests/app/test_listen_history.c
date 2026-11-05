#include <string.h>

#include "check.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "hal/storage.h"
#include "listen_history.h"
#include "player.h"
#include "spoken_library.h"

enum {
    BACK_IN_BLACK = 0,
    HELLS_BELLS_MS = 60000,
    SEED = 7,
    SMALL_RING_SLOTS = 4,
    RING_SLOTS = 16,
    SECOND_MS = 1000
};

static FixtureLibrary fixture;
static SpokenLibrary spoken;
static Library *library;
static Player player;
static ListenHistory history;
static uint8_t ring[RING_SLOTS * HISTORY_SLOT_BYTES];
static uint32_t now_ms;

static void start_with_ring(uint32_t slots)
{
    fake_hal_reset();
    memset(ring, 0, sizeof ring);
    fake_hal_add_file(LISTEN_HISTORY_PATH, ring, slots * HISTORY_SLOT_BYTES);
    history = (ListenHistory){.open = false};
    library = fixture_library_open(&fixture);
    player_init(&player, library);
    now_ms = 5000;
}

static void start(void)
{
    start_with_ring(RING_SLOTS);
}

static void follow(void)
{
    PlayerStatus status = player_status(&player);
    listen_history_follow(&history, library, &status, now_ms);
}

static void wait(uint32_t milliseconds)
{
    now_ms += milliseconds;
    fake_hal_audio_set_position(player_status(&player).position_ms + milliseconds);
    player_update(&player, now_ms);
    follow();
}

static void play_album(uint32_t position)
{
    BrowseQuery album = {.kind = BROWSE_ALBUM_TRACKS,
                         .group = LIBRARY_ARTISTS,
                         .subject = BACK_IN_BLACK,
                         .artist = LIBRARY_NONE};
    CHECK(player_play_list(&player, album, position, SEED));
    follow();
}

static const uint8_t *slot_bytes(uint32_t slot)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(LISTEN_HISTORY_PATH, &size);
    CHECK(bytes != NULL && size >= (slot + 1) * HISTORY_SLOT_BYTES);
    return bytes + slot * HISTORY_SLOT_BYTES;
}

static HistoryRecord record_at(uint32_t slot)
{
    HistoryRecord record;
    CHECK(listen_history_decode(slot_bytes(slot), &record));
    return record;
}

static bool slot_empty(uint32_t slot)
{
    HistoryRecord record;
    return !listen_history_decode(slot_bytes(slot), &record);
}

static const char *text_of(uint32_t slot, int field)
{
    const char *text = (const char *)slot_bytes(slot) + HISTORY_TEXT_AT;
    for (int skipped = 0; skipped < field; skipped++) {
        text += strlen(text) + 1;
    }
    return text;
}

static void a_record_survives_encoding(void)
{
    uint8_t slot[HISTORY_SLOT_BYTES] = {0};
    HistoryRecord record = {.sequence = 9,
                            .session = 3,
                            .kind = HISTORY_PLAY,
                            .number = 4,
                            .start_ms = 1000,
                            .end_ms = 250000,
                            .unix_time = 0,
                            .played_ms = 200000,
                            .duration_ms = 240000,
                            .start_position_ms = 12,
                            .end_position_ms = 239000};
    listen_history_encode(&record, slot);
    CHECK(memcmp(slot, "EHST", 4) == 0);
    CHECK_EQUAL(9, slot[4]);
    CHECK_EQUAL(3, slot[8]);
    CHECK_EQUAL(HISTORY_PLAY, slot[12]);
    CHECK_EQUAL(4, slot[13]);
    HistoryRecord decoded;
    CHECK(listen_history_decode(slot, &decoded));
    CHECK(memcmp(&record, &decoded, sizeof record) == 0);
}

static void a_damaged_record_is_refused(void)
{
    uint8_t slot[HISTORY_SLOT_BYTES] = {0};
    HistoryRecord record = {.sequence = 1, .session = 1, .kind = HISTORY_PLAY};
    HistoryRecord decoded;
    CHECK(!listen_history_decode(slot, &decoded));
    listen_history_encode(&record, slot);
    slot[HISTORY_TEXT_AT + 10] ^= 1;
    CHECK(!listen_history_decode(slot, &decoded));
    slot[HISTORY_TEXT_AT + 10] ^= 1;
    CHECK(listen_history_decode(slot, &decoded));
    slot[0] = 'X';
    CHECK(!listen_history_decode(slot, &decoded));
}

static void a_played_song_is_recorded_with_its_names(void)
{
    start();
    play_album(0);
    wait(40 * SECOND_MS);
    listen_history_close(&history, now_ms);
    HistoryRecord record = record_at(0);
    CHECK_EQUAL(1, record.sequence);
    CHECK_EQUAL(1, record.session);
    CHECK_EQUAL(HISTORY_PLAY, record.kind);
    CHECK_EQUAL(1, record.number);
    CHECK_EQUAL(5000, record.start_ms);
    CHECK_EQUAL(45000, record.end_ms);
    CHECK_EQUAL(0, record.unix_time);
    CHECK_EQUAL(40 * SECOND_MS, record.played_ms);
    CHECK_EQUAL(HELLS_BELLS_MS, record.duration_ms);
    CHECK_EQUAL(0, record.start_position_ms);
    CHECK_EQUAL(40 * SECOND_MS, record.end_position_ms);
    CHECK(strcmp(text_of(0, 0), "AC/DC") == 0);
    CHECK(strcmp(text_of(0, 1), "Back In Black") == 0);
    CHECK(strcmp(text_of(0, 2), "Hells Bells") == 0);
    CHECK(slot_empty(1));
}

static void paused_time_does_not_count(void)
{
    start();
    play_album(0);
    wait(10 * SECOND_MS);
    player_toggle_pause(&player);
    follow();
    now_ms += 100 * SECOND_MS;
    follow();
    player_toggle_pause(&player);
    follow();
    wait(15 * SECOND_MS);
    listen_history_close(&history, now_ms);
    HistoryRecord record = record_at(0);
    CHECK_EQUAL(25 * SECOND_MS, record.played_ms);
    CHECK_EQUAL(125 * SECOND_MS, record.end_ms - record.start_ms);
}

static void skipping_ends_the_play_and_starts_the_next(void)
{
    start();
    play_album(0);
    wait(5 * SECOND_MS);
    player_skip(&player, 1);
    follow();
    wait(30 * SECOND_MS);
    HistoryRecord skipped = record_at(0);
    CHECK_EQUAL(5 * SECOND_MS, skipped.played_ms);
    CHECK(strcmp(text_of(0, 2), "Hells Bells") == 0);
    CHECK(slot_empty(1));
    listen_history_close(&history, now_ms);
    HistoryRecord next = record_at(1);
    CHECK_EQUAL(2, next.sequence);
    CHECK_EQUAL(30 * SECOND_MS, next.played_ms);
    CHECK(strcmp(text_of(1, 2), "Shoot to Thrill") == 0);
}

static void a_track_that_ends_hands_over_to_the_next(void)
{
    start();
    play_album(0);
    wait(59 * SECOND_MS);
    fake_hal_audio_finish_track();
    now_ms += SECOND_MS;
    player_update(&player, now_ms);
    follow();
    HistoryRecord record = record_at(0);
    CHECK_EQUAL(60 * SECOND_MS, record.played_ms);
    CHECK_EQUAL(59 * SECOND_MS, record.end_position_ms);
    CHECK(history.open);
}

static void repeat_one_records_each_time_through(void)
{
    start();
    player_set_repeat(&player, REPEAT_ONE);
    play_album(0);
    wait(59 * SECOND_MS);
    fake_hal_audio_finish_track();
    now_ms += SECOND_MS;
    player_update(&player, now_ms);
    follow();
    CHECK_EQUAL(1, record_at(0).sequence);
    CHECK(strcmp(text_of(0, 2), "Hells Bells") == 0);
    wait(50 * SECOND_MS);
    listen_history_close(&history, now_ms);
    HistoryRecord again = record_at(1);
    CHECK_EQUAL(50 * SECOND_MS, again.played_ms);
    CHECK(strcmp(text_of(1, 2), "Hells Bells") == 0);
}

static void previous_after_a_few_seconds_starts_a_new_play(void)
{
    start();
    play_album(0);
    wait(20 * SECOND_MS);
    player_skip(&player, -1);
    follow();
    CHECK_EQUAL(20 * SECOND_MS, record_at(0).played_ms);
    CHECK(history.open);
}

static void seeking_ahead_counts_only_time_listened(void)
{
    start();
    play_album(0);
    wait(5 * SECOND_MS);
    player_seek(&player, 50 * SECOND_MS);
    now_ms += 2 * SECOND_MS;
    follow();
    listen_history_close(&history, now_ms);
    CHECK_EQUAL(7 * SECOND_MS, record_at(0).played_ms);
    CHECK_EQUAL(50 * SECOND_MS, record_at(0).end_position_ms);
}

static void a_play_under_a_second_is_not_recorded(void)
{
    start();
    play_album(0);
    now_ms += 500;
    player_skip(&player, 1);
    follow();
    CHECK(slot_empty(0));
}

static void releasing_the_output_records_the_play_and_a_computer_mark(void)
{
    start();
    play_album(0);
    wait(20 * SECOND_MS);
    listen_history_note_computer(&history, now_ms);
    player_release_output(&player);
    follow();
    CHECK_EQUAL(20 * SECOND_MS, record_at(0).played_ms);
    CHECK_EQUAL(20 * SECOND_MS, record_at(0).end_position_ms);
    HistoryRecord mark = record_at(1);
    CHECK_EQUAL(HISTORY_COMPUTER, mark.kind);
    CHECK_EQUAL(2, mark.sequence);
    CHECK_EQUAL(now_ms, mark.end_ms);
    CHECK_EQUAL(0, text_of(1, 0)[0]);
    now_ms += 60 * SECOND_MS;
    follow();
    CHECK(slot_empty(2));
    player_toggle_pause(&player);
    follow();
    wait(10 * SECOND_MS);
    listen_history_close(&history, now_ms);
    HistoryRecord resumed = record_at(2);
    CHECK_EQUAL(20 * SECOND_MS, resumed.start_position_ms);
    CHECK_EQUAL(10 * SECOND_MS, resumed.played_ms);
}

static void closing_twice_records_once(void)
{
    start();
    play_album(0);
    wait(20 * SECOND_MS);
    listen_history_close(&history, now_ms);
    listen_history_close(&history, now_ms + SECOND_MS);
    CHECK(!slot_empty(0));
    CHECK(slot_empty(1));
}

static void audiobooks_and_podcasts_are_left_out(void)
{
    start();
    library = spoken_library_open(&spoken);
    player_init(&player, library);
    BrowseQuery folder = browse_root_query(BROWSE_FOLDER);
    LibraryTrack record;
    uint16_t chapter = spoken_library_track(library, SPOKEN_CHAPTER_ONE_PATH);
    CHECK(library_read_track(library, chapter, &record));
    CHECK(browse_child(folder, (BrowseItem){.kind = BROWSE_ITEM_FOLDER, .index = record.folder},
                       &folder));
    CHECK(player_play_list(&player, folder, 0, SEED));
    CHECK(player_status(&player).spoken);
    follow();
    wait(400 * SECOND_MS);
    listen_history_close(&history, now_ms);
    CHECK(slot_empty(0));
    spoken_library_close(&spoken);
}

static void store_three_plays(void)
{
    for (int play = 0; play < 3; play++) {
        play_album(0);
        wait(10 * SECOND_MS);
        listen_history_close(&history, now_ms);
    }
}

static void a_new_session_continues_the_numbering(void)
{
    start();
    store_three_plays();
    history = (ListenHistory){.open = false};
    store_three_plays();
    CHECK_EQUAL(4, record_at(3).sequence);
    CHECK_EQUAL(2, record_at(3).session);
    CHECK_EQUAL(1, record_at(2).session);
    CHECK_EQUAL(6, record_at(5).sequence);
}

static void the_ring_wraps_and_keeps_the_newest(void)
{
    start_with_ring(SMALL_RING_SLOTS);
    store_three_plays();
    store_three_plays();
    CHECK_EQUAL(5, record_at(0).sequence);
    CHECK_EQUAL(6, record_at(1).sequence);
    CHECK_EQUAL(3, record_at(2).sequence);
    history = (ListenHistory){.open = false};
    store_three_plays();
    CHECK_EQUAL(7, record_at(2).sequence);
    CHECK_EQUAL(2, record_at(2).session);
    CHECK_EQUAL(9, record_at(0).sequence);
}

static void damage(uint32_t slot)
{
    uint8_t byte = slot_bytes(slot)[HISTORY_TEXT_AT] ^ 0x55;
    CHECK(hal_file_write_at(LISTEN_HISTORY_PATH, slot * HISTORY_SLOT_BYTES + HISTORY_TEXT_AT, &byte,
                            1));
}

static void a_torn_newest_record_is_written_over(void)
{
    start();
    store_three_plays();
    damage(2);
    history = (ListenHistory){.open = false};
    store_three_plays();
    CHECK_EQUAL(3, record_at(2).sequence);
    CHECK_EQUAL(2, record_at(2).session);
    CHECK_EQUAL(5, record_at(4).sequence);
}

static void a_torn_first_slot_after_a_wrap_continues_from_the_last(void)
{
    start_with_ring(SMALL_RING_SLOTS);
    store_three_plays();
    play_album(0);
    wait(10 * SECOND_MS);
    listen_history_close(&history, now_ms);
    store_three_plays();
    damage(0);
    damage(1);
    damage(2);
    history = (ListenHistory){.open = false};
    store_three_plays();
    CHECK_EQUAL(5, record_at(0).sequence);
    CHECK_EQUAL(2, record_at(0).session);
    CHECK_EQUAL(4, record_at(3).sequence);
}

static void stale_records_from_another_ring_are_ignored(void)
{
    start();
    HistoryRecord stale = {.sequence = 40, .session = 9, .kind = HISTORY_PLAY};
    listen_history_encode(&stale, ring + 1 * HISTORY_SLOT_BYTES);
    store_three_plays();
    CHECK_EQUAL(1, record_at(0).sequence);
    CHECK_EQUAL(2, record_at(1).sequence);
    CHECK_EQUAL(1, record_at(1).session);
}

static void a_missing_file_records_nothing(void)
{
    fake_hal_reset();
    history = (ListenHistory){.open = false};
    library = fixture_library_open(&fixture);
    player_init(&player, library);
    play_album(0);
    wait(30 * SECOND_MS);
    listen_history_close(&history, now_ms);
    CHECK(!history.open);
    CHECK(!history.located);
}

static void a_known_clock_with_a_zone_dates_each_record(void)
{
    WallClock clock = {0};
    start();
    history.clock = &clock;
    play_album(0);
    wait(40 * SECOND_MS);
    listen_history_close(&history, now_ms);
    CHECK_EQUAL(0, record_at(0).unix_time);
    wall_clock_set(&clock, 1793880000u, now_ms);
    play_album(1);
    wait(40 * SECOND_MS);
    listen_history_close(&history, now_ms);
    CHECK_EQUAL(0, record_at(1).unix_time);
    clock.zone = wall_clock_zone_of(60);
    play_album(0);
    wait(40 * SECOND_MS);
    listen_history_note_computer(&history, now_ms);
    CHECK_EQUAL(1793880000u + 80 - 3600, record_at(2).unix_time);
    CHECK_EQUAL(1793880000u + 80 - 3600, record_at(3).unix_time);
    CHECK_EQUAL(HISTORY_COMPUTER, record_at(3).kind);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_record_survives_encoding),
        TEST_CASE(a_known_clock_with_a_zone_dates_each_record),
        TEST_CASE(a_damaged_record_is_refused),
        TEST_CASE(a_played_song_is_recorded_with_its_names),
        TEST_CASE(paused_time_does_not_count),
        TEST_CASE(skipping_ends_the_play_and_starts_the_next),
        TEST_CASE(a_track_that_ends_hands_over_to_the_next),
        TEST_CASE(repeat_one_records_each_time_through),
        TEST_CASE(previous_after_a_few_seconds_starts_a_new_play),
        TEST_CASE(seeking_ahead_counts_only_time_listened),
        TEST_CASE(a_play_under_a_second_is_not_recorded),
        TEST_CASE(releasing_the_output_records_the_play_and_a_computer_mark),
        TEST_CASE(closing_twice_records_once),
        TEST_CASE(audiobooks_and_podcasts_are_left_out),
        TEST_CASE(a_new_session_continues_the_numbering),
        TEST_CASE(the_ring_wraps_and_keeps_the_newest),
        TEST_CASE(a_torn_newest_record_is_written_over),
        TEST_CASE(a_torn_first_slot_after_a_wrap_continues_from_the_last),
        TEST_CASE(stale_records_from_another_ring_are_ignored),
        TEST_CASE(a_missing_file_records_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
