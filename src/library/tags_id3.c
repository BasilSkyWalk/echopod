#include <stddef.h>

#include "tags_internal.h"

enum {
    HEADER_SIZE = 10,
    FRAME_CAPACITY = 1024,
    V1_SIZE = 128,
    V1_TEXT = 30,
    V1_YEAR = 4,
    V1_COMMENT = 29,
    FLAG_UNSYNC = 0x80,
    FLAG_EXTENDED = 0x40,
    FLAG_FOOTER = 0x10,
    V23_COMPRESS = 0x80,
    V23_ENCRYPT = 0x40,
    V23_GROUP = 0x20,
    V24_GROUP = 0x40,
    V24_COMPRESS = 0x08,
    V24_ENCRYPT = 0x04,
    V24_UNSYNC = 0x02,
    V24_DATA_LENGTH = 0x01,
    FRONT_COVER = 3,
    ENCODING_LATIN1 = 0,
    ENCODING_UTF16 = 1,
    ENCODING_UTF16BE = 2,
    GENRE_TEXT_CAPACITY = 64,
    NO_GENRE = 255
};

typedef enum FrameKind {
    FRAME_OTHER,
    FRAME_TEXT,
    FRAME_GENRE,
    FRAME_YEAR,
    FRAME_PICTURE,
    FRAME_OLD_PICTURE
} FrameKind;

typedef struct FrameName {
    const char *v24;
    const char *v22;
    FrameKind kind;
    TagField field;
} FrameName;

static const FrameName FRAMES[] = {
    {"TIT2", "TT2", FRAME_TEXT, TAG_TITLE},        {"TPE1", "TP1", FRAME_TEXT, TAG_ARTIST},
    {"TPE2", "TP2", FRAME_TEXT, TAG_ALBUM_ARTIST}, {"TALB", "TAL", FRAME_TEXT, TAG_ALBUM},
    {"TCON", "TCO", FRAME_GENRE, TAG_GENRE},       {"TDRC", "", FRAME_TEXT, TAG_DATE},
    {"TYER", "TYE", FRAME_YEAR, TAG_DATE},         {"TPOS", "TPA", FRAME_TEXT, TAG_DISC},
    {"TRCK", "TRK", FRAME_TEXT, TAG_TRACK},        {"APIC", "", FRAME_PICTURE, TAG_TITLE},
    {"", "PIC", FRAME_OLD_PICTURE, TAG_TITLE},
};

typedef struct Id3Tag {
    uint32_t start;
    uint32_t end;
    uint8_t version;
    bool unsync;
    char year[TRACK_TAG_TEXT_CAPACITY];
} Id3Tag;

typedef struct Frame {
    char id[5];
    uint32_t offset;
    uint32_t length;
    bool unsync;
} Frame;

static uint32_t synchsafe(const uint8_t *bytes)
{
    return (uint32_t)(bytes[0] & 0x7F) << 21 | (uint32_t)(bytes[1] & 0x7F) << 14 |
           (uint32_t)(bytes[2] & 0x7F) << 7 | (uint32_t)(bytes[3] & 0x7F);
}

static bool synchsafe_bytes(const uint8_t *bytes)
{
    return ((bytes[0] | bytes[1] | bytes[2] | bytes[3]) & 0x80) == 0;
}

static const FrameName *frame_named(const char *id, uint8_t version)
{
    for (size_t index = 0; index < sizeof FRAMES / sizeof FRAMES[0]; index++) {
        const char *name = version == 2 ? FRAMES[index].v22 : FRAMES[index].v24;
        if (name[0] != '\0' && tag_same_text(id, version == 2 ? 3 : 4, name)) {
            return &FRAMES[index];
        }
    }
    return NULL;
}

static uint32_t remove_unsync(uint8_t *data, uint32_t length)
{
    uint32_t kept = 0;
    for (uint32_t index = 0; index < length; index++) {
        data[kept++] = data[index];
        if (data[index] == 0xFF && index + 1 < length && data[index + 1] == 0x00) {
            index++;
        }
    }
    return kept;
}

static uint32_t terminator_width(uint8_t encoding)
{
    return encoding == ENCODING_UTF16 || encoding == ENCODING_UTF16BE ? 2 : 1;
}

static uint32_t value_end(const uint8_t *data, uint32_t start, uint32_t length, uint32_t width)
{
    uint32_t index = start;
    while (index + width <= length) {
        if (data[index] == 0 && (width == 1 || data[index + 1] == 0)) {
            return index;
        }
        index += width;
    }
    return length;
}

static TagEncoding encoding_of(uint8_t encoding, const uint8_t **text, uint32_t *length)
{
    if (encoding == ENCODING_LATIN1) {
        return TAG_LATIN1;
    }
    if (encoding == ENCODING_UTF16BE) {
        return TAG_UTF16BE;
    }
    if (encoding != ENCODING_UTF16) {
        return TAG_UTF8;
    }
    bool big = *length >= 2 && (*text)[0] == 0xFE && (*text)[1] == 0xFF;
    if (*length >= 2 && (big || ((*text)[0] == 0xFF && (*text)[1] == 0xFE))) {
        *text += 2;
        *length -= 2;
    }
    return big ? TAG_UTF16BE : TAG_UTF16LE;
}

static void decode_value(char *out, const uint8_t *text, uint32_t length, uint8_t encoding)
{
    TagEncoding decoded = encoding_of(encoding, &text, &length);
    tag_decode(out, text, length, decoded);
}

typedef void (*ValueSink)(TagReader *reader, TagField field, const char *value, Id3Tag *tag);

static void for_each_value(TagReader *reader, TagField field, const uint8_t *data, uint32_t length,
                           ValueSink sink, Id3Tag *tag)
{
    tag_mark_present(reader, field);
    if (length == 0) {
        return;
    }
    uint8_t encoding = data[0];
    uint32_t width = terminator_width(encoding);
    uint32_t start = 1;
    char value[TRACK_TAG_TEXT_CAPACITY];
    while (start < length) {
        uint32_t end = value_end(data, start, length, width);
        decode_value(value, data + start, end - start, encoding);
        sink(reader, field, value, tag);
        start = end + width;
    }
}

static void offer_value(TagReader *reader, TagField field, const char *value, Id3Tag *tag)
{
    (void)tag;
    if (value[0] != '\0') {
        tag_offer_decoded(reader, field, value);
    }
}

static bool all_digits(const char *text, uint32_t length)
{
    if (length == 0) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

static uint32_t number_of(const char *text, uint32_t length)
{
    uint32_t value = 0;
    for (uint32_t index = 0; index < length && value < 1000; index++) {
        value = value * 10 + (uint32_t)(text[index] - '0');
    }
    return value;
}

static const char *genre_for_id(const char *id, uint32_t length)
{
    if (all_digits(id, length)) {
        return tags_id3_genre(number_of(id, length));
    }
    if (tag_same_text(id, length, "CR")) {
        return "Cover";
    }
    return tag_same_text(id, length, "RX") ? "Remix" : "Unknown";
}

static uint32_t length_of(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void offer_text(TagReader *reader, const char *text)
{
    if (text[0] != '\0') {
        tag_offer_decoded(reader, TAG_GENRE, text);
    }
}

static bool leading_id(const char *text, uint32_t length, uint32_t *id_length)
{
    if (length < 3 || text[0] != '(') {
        return false;
    }
    uint32_t index = 1;
    while (index < length && text[index] != ')') {
        index++;
    }
    if (index >= length) {
        return false;
    }
    *id_length = index - 1;
    return all_digits(text + 1, *id_length) || tag_same_text(text + 1, *id_length, "CR") ||
           tag_same_text(text + 1, *id_length, "RX");
}

static void offer_genre_text(TagReader *reader, const char *text, uint32_t length)
{
    uint32_t id_length = 0;
    if (all_digits(text, length) && number_of(text, length) < 256) {
        offer_text(reader, tags_id3_genre(number_of(text, length)));
    } else if (tag_same_text(text, length, "CR") || tag_same_text(text, length, "RX")) {
        offer_text(reader, genre_for_id(text, length));
    } else if (leading_id(text, length, &id_length)) {
        offer_text(reader, genre_for_id(text + 1, id_length));
    } else {
        bool escaped = length >= 2 && text[0] == '(' && text[1] == '(';
        offer_text(reader, text + (escaped ? 1 : 0));
    }
}

static void offer_genre(TagReader *reader, TagField field, const char *value, Id3Tag *tag)
{
    (void)field;
    (void)tag;
    if (value[0] != '\0') {
        offer_genre_text(reader, value, length_of(value));
    }
}

static void keep_year(TagReader *reader, TagField field, const char *value, Id3Tag *tag)
{
    (void)reader;
    (void)field;
    if (tag->year[0] == '\0') {
        for (uint32_t index = 0; index < TRACK_TAG_TEXT_CAPACITY; index++) {
            tag->year[index] = value[index];
            if (value[index] == '\0') {
                break;
            }
        }
    }
}

static void read_text_frame(TagReader *reader, const Frame *frame, const FrameName *name,
                            Id3Tag *tag)
{
    uint8_t data[FRAME_CAPACITY];
    uint32_t length = frame->length < FRAME_CAPACITY ? frame->length : FRAME_CAPACITY;
    if (!tag_read(reader, frame->offset, data, length)) {
        return;
    }
    if (frame->unsync) {
        length = remove_unsync(data, length);
    }
    ValueSink sink = name->kind == FRAME_GENRE  ? offer_genre
                     : name->kind == FRAME_YEAR ? keep_year
                                                : offer_value;
    for_each_value(reader, name->field, data, length, sink, tag);
}

static uint32_t skip_value(const TagReader *reader, uint32_t offset, uint32_t end, uint32_t width)
{
    uint8_t unit[2];
    while (offset + width <= end && tag_read(reader, offset, unit, width)) {
        offset += width;
        if (unit[0] == 0 && (width == 1 || unit[1] == 0)) {
            return offset;
        }
    }
    return end;
}

static void read_picture_frame(TagReader *reader, const Frame *frame, bool old_style)
{
    uint8_t head[1];
    uint32_t end = frame->offset + frame->length;
    if (frame->unsync || !tag_read(reader, frame->offset, head, 1)) {
        return;
    }
    uint32_t width = terminator_width(head[0]);
    uint32_t cursor = old_style ? frame->offset + 4 : skip_value(reader, frame->offset + 1, end, 1);
    uint8_t type[1];
    if (cursor >= end || !tag_read(reader, cursor, type, 1)) {
        return;
    }
    cursor = skip_value(reader, cursor + 1, end, width);
    if (cursor < end) {
        tag_offer_picture(reader, (TagPicture){.kind = TAG_PICTURE_RAW,
                                               .front = type[0] == FRONT_COVER,
                                               .offset = cursor,
                                               .length = end - cursor});
    }
}

static void read_frame(TagReader *reader, const Frame *frame, Id3Tag *tag)
{
    const FrameName *name = frame_named(frame->id, tag->version);
    if (name == NULL) {
        return;
    }
    if (name->kind == FRAME_PICTURE || name->kind == FRAME_OLD_PICTURE) {
        read_picture_frame(reader, frame, name->kind == FRAME_OLD_PICTURE);
    } else {
        read_text_frame(reader, frame, name, tag);
    }
}

static uint32_t frame_size_of(const Id3Tag *tag, const uint8_t *header)
{
    if (tag->version == 2) {
        return (uint32_t)header[3] << 16 | (uint32_t)header[4] << 8 | header[5];
    }
    if (tag->version == 4 && synchsafe_bytes(header + 4)) {
        return synchsafe(header + 4);
    }
    return tag_be32(header + 4);
}

static bool usable_frame(const Id3Tag *tag, const uint8_t *header, Frame *frame)
{
    uint8_t flags = tag->version == 2 ? 0 : header[9];
    if (tag->version == 3) {
        if ((flags & (V23_COMPRESS | V23_ENCRYPT)) != 0) {
            return false;
        }
        frame->offset += (flags & V23_GROUP) != 0 ? 1u : 0u;
    } else if (tag->version == 4) {
        if ((flags & (V24_COMPRESS | V24_ENCRYPT)) != 0) {
            return false;
        }
        frame->offset +=
            ((flags & V24_GROUP) != 0 ? 1u : 0u) + ((flags & V24_DATA_LENGTH) != 0 ? 4u : 0u);
        frame->unsync = (flags & V24_UNSYNC) != 0 || tag->unsync;
    }
    return true;
}

static void read_frames(TagReader *reader, Id3Tag *tag, uint32_t cursor)
{
    uint32_t header_size = tag->version == 2 ? 6 : HEADER_SIZE;
    uint8_t header[HEADER_SIZE];
    while (cursor + header_size <= tag->end && tag_read(reader, cursor, header, header_size)) {
        if (header[0] == 0) {
            return;
        }
        uint32_t size = frame_size_of(tag, header);
        uint32_t data = cursor + header_size;
        if (size > tag->end - data) {
            return;
        }
        Frame frame = {.offset = data, .length = size, .unsync = false};
        for (uint32_t index = 0; index < 4; index++) {
            frame.id[index] = index < (tag->version == 2 ? 3u : 4u) ? (char)header[index] : '\0';
        }
        frame.id[4] = '\0';
        if (usable_frame(tag, header, &frame) && frame.offset <= data + size) {
            frame.length = size - (frame.offset - data);
            read_frame(reader, &frame, tag);
        }
        cursor = data + size;
    }
}

static uint32_t extended_header_size(const TagReader *reader, const Id3Tag *tag, uint32_t offset)
{
    uint8_t size[4];
    if (!tag_read(reader, offset, size, 4)) {
        return 0;
    }
    return tag->version == 4 ? synchsafe(size) : tag_be32(size) + 4;
}

bool tags_read_id3v2(TagReader *reader, uint32_t offset, uint32_t *tag_size)
{
    uint8_t header[HEADER_SIZE];
    if (!tag_read(reader, offset, header, sizeof header) ||
        !tag_same_text((const char *)header, 3, "ID3") || header[3] < 2 || header[3] > 4) {
        return false;
    }
    Id3Tag tag = {.start = offset, .version = header[3], .unsync = (header[5] & FLAG_UNSYNC) != 0};
    uint32_t size = synchsafe(header + 6);
    *tag_size = HEADER_SIZE + size + ((header[5] & FLAG_FOOTER) != 0 ? HEADER_SIZE : 0);
    tag.end = offset + HEADER_SIZE + size;
    if (tag.end > reader->size) {
        tag.end = reader->size;
    }
    uint32_t cursor = offset + HEADER_SIZE;
    if (tag.version >= 3 && (header[5] & FLAG_EXTENDED) != 0) {
        cursor += extended_header_size(reader, &tag, cursor);
    }
    tag.unsync = tag.version == 4 && tag.unsync;
    read_frames(reader, &tag, cursor);
    if (reader->tags->text[TAG_DATE][0] == '\0' && tag.year[0] != '\0') {
        tag_offer_decoded(reader, TAG_DATE, tag.year);
    }
    return true;
}

static uint32_t v1_text_length(const uint8_t *text, uint32_t capacity)
{
    uint32_t length = 0;
    while (length < capacity && text[length] != 0) {
        length++;
    }
    return length;
}

static void offer_v1(TagReader *reader, TagField field, const uint8_t *text, uint32_t capacity)
{
    if (!reader->present[field]) {
        tag_offer(reader, field, text, v1_text_length(text, capacity), TAG_LATIN1);
        reader->present[field] = reader->tags->text[field][0] != '\0';
    }
}

static void offer_v1_track(TagReader *reader, const uint8_t *tag)
{
    uint8_t track = tag[126];
    if (reader->present[TAG_TRACK] || track == 0 || (track == ' ' && tag[125] != 0)) {
        return;
    }
    char text[4];
    uint32_t length = 0;
    if (track >= 100) {
        text[length++] = (char)('0' + track / 100);
    }
    if (track >= 10) {
        text[length++] = (char)('0' + track / 10 % 10);
    }
    text[length++] = (char)('0' + track % 10);
    tag_offer(reader, TAG_TRACK, (const uint8_t *)text, length, TAG_UTF8);
}

void tags_read_id3v1(TagReader *reader)
{
    uint8_t tag[V1_SIZE + 3];
    if (reader->size < V1_SIZE) {
        return;
    }
    uint32_t start = reader->size - V1_SIZE;
    if (!tag_read(reader, start, tag + 3, V1_SIZE) ||
        !tag_same_text((const char *)tag + 3, 3, "TAG")) {
        return;
    }
    if (start >= 3 && tag_read(reader, start - 3, tag, 3) &&
        tag_same_text((const char *)tag, 3, "APE")) {
        return;
    }
    const uint8_t *body = tag + 3;
    offer_v1(reader, TAG_TITLE, body + 3, V1_TEXT);
    offer_v1(reader, TAG_ARTIST, body + 33, V1_TEXT);
    offer_v1(reader, TAG_ALBUM, body + 63, V1_TEXT);
    offer_v1(reader, TAG_DATE, body + 93, V1_YEAR);
    offer_v1_track(reader, body);
    if (!reader->present[TAG_GENRE] && body[127] != NO_GENRE) {
        offer_text(reader, tags_id3_genre(body[127]));
    }
}
