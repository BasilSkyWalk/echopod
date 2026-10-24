#include <string.h>

#include "check.h"
#include "library/track_fields.h"

static TrackTags empty_tags(void)
{
    TrackTags tags;
    memset(&tags, 0, sizeof tags);
    return tags;
}

static void numbers_take_the_leading_integer(void)
{
    CHECK_EQUAL(1u, track_fields_number("1/10"));
    CHECK_EQUAL(7u, track_fields_number("07"));
    CHECK_EQUAL(7u, track_fields_number("  7 of 9"));
    CHECK_EQUAL(0u, track_fields_number(""));
    CHECK_EQUAL(0u, track_fields_number("x"));
}

static void years_take_the_first_four_digits(void)
{
    CHECK_EQUAL(1980u, track_fields_year("1980-08-01"));
    CHECK_EQUAL(2025u, track_fields_year("2025"));
    CHECK_EQUAL(0u, track_fields_year(""));
    CHECK_EQUAL(0u, track_fields_year("19"));
    CHECK_EQUAL(1999u, track_fields_year(" 19990"));
}

static void tags_are_used_when_present(void)
{
    TrackTags tags = empty_tags();
    strcpy(tags.text[TAG_TITLE], "Hells Bells");
    strcpy(tags.text[TAG_ARTIST], "AC/DC");
    strcpy(tags.text[TAG_ALBUM_ARTIST], "AC/DC");
    strcpy(tags.text[TAG_ALBUM], "Back In Black");
    strcpy(tags.text[TAG_GENRE], "Metal");
    strcpy(tags.text[TAG_DATE], "1980-08-01");
    strcpy(tags.text[TAG_DISC], "1");
    strcpy(tags.text[TAG_TRACK], "1/10");
    tags.duration_ms = 312800;
    static TrackFields fields;
    track_fields_from_tags("AC DC/Back In Black/x.flac", &tags, &fields);
    CHECK_EQUAL(0, strcmp("Hells Bells", fields.track.title));
    CHECK_EQUAL(0, strcmp("AC/DC", fields.track.artist));
    CHECK_EQUAL(0, strcmp("AC/DC", fields.track.album_artist));
    CHECK_EQUAL(0, strcmp("Back In Black", fields.track.album));
    CHECK_EQUAL(0, strcmp("Metal", fields.track.genre));
    CHECK_EQUAL(1980, fields.track.year);
    CHECK_EQUAL(1, fields.track.disc);
    CHECK_EQUAL(1, fields.track.number);
    CHECK_EQUAL(312800u, fields.track.duration_ms);
    CHECK_EQUAL(0, strcmp("AC DC/Back In Black/x.flac", fields.track.path));
}

static void missing_tags_fall_back_to_the_path(void)
{
    TrackTags tags = empty_tags();
    static TrackFields fields;
    track_fields_from_tags("Artist/Some Album/1-03 - A Song.flac", &tags, &fields);
    CHECK_EQUAL(0, strcmp("A Song", fields.track.title));
    CHECK_EQUAL(0, strcmp("Some Album", fields.track.album));
    CHECK_EQUAL(0, strcmp("Unknown Artist", fields.track.artist));
    CHECK_EQUAL(0, strcmp("Unknown Artist", fields.track.album_artist));
    CHECK_EQUAL(0, strcmp("Unknown Genre", fields.track.genre));
    CHECK_EQUAL(1, fields.track.disc);
    CHECK_EQUAL(3, fields.track.number);
}

static void artists_fill_in_for_each_other(void)
{
    TrackTags tags = empty_tags();
    static TrackFields fields;
    strcpy(tags.text[TAG_ALBUM_ARTIST], "X");
    track_fields_from_tags("a/b/c.flac", &tags, &fields);
    CHECK_EQUAL(0, strcmp("X", fields.track.artist));
    tags = empty_tags();
    strcpy(tags.text[TAG_ARTIST], "Y");
    track_fields_from_tags("a/b/c.flac", &tags, &fields);
    CHECK_EQUAL(0, strcmp("Y", fields.track.album_artist));
}

static void file_names_give_titles_and_numbers(void)
{
    TrackTags tags = empty_tags();
    static TrackFields fields;
    track_fields_from_tags("x/07 - Shoots.mp3", &tags, &fields);
    CHECK_EQUAL(0, strcmp("Shoots", fields.track.title));
    CHECK_EQUAL(1, fields.track.disc);
    CHECK_EQUAL(7, fields.track.number);
    track_fields_from_tags("x/07.Shoots.flac", &tags, &fields);
    CHECK_EQUAL(0, strcmp("Shoots", fields.track.title));
    track_fields_from_tags("Intro.flac", &tags, &fields);
    CHECK_EQUAL(0, strcmp("Intro", fields.track.title));
    CHECK_EQUAL(0, strcmp("Unknown Album", fields.track.album));
    CHECK_EQUAL(0, fields.track.number);
    track_fields_from_tags("a/2-11 -Last.flac", &tags, &fields);
    CHECK_EQUAL(2, fields.track.disc);
    CHECK_EQUAL(11, fields.track.number);
    CHECK_EQUAL(0, strcmp("Last", fields.track.title));
}

static void numbers_over_255_are_capped(void)
{
    TrackTags tags = empty_tags();
    static TrackFields fields;
    strcpy(tags.text[TAG_TRACK], "99999999999999999999");
    strcpy(tags.text[TAG_DISC], "300");
    track_fields_from_tags("a.flac", &tags, &fields);
    CHECK_EQUAL(255, fields.track.number);
    CHECK_EQUAL(255, fields.track.disc);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(numbers_take_the_leading_integer), TEST_CASE(years_take_the_first_four_digits),
        TEST_CASE(tags_are_used_when_present),       TEST_CASE(missing_tags_fall_back_to_the_path),
        TEST_CASE(artists_fill_in_for_each_other),   TEST_CASE(file_names_give_titles_and_numbers),
        TEST_CASE(numbers_over_255_are_capped),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
