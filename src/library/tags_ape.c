#include <stddef.h>

#include "tags_internal.h"

enum {
    HEADER_SIZE = 76,
    FOOTER_SIZE = 32,
    ID3V1_SIZE = 128,
    VALUE_CAPACITY = 1024,
    KEY_CAPACITY = 256,
    TYPE_MASK = 0x6,
    TYPE_TEXT = 0x0,
    TYPE_BINARY = 0x2,
    NEW_HEADER_VERSION = 3980
};

typedef struct ApeKey {
    const char *name;
    TagField field;
} ApeKey;

static const ApeKey KEYS[] = {
    {"title", TAG_TITLE}, {"artist", TAG_ARTIST}, {"album artist", TAG_ALBUM_ARTIST},
    {"album", TAG_ALBUM}, {"genre", TAG_GENRE},   {"year", TAG_DATE},
    {"disc", TAG_DISC},   {"track", TAG_TRACK},
};

static uint32_t old_blocks_per_frame(uint32_t version, uint32_t compression)
{
    if (version >= 3950) {
        return 73728 * 4;
    }
    if (version >= 3900 || (version >= 3800 && compression == 4)) {
        return 73728;
    }
    return 9216;
}

static void read_length(TagReader *reader, const uint8_t *header)
{
    uint32_t version = tag_le16(header + 4);
    uint32_t blocks_per_frame = 0;
    uint32_t final_frame_blocks = 0;
    uint32_t total_frames = 0;
    uint32_t rate = 0;
    if (version >= NEW_HEADER_VERSION) {
        blocks_per_frame = tag_le32(header + 56);
        final_frame_blocks = tag_le32(header + 60);
        total_frames = tag_le32(header + 64);
        rate = tag_le32(header + 72);
    } else {
        rate = tag_le32(header + 12);
        total_frames = tag_le32(header + 24);
        final_frame_blocks = tag_le32(header + 28);
        blocks_per_frame = old_blocks_per_frame(version, tag_le16(header + 6));
    }
    if (rate > 0 && total_frames > 0) {
        uint64_t blocks = (uint64_t)(total_frames - 1) * blocks_per_frame + final_frame_blocks;
        tag_set_duration(reader, (double)blocks / (double)rate);
    }
}

static int field_for(const char *key, uint32_t length)
{
    for (size_t index = 0; index < sizeof KEYS / sizeof KEYS[0]; index++) {
        if (tag_same_text_folded(key, length, KEYS[index].name)) {
            return (int)KEYS[index].field;
        }
    }
    return -1;
}

static void offer_values(TagReader *reader, TagField field, const uint8_t *value, uint32_t length)
{
    tag_mark_present(reader, field);
    uint32_t start = 0;
    for (uint32_t index = 0; index <= length; index++) {
        if (index == length || value[index] == 0) {
            if (index > start) {
                tag_offer(reader, field, value + start, index - start, TAG_UTF8);
            }
            start = index + 1;
        }
    }
}

static void read_cover(TagReader *reader, uint32_t offset, uint32_t length, bool front)
{
    uint8_t name[KEY_CAPACITY];
    uint32_t wanted = length < KEY_CAPACITY ? length : KEY_CAPACITY;
    if (!tag_read(reader, offset, name, wanted)) {
        return;
    }
    for (uint32_t index = 0; index < wanted; index++) {
        if (name[index] == 0) {
            tag_offer_picture(reader, (TagPicture){.kind = TAG_PICTURE_RAW,
                                                   .front = front,
                                                   .offset = offset + index + 1,
                                                   .length = length - index - 1});
            return;
        }
    }
}

static bool is_cover_key(const char *key, uint32_t length)
{
    static const char PREFIX[] = "cover art (";
    uint32_t prefix = sizeof PREFIX - 1;
    return length > prefix && tag_same_text_folded(key, prefix, PREFIX);
}

static uint32_t read_item(TagReader *reader, uint32_t cursor, uint32_t end)
{
    uint8_t head[8];
    char key[KEY_CAPACITY];
    if (cursor + 8 > end || !tag_read(reader, cursor, head, 8)) {
        return end;
    }
    uint32_t size = tag_le32(head);
    uint32_t flags = tag_le32(head + 4);
    uint32_t key_start = cursor + 8;
    uint32_t key_length = 0;
    while (key_start + key_length < end && key_length < KEY_CAPACITY &&
           tag_read(reader, key_start + key_length, key + key_length, 1) &&
           key[key_length] != '\0') {
        key_length++;
    }
    uint32_t value = key_start + key_length + 1;
    if (value > end || size > end - value) {
        return end;
    }
    uint32_t type = flags & TYPE_MASK;
    int field = field_for(key, key_length);
    if (type == TYPE_TEXT && field >= 0) {
        uint8_t text[VALUE_CAPACITY];
        uint32_t wanted = size < VALUE_CAPACITY ? size : VALUE_CAPACITY;
        if (tag_read(reader, value, text, wanted)) {
            offer_values(reader, (TagField)field, text, wanted);
        }
    } else if (type == TYPE_BINARY && is_cover_key(key, key_length)) {
        read_cover(reader, value, size, tag_same_text_folded(key, key_length, "cover art (front)"));
    }
    return value + size;
}

static bool footer_at(const TagReader *reader, uint32_t offset, uint8_t *footer)
{
    return offset >= FOOTER_SIZE && tag_read(reader, offset - FOOTER_SIZE, footer, FOOTER_SIZE) &&
           tag_same_text((const char *)footer, 8, "APETAGEX");
}

static void read_apev2(TagReader *reader)
{
    uint8_t footer[FOOTER_SIZE];
    uint32_t end = reader->size;
    if (!footer_at(reader, end, footer)) {
        if (end < ID3V1_SIZE || !footer_at(reader, end - ID3V1_SIZE, footer)) {
            return;
        }
        end -= ID3V1_SIZE;
    }
    uint32_t tag_size = tag_le32(footer + 12);
    uint32_t count = tag_le32(footer + 16);
    if (tag_size < FOOTER_SIZE || tag_size > end) {
        return;
    }
    uint32_t cursor = end - tag_size;
    uint32_t items_end = end - FOOTER_SIZE;
    for (uint32_t item = 0; item < count && cursor < items_end; item++) {
        cursor = read_item(reader, cursor, items_end);
    }
}

bool tags_read_ape(TagReader *reader)
{
    uint8_t header[HEADER_SIZE];
    if (!tag_read(reader, 0, header, sizeof header) ||
        !tag_same_text((const char *)header, 4, "MAC ")) {
        return false;
    }
    read_length(reader, header);
    read_apev2(reader);
    return true;
}
