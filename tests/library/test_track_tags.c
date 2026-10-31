#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "library/track_tags.h"
#include "memory_source.h"

static const char TITLE[] = "Svefn-g-englar \xe2\x9c\x93";
static const char ARTIST[] = "Sigur R\xc3\xb3s";
static const char ALBUM_ARTIST[] = "Sigur R\xc3\xb3s & Friends";
static const char ALBUM[] = "\xc3\x81g\xc3\xa6tis byrjun";

typedef struct Expected {
    const char *name;
    TagFormat format;
    const char *genre;
    uint32_t duration_ms;
    TagPictureKind picture;
} Expected;

static const Expected TAGGED[] = {
    {"tagged.flac", TAG_FORMAT_FLAC, "Post-Rock", 1000, TAG_PICTURE_RAW},
    {"tagged.ogg", TAG_FORMAT_OGG, "Post-Rock", 1001, TAG_PICTURE_BASE64_FLAC_BLOCK},
    {"../opus/tagged.opus", TAG_FORMAT_OGG, "Post-Rock", 1000, TAG_PICTURE_BASE64_FLAC_BLOCK},
    {"id3v24.mp3", TAG_FORMAT_MP3, "Post-Rock", 1045, TAG_PICTURE_RAW},
    {"id3v23_vbr.mp3", TAG_FORMAT_MP3, "Rock", 1032, TAG_PICTURE_RAW},
    {"aac.m4a", TAG_FORMAT_MP4, "Post-Rock", 1023, TAG_PICTURE_RAW},
    {"alac.m4a", TAG_FORMAT_MP4, "Post-Rock", 1000, TAG_PICTURE_RAW},
    {"tagged.wav", TAG_FORMAT_WAV, "Post-Rock", 1000, TAG_PICTURE_RAW},
    {"tagged.wma", TAG_FORMAT_WMA, "Post-Rock", 1000, TAG_PICTURE_RAW},
    {"tagged.ape", TAG_FORMAT_APE, "Post-Rock", 3442, TAG_PICTURE_RAW},
    {"tagged.dsf", TAG_FORMAT_DSF, "Post-Rock", 100, TAG_PICTURE_RAW},
    {"tagged.dff", TAG_FORMAT_DFF, "Post-Rock", 100, TAG_PICTURE_RAW},
};

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
    MemorySource memory;
} Fixture;

static bool load(Fixture *fixture, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/tags/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    fixture->size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    fixture->bytes = malloc(fixture->size);
    bool read = fread(fixture->bytes, 1, fixture->size, file) == fixture->size;
    fclose(file);
    fixture->memory = (MemorySource){.bytes = fixture->bytes, .size = fixture->size};
    return read;
}

static bool read_fixture(const char *name, TrackTags *tags)
{
    Fixture fixture;
    CHECK(load(&fixture, name));
    bool read = track_tags_read(memory_source(&fixture.memory), fixture.size,
                                track_tags_format_for_name(name), tags);
    free(fixture.bytes);
    return read;
}

static int leading_number(const char *text)
{
    return atoi(text);
}

static void check_picture(const char *name, const TrackTags *tags, TagPictureKind kind)
{
    Fixture fixture;
    CHECK(load(&fixture, name));
    CHECK_EQUAL(kind, tags->picture.kind);
    CHECK(tags->picture.front);
    CHECK(tags->picture.offset + tags->picture.length <= fixture.size);
    if (kind == TAG_PICTURE_RAW) {
        const uint8_t *start = fixture.bytes + tags->picture.offset;
        CHECK(start[0] == 0xFF && start[1] == 0xD8);
        CHECK(start[tags->picture.length - 2] == 0xFF && start[tags->picture.length - 1] == 0xD9);
    }
    free(fixture.bytes);
}

static void every_tagged_format_reads_its_fields_duration_and_cover(void)
{
    for (size_t index = 0; index < sizeof TAGGED / sizeof TAGGED[0]; index++) {
        const Expected *expected = &TAGGED[index];
        static TrackTags tags;
        CHECK_EQUAL(expected->format, track_tags_format_for_name(expected->name));
        CHECK(read_fixture(expected->name, &tags));
        CHECK_EQUAL(0, strcmp(TITLE, tags.text[TAG_TITLE]));
        CHECK_EQUAL(0, strcmp(ARTIST, tags.text[TAG_ARTIST]));
        CHECK_EQUAL(0, strcmp(ALBUM_ARTIST, tags.text[TAG_ALBUM_ARTIST]));
        CHECK_EQUAL(0, strcmp(ALBUM, tags.text[TAG_ALBUM]));
        CHECK_EQUAL(0, strcmp(expected->genre, tags.text[TAG_GENRE]));
        CHECK_EQUAL(1999, leading_number(tags.text[TAG_DATE]));
        CHECK_EQUAL(1, leading_number(tags.text[TAG_DISC]));
        CHECK_EQUAL(4, leading_number(tags.text[TAG_TRACK]));
        CHECK_EQUAL(expected->duration_ms, tags.duration_ms);
        check_picture(expected->name, &tags, expected->picture);
    }
}

static void an_mp3_with_only_id3v1_and_no_xing_header_still_reads(void)
{
    static TrackTags tags;
    CHECK(read_fixture("id3v1_no_xing.mp3", &tags));
    CHECK_EQUAL(0, strcmp("Old Song", tags.text[TAG_TITLE]));
    CHECK_EQUAL(0, strcmp("Old Artist", tags.text[TAG_ARTIST]));
    CHECK_EQUAL(0, strcmp("", tags.text[TAG_ALBUM_ARTIST]));
    CHECK_EQUAL(0, strcmp("Old Album", tags.text[TAG_ALBUM]));
    CHECK_EQUAL(0, strcmp("Rock", tags.text[TAG_GENRE]));
    CHECK_EQUAL(0, strcmp("1987", tags.text[TAG_DATE]));
    CHECK_EQUAL(7, leading_number(tags.text[TAG_TRACK]));
    CHECK_EQUAL(947u, tags.duration_ms);
    CHECK_EQUAL(TAG_PICTURE_NONE, tags.picture.kind);
}

static void an_untagged_flac_has_empty_fields_and_its_duration(void)
{
    static TrackTags tags;
    CHECK(read_fixture("untagged.flac", &tags));
    for (int field = 0; field < TAG_FIELD_COUNT; field++) {
        CHECK_EQUAL(0, strcmp("", tags.text[field]));
    }
    CHECK_EQUAL(1000u, tags.duration_ms);
    CHECK_EQUAL(TAG_PICTURE_NONE, tags.picture.kind);
}

static void formats_come_from_the_extension_in_any_case(void)
{
    CHECK_EQUAL(TAG_FORMAT_FLAC, track_tags_format_for_name("a.FLAC"));
    CHECK_EQUAL(TAG_FORMAT_MP4, track_tags_format_for_name("song.M4a"));
    CHECK_EQUAL(TAG_FORMAT_UNKNOWN, track_tags_format_for_name("cover.jpg"));
    CHECK_EQUAL(TAG_FORMAT_UNKNOWN, track_tags_format_for_name("flac"));
}

static void a_truncated_file_fails_cleanly(void)
{
    static TrackTags tags;
    Fixture fixture;
    CHECK(load(&fixture, "tagged.flac"));
    fixture.memory.size = 40;
    CHECK(!track_tags_read(memory_source(&fixture.memory), 40, TAG_FORMAT_FLAC, &tags));
    free(fixture.bytes);
}

static const char *const ALL_FIXTURES[] = {
    "tagged.flac", "tagged.ogg",     "../opus/tagged.opus",
    "id3v24.mp3",  "id3v23_vbr.mp3", "id3v1_no_xing.mp3",
    "aac.m4a",     "alac.m4a",       "tagged.wav",
    "tagged.wma",  "tagged.ape",     "tagged.dsf",
    "tagged.dff",  "untagged.flac",
};

static void damaged_files_never_read_outside_the_file(void)
{
    static TrackTags tags;
    for (size_t index = 0; index < sizeof ALL_FIXTURES / sizeof ALL_FIXTURES[0]; index++) {
        const char *name = ALL_FIXTURES[index];
        Fixture fixture;
        CHECK(load(&fixture, name));
        uint8_t *copy = malloc(fixture.size);
        for (uint32_t cut = 0; cut < fixture.size; cut += 1 + fixture.size / 300) {
            MemorySource truncated = {.bytes = fixture.bytes, .size = cut};
            track_tags_read(memory_source(&truncated), cut, track_tags_format_for_name(name),
                            &tags);
        }
        for (uint32_t flip = 0; flip < fixture.size; flip += 1 + fixture.size / 300) {
            memcpy(copy, fixture.bytes, fixture.size);
            copy[flip] ^= 0xFF;
            if (flip + 1 < fixture.size) {
                copy[flip + 1] = 0xFF;
            }
            MemorySource damaged = {.bytes = copy, .size = fixture.size};
            track_tags_read(memory_source(&damaged), fixture.size, track_tags_format_for_name(name),
                            &tags);
            for (int field = 0; field < TAG_FIELD_COUNT; field++) {
                CHECK(memchr(tags.text[field], '\0', TRACK_TAG_TEXT_CAPACITY) != NULL);
            }
            CHECK(tags.picture.kind == TAG_PICTURE_NONE ||
                  tags.picture.offset + tags.picture.length <= fixture.size ||
                  tags.picture.kind == TAG_PICTURE_BASE64_FLAC_BLOCK);
        }
        free(copy);
        free(fixture.bytes);
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_tagged_format_reads_its_fields_duration_and_cover),
        TEST_CASE(an_mp3_with_only_id3v1_and_no_xing_header_still_reads),
        TEST_CASE(an_untagged_flac_has_empty_fields_and_its_duration),
        TEST_CASE(formats_come_from_the_extension_in_any_case),
        TEST_CASE(a_truncated_file_fails_cleanly),
        TEST_CASE(damaged_files_never_read_outside_the_file),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
