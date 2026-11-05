#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "library/track_tags.h"
#include "memory_source.h"
#include "tags_internal.h"

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

static uint8_t *decoded_picture(Fixture *fixture, TagFormat format, uint32_t *length, bool *front)
{
    static TrackTags tags;
    static TagPictureReader reader;
    ByteSource bytes;
    ByteSource source = memory_source(&fixture->memory);
    CHECK(track_tags_read(source, fixture->size, format, &tags));
    CHECK_EQUAL(0, strcmp(TITLE, tags.text[TAG_TITLE]));
    CHECK_EQUAL(0, strcmp(ALBUM, tags.text[TAG_ALBUM]));
    CHECK_EQUAL(4, leading_number(tags.text[TAG_TRACK]));
    CHECK(tags_open_picture(&reader, source, fixture->size, &tags.picture, &bytes));
    CHECK_EQUAL(TAG_PICTURE_RAW, tags.picture.kind);
    uint8_t *out = malloc(tags.picture.length);
    uint32_t half = tags.picture.length / 2;
    CHECK(bytes.read(bytes.context, tags.picture.offset + half, out + half,
                     tags.picture.length - half));
    CHECK(bytes.read(bytes.context, tags.picture.offset, out, half));
    *length = tags.picture.length;
    *front = tags.picture.front;
    return out;
}

static uint8_t *picture_bytes(const char *name, uint32_t *length, bool *front)
{
    Fixture fixture;
    CHECK(load(&fixture, name));
    uint8_t *out = decoded_picture(&fixture, track_tags_format_for_name(name), length, front);
    free(fixture.bytes);
    return out;
}

static uint32_t page_end(const uint8_t *page)
{
    uint32_t end = 27 + page[26];
    for (uint32_t index = 0; index < page[26]; index++) {
        end += page[27 + index];
    }
    return end;
}

static uint32_t repage_comments(const uint8_t *ogg, uint32_t size, uint8_t *out)
{
    uint32_t second = page_end(ogg);
    uint32_t third = second + page_end(ogg + second);
    const uint8_t *packet = ogg + second + 27 + ogg[second + 26];
    uint32_t length = third - (uint32_t)(packet - ogg);
    memcpy(out, ogg, second);
    uint8_t *at = out + second;
    for (uint32_t done = 0, page = 0; done <= length; page++) {
        uint32_t lace = length - done < 255 ? length - done : 255;
        memcpy(at, ogg + second, 27);
        at[5] = page > 0 ? 1 : 0;
        at[26] = 1;
        at[27] = (uint8_t)lace;
        memcpy(at + 28, packet + done, lace);
        at += 28 + lace;
        done += lace;
        if (lace < 255) {
            break;
        }
    }
    memcpy(at, ogg + third, size - third);
    return (uint32_t)(at - out) + size - third;
}

static void ogg_comments_and_pictures_read_across_small_pages(void)
{
    uint32_t expected_length = 0;
    uint32_t length = 0;
    bool front = false;
    Fixture original;
    CHECK(load(&original, "../opus/tagged.opus"));
    uint8_t *expected = picture_bytes("tagged.flac", &expected_length, &front);
    uint8_t *paged = malloc(original.size * 2);
    Fixture fixture = {.bytes = paged,
                       .size = repage_comments(original.bytes, original.size, paged)};
    fixture.memory = (MemorySource){.bytes = fixture.bytes, .size = fixture.size};
    CHECK(fixture.size > original.size + 100);
    uint8_t *bytes = decoded_picture(&fixture, TAG_FORMAT_OGG, &length, &front);
    CHECK(front);
    CHECK_EQUAL(expected_length, length);
    CHECK_EQUAL(0, memcmp(expected, bytes, length));
    free(bytes);
    free(paged);
    free(expected);
    free(original.bytes);
}

static void ogg_pictures_decode_to_the_same_image_as_flac(void)
{
    static const char *const NAMES[] = {"tagged.ogg", "../opus/tagged.opus"};
    uint32_t expected_length = 0;
    bool front = false;
    uint8_t *expected = picture_bytes("tagged.flac", &expected_length, &front);
    CHECK(front);
    for (size_t index = 0; index < sizeof NAMES / sizeof NAMES[0]; index++) {
        uint32_t length = 0;
        uint8_t *bytes = picture_bytes(NAMES[index], &length, &front);
        CHECK(front);
        CHECK_EQUAL(expected_length, length);
        CHECK_EQUAL(0, memcmp(expected, bytes, length));
        free(bytes);
    }
    free(expected);
}

static void read_whole_picture(ByteSource source, uint32_t size, TrackTags *tags)
{
    static TagPictureReader reader;
    static uint8_t buffer[4096];
    ByteSource bytes;
    if (!tags_open_picture(&reader, source, size, &tags->picture, &bytes)) {
        return;
    }
    for (uint32_t done = 0; done < tags->picture.length; done += sizeof buffer) {
        uint32_t left = tags->picture.length - done;
        uint32_t chunk = left < sizeof buffer ? left : sizeof buffer;
        if (!bytes.read(bytes.context, tags->picture.offset + done, buffer, chunk)) {
            return;
        }
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

static void odd_spaces_become_plain_spaces_and_ends_are_trimmed(void)
{
    static const char TEXT[] = "\xe2\x80\x83"
                               "A\xe2\x80\x82"
                               "B\xe2\x80\xaf"
                               "C\xe2\x81\x9f"
                               "D\xe2\x80\x8a"
                               "E\xe2\x80\xa8"
                               "F\xe2\x80\x89";
    char out[TRACK_TAG_TEXT_CAPACITY];
    tag_decode(out, (const uint8_t *)TEXT, sizeof TEXT - 1, TAG_UTF8);
    CHECK_EQUAL(0, strcmp("A B C D E F", out));
    static const uint8_t WIDE[] = {0x00, 0x20, 0x00, 'x', 0x20, 0x27, 0x00, 'y', 0x30, 0x00};
    tag_decode(out, WIDE, sizeof WIDE, TAG_UTF16BE);
    CHECK_EQUAL(0, strcmp("x\xe2\x80\xa7y", out));
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
            read_whole_picture(memory_source(&damaged), fixture.size, &tags);
        }
        free(copy);
        free(fixture.bytes);
    }
}

static void album_versions_come_from_every_tag_kind_that_has_one(void)
{
    static const char *const NAMES[] = {
        "../scan_music/Artist B/Old (2019)/07 - Oldie.mp3",
        "../scan_music/Artist B/Old (2019)/08 - Newer.flac",
        "../scan_music/Artist B/Old (2019)/09 - Newest.m4a",
    };
    static TrackTags tags;
    for (size_t index = 0; index < sizeof NAMES / sizeof NAMES[0]; index++) {
        CHECK(read_fixture(NAMES[index], &tags));
        CHECK_EQUAL(0, strcmp("Old Album", tags.text[TAG_ALBUM]));
        CHECK_EQUAL(0, strcmp("2019 Remix", tags.text[TAG_ALBUM_VERSION]));
    }
    CHECK(read_fixture("../scan_music/Artist B/Old (2019)/10 - Last.ape", &tags));
    CHECK_EQUAL(0, strcmp("Live", tags.text[TAG_ALBUM_VERSION]));
    CHECK(read_fixture("id3v24.mp3", &tags));
    CHECK_EQUAL(0, strcmp("", tags.text[TAG_ALBUM_VERSION]));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_tagged_format_reads_its_fields_duration_and_cover),
        TEST_CASE(ogg_pictures_decode_to_the_same_image_as_flac),
        TEST_CASE(ogg_comments_and_pictures_read_across_small_pages),
        TEST_CASE(an_mp3_with_only_id3v1_and_no_xing_header_still_reads),
        TEST_CASE(an_untagged_flac_has_empty_fields_and_its_duration),
        TEST_CASE(odd_spaces_become_plain_spaces_and_ends_are_trimmed),
        TEST_CASE(formats_come_from_the_extension_in_any_case),
        TEST_CASE(a_truncated_file_fails_cleanly),
        TEST_CASE(damaged_files_never_read_outside_the_file),
        TEST_CASE(album_versions_come_from_every_tag_kind_that_has_one),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
