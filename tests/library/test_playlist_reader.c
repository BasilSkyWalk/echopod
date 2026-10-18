#include <string.h>

#include "check.h"
#include "library/playlist_reader.h"
#include "memory_source.h"

typedef struct Expected {
    const char *bytes;
    uint32_t size;
    const char *entries[6];
} Expected;

static void check_entries(const char *bytes, uint32_t size, const char *const *wanted)
{
    MemorySource memory = {.bytes = (const uint8_t *)bytes, .size = size};
    PlaylistReader reader;
    CHECK(playlist_reader_open(&reader, memory_source(&memory), size));
    char entry[PLAYLIST_ENTRY_CAPACITY];
    int index = 0;
    while (playlist_reader_next(&reader, entry, sizeof entry)) {
        CHECK(wanted[index] != NULL);
        if (wanted[index] == NULL) {
            return;
        }
        CHECK_EQUAL(0, strcmp(wanted[index], entry));
        index++;
    }
    CHECK(wanted[index] == NULL);
}

static void utf8_with_a_bom_skips_comments_and_blank_lines(void)
{
    static const char TEXT[] = "\xef\xbb\xbf#EXTM3U\r\n../A/b.flac\r\n\r\n  # c\n  x.flac  \n";
    static const char *const WANTED[] = {"../A/b.flac", "x.flac", NULL};
    check_entries(TEXT, sizeof TEXT - 1, WANTED);
}

static void windows_1252_is_the_fallback(void)
{
    static const char TEXT[] = "Bj\xf6rk\\J\xf3ga.flac\x85"
                               "end\nlast\x1dsplit\n\x81"
                               "bad";
    static const char *const WANTED[] = {"Bj\xc3\xb6rk\\J\xc3\xb3ga.flac\xe2\x80\xa6"
                                         "end",
                                         "last", "split",
                                         "\xef\xbf\xbd"
                                         "bad",
                                         NULL};
    check_entries(TEXT, sizeof TEXT - 1, WANTED);
}

static void unicode_line_separators_split_utf8(void)
{
    static const char TEXT[] = "uni\xe2\x80\xa8sep\xc2\x85next\x0bv\x0c"
                               "f";
    static const char *const WANTED[] = {"uni", "sep", "next", "v", "f", NULL};
    check_entries(TEXT, sizeof TEXT - 1, WANTED);
}

static void an_encoded_surrogate_is_not_utf8(void)
{
    static const char TEXT[] = "\xed\xa0\x80"
                               "bad surrogate";
    static const char *const WANTED[] = {"\xc3\xad\xc2\xa0\xe2\x82\xac"
                                         "bad surrogate",
                                         NULL};
    check_entries(TEXT, sizeof TEXT - 1, WANTED);
}

static void long_files_cross_chunk_boundaries(void)
{
    static char text[5000];
    static const char LINE[] = "Artist/Album/Track.flac\n";
    uint32_t used = 0;
    while (used + sizeof LINE - 1 < sizeof text) {
        memcpy(text + used, LINE, sizeof LINE - 1);
        used += sizeof LINE - 1;
    }
    MemorySource memory = {.bytes = (const uint8_t *)text, .size = used};
    PlaylistReader reader;
    CHECK(playlist_reader_open(&reader, memory_source(&memory), used));
    char entry[PLAYLIST_ENTRY_CAPACITY];
    uint32_t count = 0;
    while (playlist_reader_next(&reader, entry, sizeof entry)) {
        CHECK_EQUAL(0, strcmp("Artist/Album/Track.flac", entry));
        count++;
    }
    CHECK_EQUAL(used / (sizeof LINE - 1), count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(utf8_with_a_bom_skips_comments_and_blank_lines),
        TEST_CASE(windows_1252_is_the_fallback),
        TEST_CASE(unicode_line_separators_split_utf8),
        TEST_CASE(an_encoded_surrogate_is_not_utf8),
        TEST_CASE(long_files_cross_chunk_boundaries),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
