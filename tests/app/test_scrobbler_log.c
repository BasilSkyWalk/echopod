#include <stdio.h>
#include <string.h>

#include "check.h"
#include "fake_hal.h"
#include "listen_history.h"
#include "scrobbler_log.h"

enum {
    FIXTURE_SLOTS = 16,
    BIG_SLOTS = 64,
    FIXTURE_NEWEST = 11,
    FILE_LIMIT = 8192,
    MARK_TEXT_BYTES = SCROBBLER_MARK_BYTES + 1,
    THIRD_BOOT_SECONDS = 1790100000u,
    MANY_PLAYS = 30
};

static uint8_t ring[BIG_SLOTS * HISTORY_SLOT_BYTES];
static uint8_t marker[SCROBBLER_MARK_BYTES];
static char existing[FILE_LIMIT];
static char expected[FILE_LIMIT];
static char single_run[FILE_LIMIT];
static ListenHistory history;
static WallClock clock;

static uint32_t read_fixture(const char *name, void *buffer, uint32_t capacity)
{
    char path[512];
    snprintf(path, sizeof path, "%s/history/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    size_t length = fread(buffer, 1, capacity, file);
    fclose(file);
    return (uint32_t)length;
}

static void start_ring(uint32_t slots, uint32_t newest)
{
    fake_hal_reset();
    fake_hal_set_reserve_result(HAL_RESERVE_CREATED);
    fake_hal_add_file(LISTEN_HISTORY_PATH, ring, slots * HISTORY_SLOT_BYTES);
    history =
        (ListenHistory){.located = true, .slots = slots, .sequence = newest + 1, .session = 3};
}

static void start(void)
{
    memset(ring, 0, sizeof ring);
    CHECK_EQUAL(FIXTURE_SLOTS * HISTORY_SLOT_BYTES,
                read_fixture("history.bin", ring, FIXTURE_SLOTS * HISTORY_SLOT_BYTES));
    start_ring(FIXTURE_SLOTS, FIXTURE_NEWEST);
}

static void add_marker(const char *text)
{
    memcpy(marker, text, SCROBBLER_MARK_BYTES);
    fake_hal_add_file(SCROBBLER_MARK_PATH, marker, SCROBBLER_MARK_BYTES);
}

static uint32_t add_log(const char *fixture)
{
    uint32_t length = read_fixture(fixture, existing, sizeof existing);
    fake_hal_add_file(SCROBBLER_LOG_PATH, (const uint8_t *)existing, length);
    return length;
}

static bool file_is(const char *path, const char *text, uint32_t length)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(path, &size);
    return bytes != NULL && size == length && memcmp(bytes, text, length) == 0;
}

static bool file_is_fixture(const char *path, const char *fixture)
{
    uint32_t length = read_fixture(fixture, expected, sizeof expected);
    return file_is(path, expected, length);
}

static bool mark_is(const char *text)
{
    return file_is(SCROBBLER_MARK_PATH, text, SCROBBLER_MARK_BYTES);
}

static bool missing(const char *path)
{
    uint32_t size = 0;
    return fake_hal_file_bytes(path, &size) == NULL && size == 0;
}

static void put_record(uint32_t sequence, uint32_t session, uint8_t kind, uint32_t start_ms,
                       uint32_t played_ms, uint32_t unix_time, const char *title)
{
    uint8_t *slot = ring + (sequence - 1) % (history.slots) * HISTORY_SLOT_BYTES;
    memset(slot, 0, HISTORY_SLOT_BYTES);
    if (kind == HISTORY_PLAY) {
        static const char NAMES[] = "Artist\0Album";
        memcpy(slot + HISTORY_TEXT_AT, NAMES, sizeof NAMES);
        strcpy((char *)slot + HISTORY_TEXT_AT + sizeof NAMES, title);
    }
    HistoryRecord record = {.sequence = sequence,
                            .session = session,
                            .kind = kind,
                            .number = kind == HISTORY_PLAY ? 5 : 0,
                            .start_ms = start_ms,
                            .end_ms = start_ms + played_ms,
                            .unix_time = unix_time,
                            .played_ms = played_ms,
                            .duration_ms = kind == HISTORY_PLAY ? 200000 : 0,
                            .start_position_ms = 0,
                            .end_position_ms = played_ms};
    listen_history_encode(&record, slot);
}

static void the_player_writes_the_log_the_pc_tool_writes(void)
{
    start();
    CHECK_EQUAL(6, scrobbler_log_write(&history, "test"));
    CHECK(file_is_fixture(SCROBBLER_LOG_PATH, "all.log"));
    CHECK(mark_is("0000000011\n"));
}

static void a_play_resumed_after_the_last_export_is_added_like_the_pc_tool_adds_it(void)
{
    start();
    add_marker("0000000008\n");
    add_log("existing.log");
    CHECK_EQUAL(2, scrobbler_log_write(&history, "test"));
    CHECK(file_is_fixture(SCROBBLER_LOG_PATH, "appended.log"));
    CHECK(mark_is("0000000011\n"));
}

static void an_empty_log_gets_the_header(void)
{
    start();
    fake_hal_add_file(SCROBBLER_LOG_PATH, NULL, 0);
    CHECK_EQUAL(6, scrobbler_log_write(&history, "test"));
    CHECK(file_is_fixture(SCROBBLER_LOG_PATH, "all.log"));
}

static void connecting_again_without_new_plays_changes_nothing(void)
{
    start();
    CHECK_EQUAL(6, scrobbler_log_write(&history, "test"));
    int grows = fake_hal_grow_count();
    int reserves = fake_hal_reserve_count();
    clock = (WallClock){.known = false};
    wall_clock_set(&clock, THIRD_BOOT_SECONDS + 300, 300000);
    clock.zone = WALL_CLOCK_ZONE_UTC;
    history.clock = &clock;
    listen_history_note_computer(&history, 300000);
    CHECK_EQUAL(FIXTURE_NEWEST + 2, history.sequence);
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    CHECK(file_is_fixture(SCROBBLER_LOG_PATH, "all.log"));
    CHECK(mark_is("0000000011\n"));
    CHECK_EQUAL(grows, fake_hal_grow_count());
    CHECK_EQUAL(reserves, fake_hal_reserve_count());
}

static void a_deleted_log_comes_back_with_only_the_new_plays(void)
{
    start();
    add_marker("0000000011\n");
    put_record(12, 4, HISTORY_PLAY, 10000, 150000, 1790200160u, "New Song");
    put_record(13, 4, HISTORY_COMPUTER, 170000, 0, 1790200170u, "");
    history.sequence = 14;
    CHECK_EQUAL(1, scrobbler_log_write(&history, "0.11.0"));
    static const char LOG[] = "#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/EchoPod 0.11.0\n"
                              "Artist\tAlbum\tNew Song\t5\t200\tL\t1790200010\t\n";
    CHECK(file_is(SCROBBLER_LOG_PATH, LOG, sizeof LOG - 1));
    CHECK(mark_is("0000000013\n"));
}

static void plays_without_a_clock_wait_until_a_later_session_has_one(void)
{
    start();
    add_marker("0000000011\n");
    put_record(12, 4, HISTORY_PLAY, 10000, 150000, 0, "Undated");
    put_record(13, 4, HISTORY_COMPUTER, 200000, 0, 0, "");
    history.sequence = 14;
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    CHECK(missing(SCROBBLER_LOG_PATH));
    CHECK(mark_is("0000000011\n"));
    put_record(14, 5, HISTORY_PLAY, 5000, 90000, 1790300095u, "Dated");
    put_record(15, 5, HISTORY_COMPUTER, 110000, 0, 1790300110u, "");
    history.sequence = 16;
    CHECK_EQUAL(2, scrobbler_log_write(&history, "test"));
    static const char LOG[] = "#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/EchoPod test\n"
                              "Artist\tAlbum\tUndated\t5\t200\tL\t1790299810\t\n"
                              "Artist\tAlbum\tDated\t5\t200\tS\t1790300005\t\n";
    CHECK(file_is(SCROBBLER_LOG_PATH, LOG, sizeof LOG - 1));
    CHECK(mark_is("0000000015\n"));
}

static void a_full_card_marks_nothing(void)
{
    start();
    fake_hal_set_reserve_result(HAL_RESERVE_NO_SPACE);
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    CHECK(missing(SCROBBLER_LOG_PATH));
    CHECK(missing(SCROBBLER_MARK_PATH));
    start();
    add_marker("0000000008\n");
    uint32_t length = add_log("existing.log");
    fake_hal_set_grow_limit(length + 10);
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    CHECK(file_is(SCROBBLER_LOG_PATH, existing, length));
    CHECK(mark_is("0000000008\n"));
    fake_hal_set_grow_limit(0);
    CHECK_EQUAL(2, scrobbler_log_write(&history, "test"));
    CHECK(file_is_fixture(SCROBBLER_LOG_PATH, "appended.log"));
}

static void put_many_plays(void)
{
    memset(ring, 0, sizeof ring);
    history.slots = BIG_SLOTS;
    for (uint32_t play = 1; play <= MANY_PLAYS; play++) {
        char title[16];
        snprintf(title, sizeof title, "Song %u", (unsigned)play);
        put_record(play, 1, HISTORY_PLAY, play * 300000, 150000, 0, title);
    }
    put_record(MANY_PLAYS + 1, 1, HISTORY_COMPUTER, 10000000, 0, 1790400000u, "");
}

static void a_log_cut_short_by_a_full_card_is_finished_later_without_repeats(void)
{
    put_many_plays();
    start_ring(BIG_SLOTS, MANY_PLAYS + 1);
    CHECK_EQUAL(MANY_PLAYS, scrobbler_log_write(&history, "test"));
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_file_bytes(SCROBBLER_LOG_PATH, &size);
    CHECK(bytes != NULL && size < sizeof single_run);
    memcpy(single_run, bytes, size);
    CHECK(fake_hal_grow_count() >= 2);
    start_ring(BIG_SLOTS, MANY_PLAYS + 1);
    fake_hal_set_grow_limit(1);
    uint32_t first = scrobbler_log_write(&history, "test");
    CHECK(first > 0 && first < MANY_PLAYS);
    char mark[MARK_TEXT_BYTES];
    snprintf(mark, sizeof mark, "%010u\n", (unsigned)first);
    CHECK(mark_is(mark));
    fake_hal_set_grow_limit(0);
    CHECK_EQUAL(MANY_PLAYS - first, scrobbler_log_write(&history, "test"));
    CHECK(file_is(SCROBBLER_LOG_PATH, single_run, size));
    CHECK(mark_is("0000000031\n"));
}

static void a_ring_with_no_records_writes_nothing(void)
{
    memset(ring, 0, sizeof ring);
    start_ring(FIXTURE_SLOTS, 0);
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    history.located = false;
    CHECK_EQUAL(0, scrobbler_log_write(&history, "test"));
    CHECK(missing(SCROBBLER_LOG_PATH));
    CHECK(missing(SCROBBLER_MARK_PATH));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_player_writes_the_log_the_pc_tool_writes),
        TEST_CASE(a_play_resumed_after_the_last_export_is_added_like_the_pc_tool_adds_it),
        TEST_CASE(an_empty_log_gets_the_header),
        TEST_CASE(connecting_again_without_new_plays_changes_nothing),
        TEST_CASE(a_deleted_log_comes_back_with_only_the_new_plays),
        TEST_CASE(plays_without_a_clock_wait_until_a_later_session_has_one),
        TEST_CASE(a_full_card_marks_nothing),
        TEST_CASE(a_log_cut_short_by_a_full_card_is_finished_later_without_repeats),
        TEST_CASE(a_ring_with_no_records_writes_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
