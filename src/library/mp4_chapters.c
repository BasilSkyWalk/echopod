#include "library/mp4_chapters.h"

enum {
    ATOM_HEADER = 8,
    CHPL_COUNT_AT = 8,
    CHPL_ENTRIES_AT = 9,
    ENTRY_START_BYTES = 8,
    UNITS_PER_MS = 10000,
    MS_PER_SECOND = 1000,
    TYPE_MOOV = 0x6D6F6F76,
    TYPE_UDTA = 0x75647461,
    TYPE_CHPL = 0x6368706C
};

typedef struct Atom {
    uint32_t body;
    uint32_t end;
    uint32_t type;
} Atom;

static uint32_t be32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

static uint64_t be64(const uint8_t *bytes)
{
    return (uint64_t)be32(bytes) << 32 | be32(bytes + 4);
}

static bool read_at(ByteSource source, uint32_t size, uint32_t offset, void *buffer,
                    uint32_t length)
{
    return offset <= size && length <= size - offset &&
           source.read(source.context, offset, buffer, length);
}

static bool read_atom(ByteSource source, uint32_t size, uint32_t offset, uint32_t limit, Atom *atom)
{
    uint8_t header[ATOM_HEADER];
    if (offset > limit || limit - offset < ATOM_HEADER ||
        !read_at(source, size, offset, header, ATOM_HEADER)) {
        return false;
    }
    uint32_t length = be32(header);
    if (length < ATOM_HEADER || length > limit - offset) {
        return false;
    }
    atom->body = offset + ATOM_HEADER;
    atom->end = offset + length;
    atom->type = be32(header + 4);
    return true;
}

static bool find_child(ByteSource source, uint32_t size, uint32_t from, uint32_t limit,
                       uint32_t type, Atom *found)
{
    Atom atom;
    uint32_t cursor = from;
    while (read_atom(source, size, cursor, limit, &atom)) {
        if (atom.type == type) {
            *found = atom;
            return true;
        }
        cursor = atom.end;
    }
    return false;
}

static bool find_chpl(ByteSource source, uint32_t size, Atom *chpl)
{
    Atom moov;
    Atom udta;
    return find_child(source, size, 0, size, TYPE_MOOV, &moov) &&
           find_child(source, size, moov.body, moov.end, TYPE_UDTA, &udta) &&
           find_child(source, size, udta.body, udta.end, TYPE_CHPL, chpl);
}

static bool read_entries(Mp4Chapters *chapters, ByteSource source, uint32_t size, const Atom *chpl,
                         uint32_t count)
{
    uint32_t cursor = chpl->body + CHPL_ENTRIES_AT;
    uint32_t previous = 0;
    uint32_t previous_seconds = 0;
    for (uint32_t index = 0; index < count; index++) {
        uint8_t entry[ENTRY_START_BYTES + 1];
        if (chpl->end - cursor < sizeof entry ||
            !read_at(source, size, cursor, entry, sizeof entry)) {
            return false;
        }
        uint64_t start = be64(entry) / UNITS_PER_MS;
        uint32_t start_ms = start > UINT32_MAX ? UINT32_MAX : (uint32_t)start;
        uint32_t length = entry[ENTRY_START_BYTES];
        if (start_ms < previous || chpl->end - cursor - sizeof entry < length) {
            return false;
        }
        uint32_t seconds = start_ms / MS_PER_SECOND;
        if (index < MP4_CHAPTERS_KEPT) {
            if (seconds - previous_seconds > UINT16_MAX) {
                return false;
            }
            chapters->gap_seconds[index] = (uint16_t)(seconds - previous_seconds);
        }
        previous = start_ms;
        previous_seconds = seconds;
        cursor += (uint32_t)sizeof entry + length;
    }
    return true;
}

bool mp4_chapters_read(Mp4Chapters *chapters, ByteSource source, uint32_t size)
{
    Atom chpl;
    uint8_t head[CHPL_ENTRIES_AT];
    chapters->count = 0;
    if (!find_chpl(source, size, &chpl) || chpl.end - chpl.body < sizeof head ||
        !read_at(source, size, chpl.body, head, sizeof head)) {
        return false;
    }
    uint32_t count = head[CHPL_COUNT_AT];
    if (count == 0 || !read_entries(chapters, source, size, &chpl, count)) {
        return false;
    }
    chapters->count = (uint16_t)count;
    return true;
}

uint32_t mp4_chapter_start_ms(const Mp4Chapters *chapters, uint16_t index)
{
    uint32_t seconds = 0;
    for (uint16_t at = 0; at <= index && at < MP4_CHAPTERS_KEPT; at++) {
        seconds += chapters->gap_seconds[at];
    }
    return seconds * MS_PER_SECOND;
}

uint16_t mp4_chapter_at(const Mp4Chapters *chapters, uint32_t position_ms)
{
    uint32_t seconds = 0;
    uint16_t found = 0;
    for (uint16_t index = 0; index < chapters->count && index < MP4_CHAPTERS_KEPT; index++) {
        seconds += chapters->gap_seconds[index];
        if (index > 0 && seconds * MS_PER_SECOND > position_ms) {
            break;
        }
        found = index;
    }
    return found;
}
