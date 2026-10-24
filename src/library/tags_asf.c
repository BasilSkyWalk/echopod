#include <stddef.h>

#include "tags_internal.h"

enum {
    GUID_SIZE = 16,
    OBJECT_HEADER = 24,
    HEADER_OBJECT_HEADER = 30,
    EXTENSION_HEADER = 22,
    VALUE_CAPACITY = 1024,
    NAME_CAPACITY = 64,
    TYPE_STRING = 0,
    TYPE_BYTES = 1,
    TYPE_BOOL = 2,
    TYPE_DWORD = 3,
    TYPE_QWORD = 4,
    TYPE_WORD = 5,
    FRONT_COVER = 3
};

static const uint8_t HEADER_GUID[GUID_SIZE] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11,
                                               0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
static const uint8_t FILE_PROPERTIES[GUID_SIZE] = {0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11,
                                                   0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
static const uint8_t CONTENT[GUID_SIZE] = {0x33, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11,
                                           0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
static const uint8_t EXTENDED_CONTENT[GUID_SIZE] = {0x40, 0xA4, 0xD0, 0xD2, 0x07, 0xE3, 0xD2, 0x11,
                                                    0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50};
static const uint8_t HEADER_EXTENSION[GUID_SIZE] = {0xB5, 0x03, 0xBF, 0x5F, 0x2E, 0xA9, 0xCF, 0x11,
                                                    0x8E, 0xE3, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
static const uint8_t METADATA[GUID_SIZE] = {0xEA, 0xCB, 0xF8, 0xC5, 0xAF, 0x5B, 0x77, 0x48,
                                            0x84, 0x67, 0xAA, 0x8C, 0x44, 0xFA, 0x4C, 0xCA};
static const uint8_t METADATA_LIBRARY[GUID_SIZE] = {0x94, 0x1C, 0x23, 0x44, 0x98, 0x94, 0xD1, 0x49,
                                                    0xA1, 0x41, 0x1D, 0x13, 0x4E, 0x45, 0x70, 0x54};

typedef struct AsfName {
    const char *name;
    TagField field;
} AsfName;

static const AsfName NAMES[] = {
    {"WM/AlbumArtist", TAG_ALBUM_ARTIST},
    {"WM/AlbumTitle", TAG_ALBUM},
    {"WM/Genre", TAG_GENRE},
    {"WM/Year", TAG_DATE},
    {"WM/PartOfSet", TAG_DISC},
    {"WM/TrackNumber", TAG_TRACK},
};

typedef struct Attribute {
    char name[NAME_CAPACITY];
    uint32_t type;
    uint32_t offset;
    uint32_t length;
} Attribute;

static bool same_guid(const uint8_t *bytes, const uint8_t *guid)
{
    for (int index = 0; index < GUID_SIZE; index++) {
        if (bytes[index] != guid[index]) {
            return false;
        }
    }
    return true;
}

static void read_name(const TagReader *reader, uint32_t offset, uint32_t length, char *name)
{
    uint8_t units[NAME_CAPACITY * 2];
    uint32_t wanted = length < sizeof units ? length : sizeof units;
    name[0] = '\0';
    if (tag_read(reader, offset, units, wanted)) {
        uint32_t count = 0;
        for (uint32_t index = 0; index + 1 < wanted && count + 1 < NAME_CAPACITY; index += 2) {
            uint16_t unit = tag_le16(units + index);
            if (unit == 0) {
                break;
            }
            name[count++] = unit < 0x80 ? (char)unit : '?';
        }
        name[count] = '\0';
    }
}

static uint32_t length_of(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int field_for(const char *name)
{
    for (size_t index = 0; index < sizeof NAMES / sizeof NAMES[0]; index++) {
        if (tag_same_text(name, length_of(name), NAMES[index].name)) {
            return (int)NAMES[index].field;
        }
    }
    return -1;
}

static void offer_number(TagReader *reader, TagField field, uint64_t value)
{
    char digits[24];
    char text[24];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value > 0);
    uint32_t length = 0;
    while (count > 0) {
        text[length++] = digits[--count];
    }
    text[length] = '\0';
    tag_offer_decoded(reader, field, text);
}

static void read_picture(TagReader *reader, const Attribute *attribute)
{
    uint8_t head[5];
    if (attribute->length < 5 || !tag_read(reader, attribute->offset, head, 5)) {
        return;
    }
    uint32_t cursor = attribute->offset + 5;
    uint32_t end = attribute->offset + attribute->length;
    for (int strings = 0; strings < 2; strings++) {
        uint8_t unit[2] = {1, 1};
        while (cursor + 2 <= end && tag_read(reader, cursor, unit, 2)) {
            cursor += 2;
            if (unit[0] == 0 && unit[1] == 0) {
                break;
            }
        }
    }
    uint32_t length = tag_le32(head + 1);
    if (cursor <= end && length <= end - cursor) {
        tag_offer_picture(reader, (TagPicture){.kind = TAG_PICTURE_RAW,
                                               .front = head[0] == FRONT_COVER,
                                               .offset = cursor,
                                               .length = length});
    }
}

static void offer_attribute(TagReader *reader, const Attribute *attribute)
{
    if (tag_same_text(attribute->name, length_of(attribute->name), "WM/Picture")) {
        if (attribute->type == TYPE_BYTES) {
            read_picture(reader, attribute);
        }
        return;
    }
    int field = field_for(attribute->name);
    uint8_t value[VALUE_CAPACITY];
    uint32_t wanted = attribute->length < VALUE_CAPACITY ? attribute->length : VALUE_CAPACITY;
    if (field < 0 || !tag_read(reader, attribute->offset, value, wanted)) {
        return;
    }
    if (attribute->type == TYPE_STRING) {
        tag_offer(reader, (TagField)field, value, wanted, TAG_UTF16LE);
    } else if (attribute->type == TYPE_DWORD && wanted >= 4) {
        offer_number(reader, (TagField)field, tag_le32(value));
    } else if (attribute->type == TYPE_QWORD && wanted >= 8) {
        offer_number(reader, (TagField)field, tag_le64(value));
    } else if (attribute->type == TYPE_WORD && wanted >= 2) {
        offer_number(reader, (TagField)field, tag_le16(value));
    }
}

static void read_content(TagReader *reader, uint32_t body, uint32_t end)
{
    uint8_t lengths[10];
    if (!tag_read(reader, body, lengths, sizeof lengths)) {
        return;
    }
    uint32_t title = tag_le16(lengths);
    uint32_t author = tag_le16(lengths + 2);
    uint32_t cursor = body + sizeof lengths;
    uint8_t value[VALUE_CAPACITY];
    if (title <= end - cursor &&
        tag_read(reader, cursor, value, title < VALUE_CAPACITY ? title : VALUE_CAPACITY)) {
        tag_offer(reader, TAG_TITLE, value, title < VALUE_CAPACITY ? title : VALUE_CAPACITY,
                  TAG_UTF16LE);
    }
    cursor += title;
    if (author <= end - cursor &&
        tag_read(reader, cursor, value, author < VALUE_CAPACITY ? author : VALUE_CAPACITY)) {
        tag_offer(reader, TAG_ARTIST, value, author < VALUE_CAPACITY ? author : VALUE_CAPACITY,
                  TAG_UTF16LE);
    }
}

static void read_extended_content(TagReader *reader, uint32_t body, uint32_t end)
{
    uint8_t word[4];
    if (!tag_read(reader, body, word, 2)) {
        return;
    }
    uint32_t count = tag_le16(word);
    uint32_t cursor = body + 2;
    for (uint32_t index = 0; index < count && cursor + 2 <= end; index++) {
        Attribute attribute;
        if (!tag_read(reader, cursor, word, 2)) {
            return;
        }
        uint32_t name_length = tag_le16(word);
        read_name(reader, cursor + 2, name_length, attribute.name);
        cursor += 2 + name_length;
        if (cursor + 4 > end || !tag_read(reader, cursor, word, 4)) {
            return;
        }
        attribute.type = tag_le16(word);
        attribute.length = tag_le16(word + 2);
        attribute.offset = cursor + 4;
        cursor = attribute.offset + attribute.length;
        if (cursor > end) {
            return;
        }
        offer_attribute(reader, &attribute);
    }
}

static void read_metadata(TagReader *reader, uint32_t body, uint32_t end)
{
    uint8_t record[12];
    if (!tag_read(reader, body, record, 2)) {
        return;
    }
    uint32_t count = tag_le16(record);
    uint32_t cursor = body + 2;
    for (uint32_t index = 0; index < count && cursor + 12 <= end; index++) {
        if (!tag_read(reader, cursor, record, sizeof record)) {
            return;
        }
        Attribute attribute;
        uint32_t name_length = tag_le16(record + 4);
        attribute.type = tag_le16(record + 6);
        attribute.length = tag_le32(record + 8);
        read_name(reader, cursor + 12, name_length, attribute.name);
        attribute.offset = cursor + 12 + name_length;
        if (attribute.offset > end || attribute.length > end - attribute.offset) {
            return;
        }
        cursor = attribute.offset + attribute.length;
        offer_attribute(reader, &attribute);
    }
}

static void read_objects(TagReader *reader, uint32_t cursor, uint32_t end, uint32_t count);

static void read_extension(TagReader *reader, uint32_t body, uint32_t end)
{
    uint8_t size[4];
    if (body + EXTENSION_HEADER > end || !tag_read(reader, body + 18, size, 4)) {
        return;
    }
    uint32_t data = body + EXTENSION_HEADER;
    uint32_t data_end = tag_le32(size) <= end - data ? data + tag_le32(size) : end;
    read_objects(reader, data, data_end, UINT32_MAX);
}

static void read_file_properties(TagReader *reader, uint32_t body, uint32_t end)
{
    uint8_t properties[64];
    if (end - body < sizeof properties || !tag_read(reader, body, properties, sizeof properties)) {
        return;
    }
    uint64_t play_duration = tag_le64(properties + 40);
    uint64_t preroll = tag_le64(properties + 56);
    double seconds = (double)play_duration / 10000000.0 - (double)preroll / 1000.0;
    tag_set_duration(reader, seconds > 0.0 ? seconds : 0.0);
}

static void read_object(TagReader *reader, const uint8_t *guid, uint32_t body, uint32_t end)
{
    if (same_guid(guid, FILE_PROPERTIES)) {
        read_file_properties(reader, body, end);
    } else if (same_guid(guid, CONTENT)) {
        read_content(reader, body, end);
    } else if (same_guid(guid, EXTENDED_CONTENT)) {
        read_extended_content(reader, body, end);
    } else if (same_guid(guid, METADATA) || same_guid(guid, METADATA_LIBRARY)) {
        read_metadata(reader, body, end);
    } else if (same_guid(guid, HEADER_EXTENSION)) {
        read_extension(reader, body, end);
    }
}

static void read_objects(TagReader *reader, uint32_t cursor, uint32_t end, uint32_t count)
{
    uint8_t header[OBJECT_HEADER];
    for (uint32_t index = 0; index < count && cursor + OBJECT_HEADER <= end; index++) {
        if (!tag_read(reader, cursor, header, sizeof header)) {
            return;
        }
        uint64_t size = tag_le64(header + GUID_SIZE);
        if (size < OBJECT_HEADER || size > end - cursor) {
            return;
        }
        read_object(reader, header, cursor + OBJECT_HEADER, cursor + (uint32_t)size);
        cursor += (uint32_t)size;
    }
}

bool tags_read_wma(TagReader *reader)
{
    uint8_t header[HEADER_OBJECT_HEADER];
    if (!tag_read(reader, 0, header, sizeof header) || !same_guid(header, HEADER_GUID)) {
        return false;
    }
    uint64_t size = tag_le64(header + GUID_SIZE);
    uint32_t end = size < reader->size ? (uint32_t)size : reader->size;
    read_objects(reader, HEADER_OBJECT_HEADER, end, tag_le32(header + OBJECT_HEADER));
    return true;
}
