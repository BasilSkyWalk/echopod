#include <stddef.h>

#include "tags_internal.h"

enum {
    ATOM_HEADER = 8,
    VALUE_CAPACITY = 1024,
    DATA_HEADER = 16,
    TYPE_UTF8 = 1,
    MAX_DEPTH = 8
};

typedef struct Atom {
    uint32_t offset;
    uint32_t body;
    uint32_t end;
    char type[4];
} Atom;

typedef struct Mp4Item {
    const char *type;
    TagField field;
    bool numeric;
} Mp4Item;

static const Mp4Item ITEMS[] = {
    {"\xa9nam", TAG_TITLE, false},
    {"\xa9"
     "ART",
     TAG_ARTIST, false},
    {"aART", TAG_ALBUM_ARTIST, false},
    {"\xa9"
     "alb",
     TAG_ALBUM, false},
    {"\xa9gen", TAG_GENRE, false},
    {"\xa9"
     "day",
     TAG_DATE, false},
    {"disk", TAG_DISC, true},
    {"trkn", TAG_TRACK, true},
};

static bool read_atom(const TagReader *reader, uint32_t offset, uint32_t limit, Atom *atom)
{
    uint8_t header[16];
    if (offset > limit || limit - offset < ATOM_HEADER ||
        !tag_read(reader, offset, header, ATOM_HEADER)) {
        return false;
    }
    uint64_t size = tag_be32(header);
    uint32_t body = offset + ATOM_HEADER;
    if (size == 1) {
        if (!tag_read(reader, body, header + 8, 8)) {
            return false;
        }
        size = tag_be64(header + 8);
        body += 8;
    } else if (size == 0) {
        size = limit - offset;
    }
    if (size < body - offset || size > limit - offset) {
        return false;
    }
    *atom = (Atom){.offset = offset, .body = body, .end = offset + (uint32_t)size};
    for (int index = 0; index < 4; index++) {
        atom->type[index] = (char)header[4 + index];
    }
    return true;
}

static bool find_child(const TagReader *reader, const Atom *parent, uint32_t skip, const char *type,
                       Atom *found)
{
    uint32_t cursor = parent->body + skip;
    Atom atom;
    while (read_atom(reader, cursor, parent->end, &atom)) {
        if (tag_same_text(atom.type, 4, type)) {
            *found = atom;
            return true;
        }
        cursor = atom.end;
    }
    return false;
}

static bool is_sound_track(const TagReader *reader, const Atom *trak)
{
    Atom mdia;
    Atom hdlr;
    uint8_t handler[12];
    return find_child(reader, trak, 0, "mdia", &mdia) &&
           find_child(reader, &mdia, 0, "hdlr", &hdlr) &&
           tag_read(reader, hdlr.body, handler, sizeof handler) &&
           tag_same_text((const char *)handler + 8, 4, "soun");
}

static void read_duration(TagReader *reader, const Atom *trak)
{
    Atom mdia;
    Atom mdhd;
    uint8_t data[32];
    if (!find_child(reader, trak, 0, "mdia", &mdia) ||
        !find_child(reader, &mdia, 0, "mdhd", &mdhd) ||
        !tag_read(reader, mdhd.body, data, mdhd.end - mdhd.body < 32 ? mdhd.end - mdhd.body : 32)) {
        return;
    }
    uint32_t unit = 0;
    uint64_t length = 0;
    if (data[0] == 0) {
        unit = tag_be32(data + 12);
        length = tag_be32(data + 16);
    } else if (data[0] == 1) {
        unit = tag_be32(data + 20);
        length = tag_be64(data + 24);
    }
    if (unit > 0) {
        tag_set_duration(reader, (double)length / (double)unit);
    }
}

static bool sound_track(TagReader *reader, const Atom *moov, Atom *trak)
{
    uint32_t cursor = moov->body;
    Atom atom;
    while (read_atom(reader, cursor, moov->end, &atom)) {
        if (tag_same_text(atom.type, 4, "trak") && is_sound_track(reader, &atom)) {
            *trak = atom;
            return true;
        }
        cursor = atom.end;
    }
    return false;
}

static const Mp4Item *item_for(const char *type)
{
    for (size_t index = 0; index < sizeof ITEMS / sizeof ITEMS[0]; index++) {
        if (tag_same_text(type, 4, ITEMS[index].type)) {
            return &ITEMS[index];
        }
    }
    return NULL;
}

static void offer_number(TagReader *reader, TagField field, uint32_t value)
{
    char text[8];
    uint32_t length = 0;
    char digits[8];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value > 0 && count < sizeof digits);
    while (count > 0) {
        text[length++] = digits[--count];
    }
    text[length] = '\0';
    tag_offer_decoded(reader, field, text);
}

static void read_value(TagReader *reader, const Mp4Item *item, const Atom *data)
{
    uint8_t value[VALUE_CAPACITY];
    uint32_t length = data->end - data->body;
    if (length < 8) {
        return;
    }
    length -= 8;
    length = length < VALUE_CAPACITY ? length : VALUE_CAPACITY;
    if (!tag_read(reader, data->body + 8, value, length)) {
        return;
    }
    if (item->numeric) {
        tag_mark_present(reader, item->field);
        if (length >= 4) {
            offer_number(reader, item->field, tag_be16(value + 2));
        }
    } else {
        tag_offer(reader, item->field, value, length, TAG_UTF8);
    }
}

static void read_old_genre(TagReader *reader, const Atom *data)
{
    uint8_t value[2];
    if (data->end - data->body >= 10 && tag_read(reader, data->body + 8, value, 2)) {
        uint32_t index = tag_be16(value);
        if (index >= 1) {
            tag_offer_decoded(reader, TAG_GENRE, tags_id3_genre(index - 1));
        }
    }
}

static void read_cover(TagReader *reader, const Atom *data)
{
    if (data->end - data->body > 8) {
        tag_offer_picture(reader, (TagPicture){.kind = TAG_PICTURE_RAW,
                                               .front = true,
                                               .offset = data->body + 8,
                                               .length = data->end - data->body - 8});
    }
}

static void read_item(TagReader *reader, const Atom *item_atom, const Atom *genre_fallback)
{
    const Mp4Item *item = item_for(item_atom->type);
    bool cover = tag_same_text(item_atom->type, 4, "covr");
    if (item == NULL && !cover && genre_fallback == NULL) {
        return;
    }
    uint32_t cursor = item_atom->body;
    Atom data;
    while (read_atom(reader, cursor, item_atom->end, &data)) {
        if (tag_same_text(data.type, 4, "data")) {
            if (cover) {
                read_cover(reader, &data);
            } else if (item != NULL) {
                read_value(reader, item, &data);
            } else {
                read_old_genre(reader, &data);
            }
        }
        cursor = data.end;
    }
}

static void read_items(TagReader *reader, const Atom *ilst)
{
    uint32_t cursor = ilst->body;
    Atom atom;
    Atom old_genre;
    bool has_old_genre = false;
    while (read_atom(reader, cursor, ilst->end, &atom)) {
        if (tag_same_text(atom.type, 4, "gnre")) {
            old_genre = atom;
            has_old_genre = true;
        } else {
            read_item(reader, &atom, NULL);
        }
        cursor = atom.end;
    }
    if (has_old_genre && !reader->present[TAG_GENRE]) {
        read_item(reader, &old_genre, &old_genre);
    }
}

static void read_tags(TagReader *reader, const Atom *moov)
{
    Atom udta;
    Atom meta;
    Atom ilst;
    if (find_child(reader, moov, 0, "udta", &udta) && find_child(reader, &udta, 0, "meta", &meta) &&
        find_child(reader, &meta, 4, "ilst", &ilst)) {
        read_items(reader, &ilst);
    }
}

bool tags_read_mp4(TagReader *reader)
{
    Atom root = {.offset = 0, .body = 0, .end = reader->size};
    Atom moov;
    Atom trak;
    if (!find_child(reader, &root, 0, "moov", &moov)) {
        return false;
    }
    if (sound_track(reader, &moov, &trak)) {
        read_duration(reader, &trak);
    }
    read_tags(reader, &moov);
    return true;
}
