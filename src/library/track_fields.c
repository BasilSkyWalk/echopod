#include "library/track_fields.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    MAX_SMALL = 255,
    NUMBER_CEILING = 1000000,
    YEAR_DIGITS = 4
};

static const char UNKNOWN_ARTIST[] = "Unknown Artist";
static const char UNKNOWN_ALBUM[] = "Unknown Album";
static const char UNKNOWN_GENRE[] = "Unknown Genre";

static bool is_digit(char character)
{
    return character >= '0' && character <= '9';
}

static size_t space_length(const char *text)
{
    if (*text == ' ' || (*text >= '\t' && *text <= '\r') || (*text >= 0x1C && *text <= 0x1F)) {
        return 1;
    }
    return (unsigned char)text[0] == 0xC2 && (unsigned char)text[1] == 0xA0 ? 2 : 0;
}

static const char *skip_spaces(const char *text)
{
    for (size_t length = space_length(text); length > 0; length = space_length(text)) {
        text += length;
    }
    return text;
}

static const char *read_digits(const char *text, uint32_t *value)
{
    *value = 0;
    while (is_digit(*text)) {
        *value = *value < NUMBER_CEILING ? *value * 10 + (uint32_t)(*text - '0') : *value;
        text++;
    }
    return text;
}

uint32_t track_fields_number(const char *text)
{
    uint32_t value = 0;
    read_digits(skip_spaces(text), &value);
    return value;
}

uint32_t track_fields_year(const char *text)
{
    text = skip_spaces(text);
    uint32_t year = 0;
    for (int digit = 0; digit < YEAR_DIGITS; digit++) {
        if (!is_digit(text[digit])) {
            return 0;
        }
        year = year * 10 + (uint32_t)(text[digit] - '0');
    }
    return year;
}

typedef struct FilePrefix {
    size_t length;
    uint32_t disc;
    uint32_t number;
} FilePrefix;

static bool disc_track_prefix(const char *stem, FilePrefix *prefix)
{
    const char *cursor = read_digits(stem, &prefix->disc);
    if (cursor == stem || *cursor != '-') {
        return false;
    }
    const char *number = cursor + 1;
    cursor = read_digits(number, &prefix->number);
    if (cursor == number) {
        return false;
    }
    cursor = skip_spaces(cursor);
    if (*cursor != '-') {
        return false;
    }
    prefix->length = (size_t)(skip_spaces(cursor + 1) - stem);
    return true;
}

static bool track_prefix(const char *stem, FilePrefix *prefix)
{
    const char *cursor = read_digits(stem, &prefix->number);
    if (cursor == stem) {
        return false;
    }
    cursor = skip_spaces(cursor);
    if (*cursor != '-' && *cursor != '.') {
        return false;
    }
    prefix->disc = 0;
    prefix->length = (size_t)(skip_spaces(cursor + 1) - stem);
    return true;
}

static FilePrefix prefix_of(const char *stem)
{
    FilePrefix prefix = {.length = 0, .disc = 0, .number = 0};
    if (disc_track_prefix(stem, &prefix) || track_prefix(stem, &prefix)) {
        return prefix;
    }
    return (FilePrefix){.length = 0, .disc = 0, .number = 0};
}

static void copy_text(char *out, const char *text, size_t length)
{
    size_t limit = length < TRACK_FIELD_TEXT_CAPACITY - 1 ? length : TRACK_FIELD_TEXT_CAPACITY - 1;
    for (size_t index = 0; index < limit; index++) {
        out[index] = text[index];
    }
    out[limit] = '\0';
}

static const char *last_component(const char *path, size_t *length, const char **parent,
                                  size_t *parent_length)
{
    const char *name = path;
    const char *previous = path;
    const char *previous_end = path;
    for (const char *cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            previous = name;
            previous_end = cursor;
            name = cursor + 1;
        }
    }
    *parent = previous;
    *parent_length = (size_t)(previous_end - previous);
    size_t full = 0;
    while (name[full] != '\0') {
        full++;
    }
    *length = full;
    return name;
}

static size_t stem_length(const char *name, size_t length)
{
    size_t dot = length;
    for (size_t index = 0; index < length; index++) {
        dot = name[index] == '.' ? index : dot;
    }
    return dot > 0 && dot + 1 < length ? dot : length;
}

static const char *chosen(const char *text, const char *fallback)
{
    return text[0] != '\0' ? text : fallback;
}

static uint8_t capped(uint32_t value)
{
    return (uint8_t)(value < MAX_SMALL ? value : MAX_SMALL);
}

static void fill_names(const TrackTags *tags, BuilderTrack *track)
{
    const char *artist = tags->text[TAG_ARTIST];
    const char *album_artist = tags->text[TAG_ALBUM_ARTIST];
    track->artist = chosen(artist, chosen(album_artist, UNKNOWN_ARTIST));
    track->album_artist = chosen(album_artist, chosen(artist, UNKNOWN_ARTIST));
    track->album_artist_guessed = album_artist[0] == 0;
    track->genre = chosen(tags->text[TAG_GENRE], UNKNOWN_GENRE);
}

void track_fields_from_tags(const char *path, const TrackTags *tags, TrackFields *fields)
{
    size_t name_length = 0;
    const char *parent = NULL;
    size_t parent_length = 0;
    const char *name = last_component(path, &name_length, &parent, &parent_length);
    size_t stem = stem_length(name, name_length);
    copy_text(fields->title, name, stem);
    FilePrefix prefix = prefix_of(fields->title);
    copy_text(fields->title, name + prefix.length, stem - prefix.length);
    copy_text(fields->album, parent, parent_length);
    BuilderTrack *track = &fields->track;
    track->path = path;
    track->title = chosen(tags->text[TAG_TITLE], fields->title);
    track->album = chosen(tags->text[TAG_ALBUM], chosen(fields->album, UNKNOWN_ALBUM));
    fill_names(tags, track);
    track->year = (uint16_t)track_fields_year(tags->text[TAG_DATE]);
    uint32_t disc = track_fields_number(tags->text[TAG_DISC]);
    uint32_t number = track_fields_number(tags->text[TAG_TRACK]);
    track->disc = capped(disc != 0 ? disc : prefix.disc != 0 ? prefix.disc : 1);
    track->number = capped(number != 0 ? number : prefix.number);
    track->duration_ms = tags->duration_ms;
}
