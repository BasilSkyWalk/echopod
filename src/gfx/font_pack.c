#include "gfx/font_pack.h"

enum {
    FACE_UNKNOWN = 0,
    FACE_READY = 1,
    FACE_MISSING = 2,
    MAX_CODEPOINT = 0xFFFF
};

static const uint8_t MAGIC[4] = {'E', 'G', 'L', 'F'};

typedef struct PackFace {
    uint8_t state;
    uint16_t directory_count;
    uint32_t glyph_count;
    uint32_t index_offset;
    uint8_t directory[2 * FONT_PACK_MAX_DIRECTORY];
} PackFace;

typedef struct PackSlot {
    uint32_t key;
    bool present;
    Glyph glyph;
    uint8_t bitmap[FONT_PACK_MAX_BITMAP];
} PackSlot;

typedef struct PackState {
    PackFace faces[FONT_PACK_FACES];
    uint8_t entries[FONT_PACK_ENTRIES_PER_SECTOR * FONT_PACK_ENTRY_BYTES];
} PackState;

static FontPackReader pack_reader;
static PackState *state;
static PackSlot *slots;
static uint32_t slot_count;
static uint32_t victim;

static uint32_t le16(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t le32(const uint8_t *bytes)
{
    return le16(bytes) | le16(bytes + 2) << 16;
}

uint32_t font_pack_attach(FontPackReader reader, void *memory, size_t bytes)
{
    font_pack_detach();
    size_t set_bytes = FONT_PACK_WAYS * sizeof(PackSlot);
    if (reader.read == NULL || memory == NULL || bytes < sizeof(PackState) + set_bytes) {
        return 0;
    }
    uint32_t sets = 1;
    while ((size_t)sets * 2 * set_bytes <= bytes - sizeof(PackState)) {
        sets *= 2;
    }
    pack_reader = reader;
    state = memory;
    slots = (PackSlot *)(state + 1);
    slot_count = sets * FONT_PACK_WAYS;
    font_pack_forget();
    return slot_count;
}

void font_pack_detach(void)
{
    state = NULL;
    slots = NULL;
    slot_count = 0;
}

void font_pack_forget(void)
{
    if (state == NULL) {
        return;
    }
    for (uint32_t face = 0; face < FONT_PACK_FACES; face++) {
        state->faces[face].state = FACE_UNKNOWN;
    }
    for (uint32_t index = 0; index < slot_count; index++) {
        slots[index].key = 0;
    }
}

static bool read_face(uint32_t face, uint32_t offset, void *buffer, uint32_t length)
{
    return pack_reader.read(pack_reader.context, face, offset, buffer, length);
}

static bool open_face(uint32_t index, PackFace *face)
{
    uint8_t header[FONT_PACK_HEADER_BYTES];
    if (!read_face(index, 0, header, sizeof header)) {
        return false;
    }
    for (int byte = 0; byte < 4; byte++) {
        if (header[byte] != MAGIC[byte]) {
            return false;
        }
    }
    uint32_t count = le32(header + 8);
    uint32_t sectors = (count + FONT_PACK_ENTRIES_PER_SECTOR - 1) / FONT_PACK_ENTRIES_PER_SECTOR;
    if (header[4] != FONT_PACK_VERSION || header[5] != FONT_PACK_ENTRY_BYTES ||
        le16(header + 6) != FONT_PACK_ENTRIES_PER_SECTOR || count == 0 ||
        sectors > FONT_PACK_MAX_DIRECTORY) {
        return false;
    }
    face->glyph_count = count;
    face->directory_count = (uint16_t)sectors;
    face->index_offset = le32(header + 16);
    return read_face(index, le32(header + 12), face->directory, 2 * sectors);
}

static PackFace *ready_face(uint32_t index)
{
    PackFace *face = &state->faces[index];
    if (face->state == FACE_UNKNOWN) {
        face->state = open_face(index, face) ? FACE_READY : FACE_MISSING;
    }
    return face->state == FACE_READY ? face : NULL;
}

static int index_sector(const PackFace *face, uint32_t codepoint)
{
    int low = 0;
    int high = (int)face->directory_count - 1;
    int found = -1;
    while (low <= high) {
        int middle = low + (high - low) / 2;
        if (le16(face->directory + 2 * middle) <= codepoint) {
            found = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return found;
}

static const uint8_t *find_entry(uint32_t index, const PackFace *face, uint32_t codepoint)
{
    int sector = index_sector(face, codepoint);
    if (sector < 0) {
        return NULL;
    }
    uint32_t first = (uint32_t)sector * FONT_PACK_ENTRIES_PER_SECTOR;
    uint32_t count = face->glyph_count - first < FONT_PACK_ENTRIES_PER_SECTOR
                         ? face->glyph_count - first
                         : FONT_PACK_ENTRIES_PER_SECTOR;
    uint32_t offset = face->index_offset + (uint32_t)sector * FONT_PACK_SECTOR_BYTES;
    if (!read_face(index, offset, state->entries, count * FONT_PACK_ENTRY_BYTES)) {
        return NULL;
    }
    int low = 0;
    int high = (int)count - 1;
    while (low <= high) {
        int middle = low + (high - low) / 2;
        const uint8_t *entry = state->entries + middle * FONT_PACK_ENTRY_BYTES;
        uint32_t found = le16(entry);
        if (found == codepoint) {
            return entry;
        }
        if (found < codepoint) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return NULL;
}

static bool load_glyph(uint32_t index, uint32_t codepoint, PackSlot *slot)
{
    const PackFace *face = ready_face(index);
    const uint8_t *entry = face != NULL ? find_entry(index, face, codepoint) : NULL;
    if (entry == NULL) {
        return false;
    }
    uint32_t bytes = ((uint32_t)entry[2] * entry[3] + 1) / 2;
    if (bytes > FONT_PACK_MAX_BITMAP ||
        (bytes > 0 && !read_face(index, le32(entry + 8), slot->bitmap, bytes))) {
        return false;
    }
    slot->glyph = (Glyph){.codepoint = (uint16_t)codepoint,
                          .bitmap_offset = 0,
                          .width = entry[2],
                          .height = entry[3],
                          .left = (int8_t)entry[4],
                          .top = (int8_t)entry[5],
                          .advance = entry[6]};
    return true;
}

static uint32_t set_of(uint32_t key)
{
    return (key * 2654435761u >> 16) & (slot_count / FONT_PACK_WAYS - 1);
}

const Glyph *font_pack_find(uint32_t face, uint32_t codepoint)
{
    if (state == NULL || face >= FONT_PACK_FACES || codepoint > MAX_CODEPOINT) {
        return NULL;
    }
    uint32_t key = (face + 1) << 16 | codepoint;
    PackSlot *set = slots + set_of(key) * FONT_PACK_WAYS;
    for (uint32_t way = 0; way < FONT_PACK_WAYS; way++) {
        if (set[way].key == key) {
            return set[way].present ? &set[way].glyph : NULL;
        }
    }
    PackSlot *slot = &set[victim++ % FONT_PACK_WAYS];
    slot->key = 0;
    slot->present = load_glyph(face, codepoint, slot);
    slot->key = key;
    return slot->present ? &slot->glyph : NULL;
}

const uint8_t *font_pack_bitmap(const Glyph *glyph)
{
    const uint8_t *address = (const uint8_t *)glyph;
    const uint8_t *start = (const uint8_t *)slots;
    if (slots == NULL || address < start || address >= start + slot_count * sizeof(PackSlot)) {
        return NULL;
    }
    return slots[(size_t)(address - start) / sizeof(PackSlot)].bitmap;
}
