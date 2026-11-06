#include <string.h>

#include "check.h"
#include "playback/lyrics.h"

static Lyrics lyrics;

static void load(const char *text)
{
    lyrics_start(&lyrics);
    lyrics_feed(&lyrics, text, (uint32_t)strlen(text));
    lyrics_finish(&lyrics);
}

static void load_in_pieces(const char *text, uint32_t piece)
{
    lyrics_start(&lyrics);
    uint32_t length = (uint32_t)strlen(text);
    for (uint32_t offset = 0; offset < length; offset += piece) {
        uint32_t size = length - offset < piece ? length - offset : piece;
        lyrics_feed(&lyrics, text + offset, size);
    }
    lyrics_finish(&lyrics);
}

static bool line_is(int index, const char *expected)
{
    return strcmp(expected, lyrics_line(&lyrics, index)) == 0;
}

static void synced_lines_follow_the_position(void)
{
    load("[ti:Song]\n[ar:Someone]\n[00:01.00]First\n[00:03.50]Second\n[01:02.345]Third\n");
    CHECK(lyrics.synced);
    CHECK_EQUAL(3, lyrics.count);
    CHECK(line_is(0, "First"));
    CHECK_EQUAL(3500u, lyrics.lines[1].time_ms);
    CHECK_EQUAL(62345u, lyrics.lines[2].time_ms);
    CHECK_EQUAL(LYRICS_NONE, lyrics_current(&lyrics, 999, 90000));
    CHECK_EQUAL(0, lyrics_current(&lyrics, 1000, 90000));
    CHECK_EQUAL(1, lyrics_current(&lyrics, 62344, 90000));
    CHECK_EQUAL(2, lyrics_current(&lyrics, 80000, 90000));
}

static void a_line_with_several_times_repeats_in_order(void)
{
    load("[00:10.00][00:30.00]Chorus\n[00:20.00]Verse\n[00:05]Intro\n");
    CHECK_EQUAL(4, lyrics.count);
    CHECK(line_is(0, "Intro"));
    CHECK(line_is(1, "Chorus"));
    CHECK(line_is(2, "Verse"));
    CHECK(line_is(3, "Chorus"));
    CHECK_EQUAL(30000u, lyrics.lines[3].time_ms);
}

static void the_offset_moves_every_line(void)
{
    load("[offset:+500]\n[00:01.00]A\n[00:00.20]B\n");
    CHECK_EQUAL(500u, lyrics.lines[1].time_ms);
    CHECK_EQUAL(0u, lyrics.lines[0].time_ms);
    CHECK(line_is(0, "B"));
    load("[offset:-250]\n[00:01.00]A\n");
    CHECK_EQUAL(1250u, lyrics.lines[0].time_ms);
}

static void fractions_of_any_length_and_colon_separators_read(void)
{
    load("[00:01.5]a\n[00:02:05]b\n[00:03.1239]c\n[0:04]d\n");
    CHECK_EQUAL(1500u, lyrics.lines[0].time_ms);
    CHECK_EQUAL(2050u, lyrics.lines[1].time_ms);
    CHECK_EQUAL(3123u, lyrics.lines[2].time_ms);
    CHECK_EQUAL(4000u, lyrics.lines[3].time_ms);
}

static void plain_text_is_unsynced_and_scrolls_with_the_track(void)
{
    load("\n\nFirst line\n[Chorus]\n\nLast line\n\n\n");
    CHECK(!lyrics.synced);
    CHECK_EQUAL(4, lyrics.count);
    CHECK(line_is(1, "[Chorus]"));
    CHECK(line_is(2, ""));
    CHECK_EQUAL(0, lyrics_current(&lyrics, 0, 100000));
    CHECK_EQUAL(2, lyrics_current(&lyrics, 50000, 100000));
    CHECK_EQUAL(3, lyrics_current(&lyrics, 100000, 100000));
    CHECK_EQUAL(0, lyrics_current(&lyrics, 5000, 0));
}

static void untimed_lines_are_dropped_from_synced_lyrics(void)
{
    load("Credits\n[00:01.00]Sung\n\n");
    CHECK(lyrics.synced);
    CHECK_EQUAL(1, lyrics.count);
    CHECK(line_is(0, "Sung"));
}

static void bom_crlf_and_word_times_are_cleaned(void)
{
    load("\xEF\xBB\xBF[00:01.00]  <00:01.00>Hello <00:01.50>world  \r\n[00:02.00]\r\n");
    CHECK_EQUAL(2, lyrics.count);
    CHECK(line_is(0, "Hello world"));
    CHECK(line_is(1, ""));
}

static void utf8_text_survives_any_chunking(void)
{
    static const char TEXT[] = "[00:01.00]\xE5\x88\x9D\xE6\x81\x8B\n"
                               "[00:02.00]\xD0\x93\xD1\x80\xD1\x83\xD0\xBF\xD0\xBF\xD0\xB0\n";
    for (uint32_t piece = 1; piece < sizeof TEXT; piece++) {
        load_in_pieces(TEXT, piece);
        CHECK_EQUAL(2, lyrics.count);
        CHECK(line_is(0, "\xE5\x88\x9D\xE6\x81\x8B"));
        CHECK(line_is(1, "\xD0\x93\xD1\x80\xD1\x83\xD0\xBF\xD0\xBF\xD0\xB0"));
    }
}

static void a_long_line_is_cut_at_a_whole_character(void)
{
    static char text[LYRICS_PENDING_CAPACITY * 2];
    strcpy(text, "[00:01.00]");
    size_t length = strlen(text);
    while (length + 3 < sizeof text - 2) {
        memcpy(text + length, "\xE6\x81\x8B", 3);
        length += 3;
    }
    text[length++] = '\n';
    text[length] = '\0';
    load(text);
    CHECK_EQUAL(1, lyrics.count);
    size_t kept = strlen(lyrics_line(&lyrics, 0));
    CHECK_EQUAL(0u, kept % 3);
    CHECK(kept + 10 <= LYRICS_PENDING_CAPACITY);
    CHECK(kept + 13 > LYRICS_PENDING_CAPACITY);
}

static void a_huge_file_stops_when_full_and_says_so(void)
{
    lyrics_start(&lyrics);
    char line[64];
    int fed = 0;
    while (!lyrics_full(&lyrics) && fed < 5000) {
        int seconds = fed % 60;
        int minutes = fed / 60;
        line[0] = '[';
        line[1] = (char)('0' + minutes / 10 % 10);
        line[2] = (char)('0' + minutes % 10);
        line[3] = ':';
        line[4] = (char)('0' + seconds / 10);
        line[5] = (char)('0' + seconds % 10);
        memcpy(line + 6, "]Some words in a line\n", 23);
        lyrics_feed(&lyrics, line, (uint32_t)strlen(line));
        fed++;
    }
    lyrics_finish(&lyrics);
    CHECK(lyrics_full(&lyrics));
    CHECK(fed < 5000);
    CHECK(lyrics.count > 100);
    CHECK(lyrics.count <= LYRICS_LINE_CAPACITY);
    CHECK(lyrics.text_used <= LYRICS_TEXT_CAPACITY);
    CHECK(line_is(lyrics.count - 1, "Some words in a line"));
}

static void tags_that_are_not_times_stay_text(void)
{
    load("[00:01.00][x]Bracketed\n[00:02.00][00:0x]Odd\n[length: 3:20]\n");
    CHECK_EQUAL(2, lyrics.count);
    CHECK(line_is(0, "[x]Bracketed"));
    CHECK(line_is(1, "[00:0x]Odd"));
}

static void an_empty_file_has_no_lines(void)
{
    load("");
    CHECK_EQUAL(0, lyrics.count);
    CHECK_EQUAL(LYRICS_NONE, lyrics_current(&lyrics, 1000, 2000));
    CHECK(line_is(0, ""));
    CHECK(line_is(-1, ""));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(synced_lines_follow_the_position),
        TEST_CASE(a_line_with_several_times_repeats_in_order),
        TEST_CASE(the_offset_moves_every_line),
        TEST_CASE(fractions_of_any_length_and_colon_separators_read),
        TEST_CASE(plain_text_is_unsynced_and_scrolls_with_the_track),
        TEST_CASE(untimed_lines_are_dropped_from_synced_lyrics),
        TEST_CASE(bom_crlf_and_word_times_are_cleaned),
        TEST_CASE(utf8_text_survives_any_chunking),
        TEST_CASE(a_long_line_is_cut_at_a_whole_character),
        TEST_CASE(a_huge_file_stops_when_full_and_says_so),
        TEST_CASE(tags_that_are_not_times_stay_text),
        TEST_CASE(an_empty_file_has_no_lines),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
