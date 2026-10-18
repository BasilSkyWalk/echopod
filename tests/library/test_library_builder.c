#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "library/library_builder.h"

enum {
    MEMORY_SIZE = 256 * 1024,
    OUTPUT_CAPACITY = 64 * 1024,
    MINUTE_MS = 60000
};

typedef struct Output {
    uint8_t bytes[OUTPUT_CAPACITY];
    uint32_t length;
    int writes;
} Output;

static bool write_output(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Output *output = context;
    if (offset + length > OUTPUT_CAPACITY) {
        return false;
    }
    memcpy(output->bytes + offset, bytes, length);
    output->length = offset + length > output->length ? offset + length : output->length;
    output->writes++;
    return true;
}

static BuilderTrack track(const char *path, const char *title, const char *artist,
                          const char *album_artist, const char *album, const char *genre,
                          uint16_t year, uint8_t number)
{
    return (BuilderTrack){.path = path,
                          .title = title,
                          .artist = artist,
                          .album_artist = album_artist,
                          .album = album,
                          .genre = genre,
                          .year = year,
                          .disc = 1,
                          .number = number,
                          .duration_ms = MINUTE_MS};
}

static void add_fixture_tracks(LibraryBuilder *builder)
{
    const BuilderTrack tracks[] = {
        track("AC DC/Back In Black/1-02 - Shoot to Thrill.flac", "Shoot to Thrill", "AC/DC",
              "AC/DC", "Back In Black", "Metal", 1980, 2),
        track("AC DC/Back In Black/1-01 - Hells Bells.flac", "Hells Bells", "AC/DC", "AC/DC",
              "Back In Black", "Metal", 1980, 1),
        track("AC DC/High Voltage/1-01 - Long Way.flac", "It's a Long Way to the Top", "AC/DC",
              "AC/DC", "High Voltage", "Metal", 1976, 1),
        track("Bj\xc3\xb6rk/Homogenic/02 - J\xc3\xb3ga.flac", "J\xc3\xb3ga", "Bj\xc3\xb6rk",
              "Bj\xc3\xb6rk", "Homogenic", "Electronic", 1997, 2),
        track("Bj\xc3\xb6rk/Homogenic/01 - Hunter.flac", "Hunter", "Bj\xc3\xb6rk", "Bj\xc3\xb6rk",
              "Homogenic", "Electronic", 1997, 1),
        track("Various Artists/The Best of Something/01 - Help!.flac", "Help!", "The Beatles",
              "Various Artists", "The Best of Something", "Pop", 2001, 1),
        track("Various Artists/The Best of Something/02 - Army of Me.flac", "Army of Me",
              "Bj\xc3\xb6rk", "Various Artists", "The Best of Something", "Pop", 2001, 2),
        track("acloudyskye/This Won't Be The Last Time/01 - Shoots.flac", "Shoots", "acloudyskye",
              "acloudyskye", "This Won't Be The Last Time", "Unknown Genre", 2025, 1),
    };
    for (size_t index = 0; index < sizeof tracks / sizeof tracks[0]; index++) {
        CHECK_EQUAL(BUILDER_OK, library_builder_add_track(builder, &tracks[index]));
    }
}

static uint8_t *read_fixture(const char *name, uint32_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    fseek(file, 0, SEEK_END);
    *size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *bytes = malloc(*size);
    CHECK(fread(bytes, 1, *size, file) == *size);
    fclose(file);
    return bytes;
}

static void the_fixture_tracks_build_the_same_bytes_as_the_pc_indexer(void)
{
    static uint8_t memory[MEMORY_SIZE];
    static Output output;
    output.length = 0;
    LibraryBuilder builder;
    library_builder_init(&builder, memory, sizeof memory);
    add_fixture_tracks(&builder);
    uint32_t length = 0;
    CHECK_EQUAL(BUILDER_OK,
                library_builder_finish(&builder, (ByteSink){write_output, &output}, &length));
    uint32_t expected_size = 0;
    uint8_t *expected = read_fixture("small_library_tracks.db", &expected_size);
    CHECK_EQUAL(expected_size, length);
    CHECK_EQUAL(expected_size, output.length);
    uint32_t first_difference = expected_size;
    for (uint32_t index = 0; index < expected_size && index < output.length; index++) {
        if (expected[index] != output.bytes[index]) {
            first_difference = index;
            break;
        }
    }
    CHECK_EQUAL(expected_size, first_difference);
    free(expected);
}

static char *next_field(char **cursor)
{
    char *start = *cursor;
    while (**cursor != '\t' && **cursor != '\n' && **cursor != '\0') {
        (*cursor)++;
    }
    if (**cursor != '\0') {
        *(*cursor)++ = '\0';
    }
    return start;
}

static BuilderTrack parse_track(char **cursor)
{
    BuilderTrack parsed;
    parsed.path = next_field(cursor);
    parsed.title = next_field(cursor);
    parsed.artist = next_field(cursor);
    parsed.album_artist = next_field(cursor);
    parsed.album = next_field(cursor);
    parsed.genre = next_field(cursor);
    parsed.year = (uint16_t)atoi(next_field(cursor));
    parsed.disc = (uint8_t)atoi(next_field(cursor));
    parsed.number = (uint8_t)atoi(next_field(cursor));
    parsed.duration_ms = (uint32_t)strtoul(next_field(cursor), NULL, 10);
    return parsed;
}

static void random_tracks_build_the_same_bytes_as_the_pc_indexer(void)
{
    static uint8_t memory[MEMORY_SIZE];
    static Output output;
    output.length = 0;
    uint32_t tsv_size = 0;
    uint8_t *tsv = read_fixture("builder_random/tracks.tsv", &tsv_size);
    char *text = realloc(tsv, tsv_size + 1);
    text[tsv_size] = '\0';
    LibraryBuilder builder;
    library_builder_init(&builder, memory, sizeof memory);
    char *cursor = text;
    while (*cursor != '\0') {
        BuilderTrack parsed = parse_track(&cursor);
        CHECK_EQUAL(BUILDER_OK, library_builder_add_track(&builder, &parsed));
    }
    uint32_t length = 0;
    CHECK_EQUAL(BUILDER_OK,
                library_builder_finish(&builder, (ByteSink){write_output, &output}, &length));
    uint32_t expected_size = 0;
    uint8_t *expected = read_fixture("builder_random/library.db", &expected_size);
    CHECK_EQUAL(expected_size, length);
    uint32_t first_difference = expected_size;
    for (uint32_t index = 0; index < expected_size && index < output.length; index++) {
        if (expected[index] != output.bytes[index]) {
            first_difference = index;
            break;
        }
    }
    CHECK_EQUAL(expected_size, first_difference);
    free(expected);
    free(text);
}

static void a_library_too_big_for_the_memory_says_so(void)
{
    static uint8_t memory[2048];
    LibraryBuilder builder;
    library_builder_init(&builder, memory, sizeof memory);
    BuilderResult result = BUILDER_OK;
    char path[64];
    for (int index = 0; index < 200 && result == BUILDER_OK; index++) {
        snprintf(path, sizeof path, "Artist/Album/%03d.flac", index);
        BuilderTrack added = track(path, path, "Artist", "Artist", "Album", "Rock", 2000, 1);
        result = library_builder_add_track(&builder, &added);
    }
    CHECK_EQUAL(BUILDER_OUT_OF_MEMORY, result);
}

static void an_empty_library_still_writes_a_valid_header(void)
{
    static uint8_t memory[MEMORY_SIZE];
    static Output output;
    output.length = 0;
    LibraryBuilder builder;
    library_builder_init(&builder, memory, sizeof memory);
    uint32_t length = 0;
    CHECK_EQUAL(BUILDER_OK,
                library_builder_finish(&builder, (ByteSink){write_output, &output}, &length));
    CHECK(length >= 128);
    CHECK_EQUAL(0, memcmp("EMDB", output.bytes, 4));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_fixture_tracks_build_the_same_bytes_as_the_pc_indexer),
        TEST_CASE(random_tracks_build_the_same_bytes_as_the_pc_indexer),
        TEST_CASE(a_library_too_big_for_the_memory_says_so),
        TEST_CASE(an_empty_library_still_writes_a_valid_header),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
