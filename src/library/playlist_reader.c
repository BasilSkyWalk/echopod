#include "library/playlist_reader.h"

enum {
    REPLACEMENT = 0xFFFD,
    LINE_SEPARATOR = 0x2028,
    PARAGRAPH_SEPARATOR = 0x2029,
    NEXT_LINE = 0x85
};

static const uint16_t WINDOWS_1252[32] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178,
};

static bool byte_at(PlaylistReader *reader, uint32_t offset, uint8_t *value)
{
    if (offset >= reader->size) {
        return false;
    }
    if (offset < reader->chunk_start || offset >= reader->chunk_start + reader->chunk_length) {
        uint32_t length =
            reader->size - offset < PLAYLIST_CHUNK ? reader->size - offset : PLAYLIST_CHUNK;
        if (!reader->source.read(reader->source.context, offset, reader->chunk, length)) {
            return false;
        }
        reader->chunk_start = offset;
        reader->chunk_length = length;
    }
    *value = reader->chunk[offset - reader->chunk_start];
    return true;
}

static uint32_t sequence_length(uint8_t lead)
{
    if (lead < 0x80) {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return 2;
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return 3;
    }
    return lead >= 0xF0 && lead <= 0xF4 ? 4 : 0;
}

static bool second_byte_valid(uint8_t lead, uint8_t second)
{
    if (lead == 0xE0) {
        return second >= 0xA0 && second <= 0xBF;
    }
    if (lead == 0xED) {
        return second >= 0x80 && second <= 0x9F;
    }
    if (lead == 0xF0) {
        return second >= 0x90 && second <= 0xBF;
    }
    if (lead == 0xF4) {
        return second >= 0x80 && second <= 0x8F;
    }
    return second >= 0x80 && second <= 0xBF;
}

static bool valid_sequence(PlaylistReader *reader, uint32_t offset, uint8_t lead, uint32_t length)
{
    for (uint32_t index = 1; index < length; index++) {
        uint8_t next = 0;
        if (!byte_at(reader, offset + index, &next)) {
            return false;
        }
        bool valid = index == 1 ? second_byte_valid(lead, next) : next >= 0x80 && next <= 0xBF;
        if (!valid) {
            return false;
        }
    }
    return true;
}

static bool is_utf8(PlaylistReader *reader)
{
    uint32_t offset = 0;
    uint8_t lead = 0;
    while (byte_at(reader, offset, &lead)) {
        uint32_t length = sequence_length(lead);
        if (length == 0 || !valid_sequence(reader, offset, lead, length)) {
            return false;
        }
        offset += length;
    }
    return true;
}

static bool has_bom(PlaylistReader *reader)
{
    uint8_t bytes[3] = {0, 0, 0};
    for (uint32_t index = 0; index < 3; index++) {
        if (!byte_at(reader, index, &bytes[index])) {
            return false;
        }
    }
    return bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF;
}

bool playlist_reader_open(PlaylistReader *reader, ByteSource source, uint32_t size)
{
    *reader = (PlaylistReader){
        .source = source, .size = size, .offset = 0, .chunk_start = 0, .chunk_length = 0};
    reader->utf8 = is_utf8(reader);
    if (reader->utf8 && has_bom(reader)) {
        reader->offset = 3;
    }
    return true;
}

static uint32_t next_code_point(PlaylistReader *reader)
{
    uint8_t lead = 0;
    byte_at(reader, reader->offset++, &lead);
    if (!reader->utf8) {
        return lead >= 0x80 && lead < 0xA0 ? WINDOWS_1252[lead - 0x80] : lead;
    }
    uint32_t length = sequence_length(lead);
    uint32_t code_point = length == 1 ? lead : lead & (0x7Fu >> length);
    for (uint32_t index = 1; index < length; index++) {
        uint8_t next = 0;
        byte_at(reader, reader->offset++, &next);
        code_point = code_point << 6 | (next & 0x3Fu);
    }
    return code_point;
}

static bool is_line_break(uint32_t code_point, bool utf8)
{
    if (code_point == '\n' || code_point == '\r' || code_point == 0x0B || code_point == 0x0C ||
        (code_point >= 0x1C && code_point <= 0x1E)) {
        return true;
    }
    return utf8 && (code_point == NEXT_LINE || code_point == LINE_SEPARATOR ||
                    code_point == PARAGRAPH_SEPARATOR);
}

static bool is_space(uint32_t code_point)
{
    return code_point == ' ' || (code_point >= 0x09 && code_point <= 0x0D) ||
           (code_point >= 0x1C && code_point <= 0x1F) || code_point == 0x85 || code_point == 0xA0 ||
           code_point == 0x1680 || (code_point >= 0x2000 && code_point <= 0x200A) ||
           code_point == 0x2028 || code_point == 0x2029 || code_point == 0x202F ||
           code_point == 0x205F || code_point == 0x3000;
}

typedef struct Line {
    char *text;
    uint32_t capacity;
    uint32_t length;
    uint32_t content_end;
} Line;

static void put(Line *line, uint32_t code_point)
{
    uint8_t bytes[4];
    uint32_t size = code_point < 0x80 ? 1 : code_point < 0x800 ? 2 : code_point < 0x10000 ? 3 : 4;
    uint32_t value = code_point;
    for (uint32_t index = size - 1; index > 0; index--) {
        bytes[index] = (uint8_t)(0x80 | (value & 0x3F));
        value >>= 6;
    }
    static const uint8_t LEADS[5] = {0, 0x00, 0xC0, 0xE0, 0xF0};
    bytes[0] = (uint8_t)(LEADS[size] | value);
    if (line->length + size >= line->capacity || (line->length == 0 && is_space(code_point))) {
        return;
    }
    for (uint32_t index = 0; index < size; index++) {
        line->text[line->length++] = (char)bytes[index];
    }
    if (!is_space(code_point)) {
        line->content_end = line->length;
    }
}

static bool read_line(PlaylistReader *reader, Line *line)
{
    if (reader->offset >= reader->size) {
        return false;
    }
    line->length = 0;
    line->content_end = 0;
    while (reader->offset < reader->size) {
        uint32_t code_point = next_code_point(reader);
        if (is_line_break(code_point, reader->utf8)) {
            uint8_t next = 0;
            if (code_point == '\r' && byte_at(reader, reader->offset, &next) && next == '\n') {
                reader->offset++;
            }
            break;
        }
        put(line, code_point);
    }
    line->text[line->content_end] = '\0';
    return true;
}

bool playlist_reader_next(PlaylistReader *reader, char *entry, uint32_t capacity)
{
    Line line = {.text = entry, .capacity = capacity, .length = 0, .content_end = 0};
    while (read_line(reader, &line)) {
        if (line.content_end > 0 && entry[0] != '#') {
            return true;
        }
    }
    return false;
}
