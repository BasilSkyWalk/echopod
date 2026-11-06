#include "playback/lyrics.h"

#include <stddef.h>

enum {
    MAX_MINUTES = 6000,
    NUMBER_CEILING = 100000000,
    BOM_LENGTH = 3
};

static const char OFFSET_NAME[] = "offset:";

static bool is_digit(char character)
{
    return character >= '0' && character <= '9';
}

static char lower(char character)
{
    return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static bool is_letter(char character)
{
    return lower(character) >= 'a' && lower(character) <= 'z';
}

static bool is_blank(char character)
{
    return character == ' ' || character == '\t' || character == '\r';
}

void lyrics_start(Lyrics *lyrics)
{
    lyrics->count = 0;
    lyrics->text_used = 0;
    lyrics->pending_length = 0;
    lyrics->offset_ms = 0;
    lyrics->synced = false;
    lyrics->cut = false;
    lyrics->first_line = true;
}

static uint32_t read_number(const char *text, size_t *index, size_t end, size_t *digits)
{
    uint32_t value = 0;
    size_t start = *index;
    while (*index < end && is_digit(text[*index])) {
        value = value < NUMBER_CEILING ? value * 10 + (uint32_t)(text[*index] - '0') : value;
        (*index)++;
    }
    *digits = *index - start;
    return value;
}

static bool parse_time(const char *tag, size_t length, uint32_t *time_ms)
{
    size_t index = 0;
    size_t digits = 0;
    uint32_t minutes = read_number(tag, &index, length, &digits);
    if (digits == 0 || index >= length || tag[index] != ':' || minutes > MAX_MINUTES) {
        return false;
    }
    index++;
    uint32_t seconds = read_number(tag, &index, length, &digits);
    if (digits == 0 || seconds >= 60) {
        return false;
    }
    uint32_t fraction_ms = 0;
    if (index < length && (tag[index] == '.' || tag[index] == ':')) {
        index++;
        size_t start = index;
        for (uint32_t scale = 100; index < length && is_digit(tag[index]); index++) {
            fraction_ms += (uint32_t)(tag[index] - '0') * scale;
            scale /= 10;
        }
        if (index == start) {
            return false;
        }
    }
    *time_ms = (minutes * 60 + seconds) * 1000 + fraction_ms;
    return index == length;
}

static bool parse_offset(const char *tag, size_t length, int32_t *offset_ms)
{
    size_t name_length = sizeof OFFSET_NAME - 1;
    if (length <= name_length) {
        return false;
    }
    for (size_t index = 0; index < name_length; index++) {
        if (lower(tag[index]) != OFFSET_NAME[index]) {
            return false;
        }
    }
    size_t index = name_length;
    while (index < length && is_blank(tag[index])) {
        index++;
    }
    bool negative = index < length && tag[index] == '-';
    index += index < length && (tag[index] == '-' || tag[index] == '+') ? 1 : 0;
    size_t digits = 0;
    uint32_t value = read_number(tag, &index, length, &digits);
    if (digits > 0) {
        *offset_ms = negative ? -(int32_t)value : (int32_t)value;
    }
    return true;
}

static bool is_metadata(const char *tag, size_t length)
{
    size_t index = 0;
    while (index < length && is_letter(tag[index])) {
        index++;
    }
    return index > 0 && index < length && tag[index] == ':';
}

static size_t word_time_length(const char *text, size_t length)
{
    size_t close = 1;
    while (close < length && text[close] != '>') {
        close++;
    }
    uint32_t ignored = 0;
    return close < length && parse_time(text + 1, close - 1, &ignored) ? close + 1 : 0;
}

static bool store_text(Lyrics *lyrics, const char *text, size_t length, uint16_t *stored)
{
    size_t used = lyrics->text_used;
    for (size_t index = 0; index < length; index++) {
        size_t skip = text[index] == '<' ? word_time_length(text + index, length - index) : 0;
        if (skip > 0) {
            index += skip - 1;
        } else if (used + 1 < LYRICS_TEXT_CAPACITY) {
            lyrics->text[used++] = text[index];
        } else {
            lyrics->cut = true;
            return false;
        }
    }
    if (used >= LYRICS_TEXT_CAPACITY) {
        lyrics->cut = true;
        return false;
    }
    lyrics->text[used++] = '\0';
    *stored = lyrics->text_used;
    lyrics->text_used = (uint16_t)used;
    return true;
}

static void add_line(Lyrics *lyrics, uint32_t time_ms, uint16_t text, bool timed)
{
    if (lyrics->count == LYRICS_LINE_CAPACITY) {
        lyrics->cut = true;
        return;
    }
    lyrics->lines[lyrics->count++] = (LyricsLine){.time_ms = time_ms, .text = text, .timed = timed};
}

static size_t skip_bom(Lyrics *lyrics, const char *line, size_t length)
{
    bool first = lyrics->first_line;
    lyrics->first_line = false;
    return first && length >= BOM_LENGTH && (uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB &&
                   (uint8_t)line[2] == 0xBF
               ? BOM_LENGTH
               : 0;
}

static size_t read_tags(Lyrics *lyrics, const char *line, size_t start, size_t length,
                        uint32_t *times, int *time_count)
{
    while (start < length && line[start] == '[') {
        size_t close = start + 1;
        while (close < length && line[close] != ']') {
            close++;
        }
        const char *tag = line + start + 1;
        size_t tag_length = close - start - 1;
        uint32_t time_ms = 0;
        if (close >= length) {
            return start;
        }
        if (parse_time(tag, tag_length, &time_ms)) {
            times[*time_count] = time_ms;
            *time_count += *time_count < LYRICS_TIMES_PER_LINE - 1 ? 1 : 0;
        } else if (*time_count == 0 && (parse_offset(tag, tag_length, &lyrics->offset_ms) ||
                                        is_metadata(tag, tag_length))) {
            return length + 1;
        } else {
            return start;
        }
        start = close + 1;
    }
    return start;
}

static void take_line(Lyrics *lyrics, const char *line, size_t length)
{
    size_t start = skip_bom(lyrics, line, length);
    while (length > start && is_blank(line[length - 1])) {
        length--;
    }
    uint32_t times[LYRICS_TIMES_PER_LINE];
    int time_count = 0;
    start = read_tags(lyrics, line, start, length, times, &time_count);
    if (start > length) {
        return;
    }
    while (start < length && is_blank(line[start])) {
        start++;
    }
    uint16_t text = 0;
    if ((time_count == 0 && start == length && lyrics->count == 0) ||
        !store_text(lyrics, line + start, length - start, &text)) {
        return;
    }
    if (time_count == 0) {
        add_line(lyrics, 0, text, false);
    }
    for (int index = 0; index < time_count; index++) {
        add_line(lyrics, times[index], text, true);
    }
}

static size_t whole_characters(const char *text, size_t length)
{
    size_t lead = length;
    while (lead > 0 && ((uint8_t)text[lead - 1] & 0xC0) == 0x80) {
        lead--;
    }
    if (lead == 0 || (uint8_t)text[lead - 1] < 0xC0) {
        return length;
    }
    uint8_t first = (uint8_t)text[lead - 1];
    size_t needed = first >= 0xF0 ? 4 : first >= 0xE0 ? 3 : 2;
    return length - (lead - 1) >= needed ? length : lead - 1;
}

void lyrics_feed(Lyrics *lyrics, const char *bytes, uint32_t length)
{
    for (uint32_t index = 0; index < length && !lyrics->cut; index++) {
        if (bytes[index] == '\n') {
            take_line(lyrics, lyrics->pending,
                      whole_characters(lyrics->pending, lyrics->pending_length));
            lyrics->pending_length = 0;
        } else if (lyrics->pending_length < LYRICS_PENDING_CAPACITY) {
            lyrics->pending[lyrics->pending_length++] = bytes[index];
        }
    }
}

static uint32_t shifted(uint32_t time_ms, int32_t offset_ms)
{
    int64_t moved = (int64_t)time_ms - offset_ms;
    return moved<0 ? 0 : moved>(int64_t) UINT32_MAX ? UINT32_MAX : (uint32_t)moved;
}

void lyrics_finish(Lyrics *lyrics)
{
    if (lyrics->pending_length > 0 && !lyrics->cut) {
        take_line(lyrics, lyrics->pending,
                  whole_characters(lyrics->pending, lyrics->pending_length));
    }
    lyrics->pending_length = 0;
    bool synced = false;
    for (uint16_t index = 0; index < lyrics->count; index++) {
        synced = synced || lyrics->lines[index].timed;
    }
    uint16_t kept = 0;
    for (uint16_t index = 0; index < lyrics->count; index++) {
        LyricsLine line = lyrics->lines[index];
        if (synced && !line.timed) {
            continue;
        }
        line.time_ms = shifted(line.time_ms, lyrics->offset_ms);
        uint16_t at = kept++;
        while (at > 0 && lyrics->lines[at - 1].time_ms > line.time_ms) {
            lyrics->lines[at] = lyrics->lines[at - 1];
            at--;
        }
        lyrics->lines[at] = line;
    }
    while (!synced && kept > 0 && lyrics->text[lyrics->lines[kept - 1].text] == '\0') {
        kept--;
    }
    lyrics->count = kept;
    lyrics->synced = synced;
}

bool lyrics_full(const Lyrics *lyrics)
{
    return lyrics->cut;
}

int lyrics_current(const Lyrics *lyrics, uint32_t position_ms, uint32_t duration_ms)
{
    if (lyrics->count == 0) {
        return LYRICS_NONE;
    }
    if (!lyrics->synced) {
        uint64_t index = duration_ms > 0 ? (uint64_t)position_ms * lyrics->count / duration_ms : 0;
        return index < lyrics->count ? (int)index : lyrics->count - 1;
    }
    int found = LYRICS_NONE;
    for (int index = 0; index < lyrics->count && lyrics->lines[index].time_ms <= position_ms;
         index++) {
        found = index;
    }
    return found;
}

const char *lyrics_line(const Lyrics *lyrics, int index)
{
    return index >= 0 && index < lyrics->count ? lyrics->text + lyrics->lines[index].text : "";
}
