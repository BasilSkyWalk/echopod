#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "library/library_builder.h"
#include "library_compare.h"
#include "random_library.h"
#include "reference_builder.h"

enum {
    PLAYER_MEMORY = 360 * 1024,
    REFERENCE_MEMORY = 64 * 1024 * 1024,
    SCRATCH_SIZE = 32 * 1024 * 1024,
    OUTPUT_CAPACITY = 16 * 1024 * 1024,
    GUARD_BYTES = 4096,
    MINUTE_MS = 60000
};

typedef struct Output {
    uint8_t *bytes;
    uint32_t length;
    int writes;
} Output;

typedef struct Scratch {
    uint8_t *bytes;
    uint32_t size;
    uint32_t highest;
} Scratch;

typedef struct Built {
    BuilderResult result;
    Output output;
    uint32_t length;
} Built;

static bool write_output(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Output *output = context;
    if (offset > OUTPUT_CAPACITY || length > OUTPUT_CAPACITY - offset) {
        return false;
    }
    memcpy(output->bytes + offset, bytes, length);
    output->length = offset + length > output->length ? offset + length : output->length;
    output->writes++;
    return true;
}

static bool write_reference(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    return write_output(context, offset, bytes, length);
}

static bool scratch_read(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    Scratch *scratch = context;
    if (offset > scratch->size || length > scratch->size - offset) {
        return false;
    }
    memcpy(buffer, scratch->bytes + offset, length);
    return true;
}

static bool scratch_write(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Scratch *scratch = context;
    if (offset > scratch->size || length > scratch->size - offset) {
        return false;
    }
    memcpy(scratch->bytes + offset, bytes, length);
    scratch->highest = offset + length > scratch->highest ? offset + length : scratch->highest;
    return true;
}

static Scratch *scratch_of_size(uint32_t size)
{
    static Scratch scratch;
    free(scratch.bytes);
    scratch = (Scratch){.bytes = calloc(size, 1), .size = size, .highest = 0};
    return &scratch;
}

static BuilderScratch scratch_io(Scratch *scratch)
{
    return (BuilderScratch){scratch_read, scratch_write, scratch, scratch->size};
}

static Output fresh_output(void)
{
    return (Output){.bytes = calloc(OUTPUT_CAPACITY, 1), .length = 0, .writes = 0};
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
    uint8_t *bytes = malloc(*size + 1);
    CHECK(fread(bytes, 1, *size, file) == *size);
    fclose(file);
    bytes[*size] = 0;
    return bytes;
}

static uint8_t *player_memory(void)
{
    static uint8_t memory[PLAYER_MEMORY];
    return memory;
}

static void start(LibraryBuilder *builder)
{
    library_builder_init(builder, player_memory(), PLAYER_MEMORY,
                         scratch_io(scratch_of_size(SCRATCH_SIZE)));
}

static void finish(LibraryBuilder *builder, Built *built)
{
    built->output = fresh_output();
    built->length = 0;
    built->result = library_builder_finish(builder, (ByteSink){write_output, &built->output}, 0,
                                           &built->length);
}

static void check_means_fixture(const Built *built, const char *fixture)
{
    CHECK_EQUAL(BUILDER_OK, built->result);
    uint32_t expected_size = 0;
    uint8_t *expected = read_fixture(fixture, &expected_size);
    CHECK_EQUAL(built->length, built->output.length);
    CHECK(library_compare(expected, expected_size, built->output.bytes, built->length));
    free(expected);
    free(built->output.bytes);
}

static void the_fixture_tracks_mean_the_same_as_the_pc_indexer(void)
{
    LibraryBuilder builder;
    start(&builder);
    add_fixture_tracks(&builder);
    Built built;
    finish(&builder, &built);
    check_means_fixture(&built, "small_library_tracks.db");
}

static char *next_field(char **cursor)
{
    char *start_of_field = *cursor;
    while (**cursor != '\t' && **cursor != '\n' && **cursor != '\0') {
        (*cursor)++;
    }
    if (**cursor != '\0') {
        *(*cursor)++ = '\0';
    }
    return start_of_field;
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

static void add_tsv_line(LibraryBuilder *builder, char **cursor)
{
    if (strncmp(*cursor, "#PLAYLIST\t", 10) == 0) {
        *cursor += 10;
        char *name = next_field(cursor);
        char *folder = next_field(cursor);
        CHECK_EQUAL(BUILDER_OK, library_builder_begin_playlist(builder, name, folder));
    } else if (**cursor == '\t') {
        (*cursor)++;
        CHECK_EQUAL(BUILDER_OK, library_builder_playlist_entry(builder, next_field(cursor)));
    } else {
        BuilderTrack parsed = parse_track(cursor);
        CHECK_EQUAL(BUILDER_OK, library_builder_add_track(builder, &parsed));
    }
}

static void build_tsv(const char *tsv, const char *fixture)
{
    uint32_t size = 0;
    char *text = (char *)read_fixture(tsv, &size);
    LibraryBuilder builder;
    start(&builder);
    char *cursor = text;
    while (*cursor != '\0') {
        add_tsv_line(&builder, &cursor);
    }
    Built built;
    finish(&builder, &built);
    check_means_fixture(&built, fixture);
    free(text);
}

static void random_tracks_mean_the_same_as_the_pc_indexer(void)
{
    build_tsv("builder_random/tracks.tsv", "builder_random/library.db");
}

static void the_fixture_playlist_resolves_like_the_pc_indexer(void)
{
    LibraryBuilder builder;
    start(&builder);
    add_fixture_tracks(&builder);
    CHECK_EQUAL(BUILDER_OK, library_builder_begin_playlist(&builder, "Road Trip", "Playlists"));
    const char *entries[] = {
        "../acloudyskye/This Won't Be The Last Time/01 - Shoots.flac",
        "../AC DC/Back In Black/1-01 - Hells Bells.flac",
        "../Missing/Gone.flac",
        "../Bj\xc3\xb6rk/Homogenic/02 - J\xc3\xb3ga.flac",
    };
    for (size_t index = 0; index < sizeof entries / sizeof entries[0]; index++) {
        CHECK_EQUAL(BUILDER_OK, library_builder_playlist_entry(&builder, entries[index]));
    }
    Built built;
    finish(&builder, &built);
    check_means_fixture(&built, "small_library.db");
}

static void random_playlists_resolve_like_the_pc_indexer(void)
{
    build_tsv("builder_random/with_playlists.tsv", "builder_random/with_playlists.db");
}

static ReferenceTrack reference_track(const BuilderTrack *track)
{
    return (ReferenceTrack){.path = track->path,
                            .title = track->title,
                            .artist = track->artist,
                            .album_artist = track->album_artist,
                            .album = track->album,
                            .genre = track->genre,
                            .year = track->year,
                            .disc = track->disc,
                            .number = track->number,
                            .duration_ms = track->duration_ms};
}

static void build_reference(const RandomLibrary *library, Built *built)
{
    uint8_t *memory = malloc(REFERENCE_MEMORY);
    ReferenceBuilder builder;
    reference_builder_init(&builder, memory, REFERENCE_MEMORY);
    for (uint32_t index = 0; index < library->track_count; index++) {
        ReferenceTrack added = reference_track(&library->tracks[index]);
        CHECK_EQUAL(REFERENCE_OK, reference_builder_add_track(&builder, &added));
    }
    for (uint32_t index = 0; index < library->playlist_count; index++) {
        const RandomPlaylist *playlist = &library->playlists[index];
        CHECK_EQUAL(REFERENCE_OK,
                    reference_builder_begin_playlist(&builder, playlist->name, playlist->folder));
        for (uint32_t entry = 0; entry < playlist->count; entry++) {
            CHECK_EQUAL(REFERENCE_OK,
                        reference_builder_playlist_entry(&builder, playlist->entries[entry]));
        }
    }
    built->output = fresh_output();
    built->length = 0;
    CHECK_EQUAL(REFERENCE_OK,
                reference_builder_finish(&builder, (ReferenceSink){write_reference, &built->output},
                                         0, &built->length));
    free(memory);
}

static BuilderResult add_library(LibraryBuilder *builder, const RandomLibrary *library)
{
    for (uint32_t index = 0; index < library->track_count; index++) {
        BuilderResult result = library_builder_add_track(builder, &library->tracks[index]);
        if (result != BUILDER_OK) {
            return result;
        }
    }
    for (uint32_t index = 0; index < library->playlist_count; index++) {
        const RandomPlaylist *playlist = &library->playlists[index];
        BuilderResult result =
            library_builder_begin_playlist(builder, playlist->name, playlist->folder);
        for (uint32_t entry = 0; result == BUILDER_OK && entry < playlist->count; entry++) {
            result = library_builder_playlist_entry(builder, playlist->entries[entry]);
        }
        if (result != BUILDER_OK) {
            return result;
        }
    }
    return BUILDER_OK;
}

static void check_matches_reference(uint32_t tracks, uint32_t playlists, uint32_t seed)
{
    RandomLibrary library;
    random_library_make(&library, tracks, playlists, seed);
    Built expected;
    build_reference(&library, &expected);
    LibraryBuilder builder;
    start(&builder);
    CHECK_EQUAL(BUILDER_OK, add_library(&builder, &library));
    Built built;
    finish(&builder, &built);
    CHECK_EQUAL(BUILDER_OK, built.result);
    CHECK(
        library_compare(expected.output.bytes, expected.length, built.output.bytes, built.length));
    free(expected.output.bytes);
    free(built.output.bytes);
    random_library_free(&library);
}

static void random_libraries_match_the_reference_builder(void)
{
    check_matches_reference(1, 1, 1);
    check_matches_reference(60, 3, 2);
    check_matches_reference(900, 12, 3);
    check_matches_reference(3000, 20, 4);
}

static void a_20000_track_library_builds_in_the_players_memory(void)
{
    check_matches_reference(20000, 30, 5);
}

static void the_builder_stays_inside_its_memory(void)
{
    static uint8_t block[PLAYER_MEMORY + 2 * GUARD_BYTES];
    memset(block, 0xA5, sizeof block);
    RandomLibrary library;
    random_library_make(&library, 3000, 10, 6);
    LibraryBuilder builder;
    library_builder_init(&builder, block + GUARD_BYTES, PLAYER_MEMORY,
                         scratch_io(scratch_of_size(SCRATCH_SIZE)));
    CHECK_EQUAL(BUILDER_OK, add_library(&builder, &library));
    Built built;
    finish(&builder, &built);
    CHECK_EQUAL(BUILDER_OK, built.result);
    bool untouched = true;
    for (uint32_t index = 0; index < GUARD_BYTES; index++) {
        untouched =
            untouched && block[index] == 0xA5 && block[GUARD_BYTES + PLAYER_MEMORY + index] == 0xA5;
    }
    CHECK(untouched);
    free(built.output.bytes);
    random_library_free(&library);
}

static void a_scratch_file_too_small_says_there_is_no_room(void)
{
    RandomLibrary library;
    random_library_make(&library, 3000, 0, 7);
    LibraryBuilder builder;
    library_builder_init(&builder, player_memory(), PLAYER_MEMORY,
                         scratch_io(scratch_of_size(64 * 1024)));
    BuilderResult result = add_library(&builder, &library);
    if (result == BUILDER_OK) {
        Built built;
        finish(&builder, &built);
        result = built.result;
        free(built.output.bytes);
    }
    CHECK_EQUAL(BUILDER_NO_ROOM, result);
    random_library_free(&library);
}

static void too_little_memory_says_so(void)
{
    static uint8_t memory[2048];
    LibraryBuilder builder;
    library_builder_init(&builder, memory, sizeof memory,
                         scratch_io(scratch_of_size(SCRATCH_SIZE)));
    BuilderTrack added =
        track("Artist/Album/01.flac", "One", "Artist", "Artist", "Album", "Rock", 2000, 1);
    BuilderResult result = library_builder_add_track(&builder, &added);
    if (result == BUILDER_OK) {
        Built built;
        finish(&builder, &built);
        result = built.result;
        free(built.output.bytes);
    }
    CHECK_EQUAL(BUILDER_OUT_OF_MEMORY, result);
}

static void the_65536th_track_is_too_many(void)
{
    LibraryBuilder builder;
    start(&builder);
    char path[64];
    BuilderResult result = BUILDER_OK;
    uint32_t added = 0;
    for (; added < 65536 && result == BUILDER_OK; added++) {
        snprintf(path, sizeof path, "A/%05u.flac", added);
        BuilderTrack next = track(path, "T", "A", "A", "B", "G", 2000, 1);
        result = library_builder_add_track(&builder, &next);
    }
    CHECK_EQUAL(BUILDER_TOO_MANY, result);
    CHECK_EQUAL(65536u, added);
}

static void a_library_bigger_than_its_room_writes_nothing(void)
{
    LibraryBuilder builder;
    start(&builder);
    add_fixture_tracks(&builder);
    Output output = fresh_output();
    uint32_t length = 0;
    CHECK_EQUAL(BUILDER_NO_ROOM,
                library_builder_finish(&builder, (ByteSink){write_output, &output}, 1000, &length));
    CHECK_EQUAL(0, output.writes);
    free(output.bytes);
}

static void an_empty_library_still_writes_a_valid_header(void)
{
    LibraryBuilder builder;
    start(&builder);
    Built built;
    finish(&builder, &built);
    CHECK_EQUAL(BUILDER_OK, built.result);
    CHECK(built.length >= 128);
    CHECK_EQUAL(0, memcmp("EMDB", built.output.bytes, 4));
    free(built.output.bytes);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_fixture_tracks_mean_the_same_as_the_pc_indexer),
        TEST_CASE(random_tracks_mean_the_same_as_the_pc_indexer),
        TEST_CASE(the_fixture_playlist_resolves_like_the_pc_indexer),
        TEST_CASE(random_playlists_resolve_like_the_pc_indexer),
        TEST_CASE(random_libraries_match_the_reference_builder),
        TEST_CASE(a_20000_track_library_builds_in_the_players_memory),
        TEST_CASE(the_builder_stays_inside_its_memory),
        TEST_CASE(a_scratch_file_too_small_says_there_is_no_room),
        TEST_CASE(too_little_memory_says_so),
        TEST_CASE(the_65536th_track_is_too_many),
        TEST_CASE(a_library_bigger_than_its_room_writes_nothing),
        TEST_CASE(an_empty_library_still_writes_a_valid_header),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    free(scratch_of_size(1)->bytes);
    return result;
}
