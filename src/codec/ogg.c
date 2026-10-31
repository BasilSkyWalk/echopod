#include "codec/ogg.h"

#include <stddef.h>

enum {
    VERSION_AT = 4,
    FLAGS_AT = 5,
    GRANULE_AT = 6,
    SERIAL_AT = 14,
    SEGMENTS_AT = 26,
    FLAG_CONTINUED = 1,
    FLAG_FIRST = 2,
    HEAD_BYTES = 19,
    HEAD_VERSION_AT = 8,
    HEAD_CHANNELS_AT = 9,
    HEAD_PRE_SKIP_AT = 10,
    HEAD_RATE_AT = 12,
    HEAD_GAIN_AT = 16,
    HEAD_FAMILY_AT = 18,
    MAJOR_VERSION_MASK = 0xF0,
    LAST_SEGMENT = 255,
    MAX_PAGE_BYTES = OGG_PAGE_HEADER_BYTES + OGG_MAX_SEGMENTS + OGG_MAX_SEGMENTS * 255,
    SCAN_OVERLAP = 3,
    BISECT_SPAN = 65536,
    MAX_PACKET_SAMPLES = 5760,
    TOC_BYTES = 2,
    COUNT_MASK = 0x3F,
    CODE_MASK = 3,
    CONFIG_SHIFT = 3,
    SILK_CONFIGS = 12,
    HYBRID_CONFIGS = 16,
    SHORTEST_CELT = 120
};

static const uint32_t SILK_FRAMES[4] = {480, 960, 1920, 2880};

typedef struct Page {
    uint32_t offset;
    uint32_t end;
    uint32_t body;
    uint32_t serial;
    uint32_t granule;
    uint32_t segments;
    uint8_t flags;
    bool has_granule;
} Page;

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static uint32_t le16(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static bool read_at(FlacInput input, uint32_t offset, void *buffer, uint32_t length)
{
    return offset <= input.size && length <= input.size - offset &&
           input.read(input.context, offset, buffer, length);
}

static bool is_capture(const uint8_t *bytes)
{
    return bytes[0] == 'O' && bytes[1] == 'g' && bytes[2] == 'g' && bytes[3] == 'S';
}

bool ogg_signature(const uint8_t *bytes)
{
    return is_capture(bytes);
}

static bool page_header(const uint8_t *header, uint32_t offset, Page *page)
{
    if (!is_capture(header) || header[VERSION_AT] != 0) {
        return false;
    }
    uint32_t low = le32(header + GRANULE_AT);
    uint32_t high = le32(header + GRANULE_AT + 4);
    *page = (Page){
        .offset = offset,
        .serial = le32(header + SERIAL_AT),
        .granule = high != 0 ? UINT32_MAX : low,
        .segments = header[SEGMENTS_AT],
        .flags = header[FLAGS_AT],
        .has_granule = !(low == UINT32_MAX && high == UINT32_MAX),
    };
    page->body = offset + OGG_PAGE_HEADER_BYTES + page->segments;
    return true;
}

static uint32_t body_bytes(const uint8_t *lacing, uint32_t segments)
{
    uint32_t bytes = 0;
    for (uint32_t index = 0; index < segments; index++) {
        bytes += lacing[index];
    }
    return bytes;
}

static bool read_page(FlacInput file, uint32_t offset, Page *page, uint8_t *lacing)
{
    uint8_t header[OGG_PAGE_HEADER_BYTES];
    if (!read_at(file, offset, header, sizeof header) || !page_header(header, offset, page) ||
        !read_at(file, offset + OGG_PAGE_HEADER_BYTES, lacing, page->segments)) {
        return false;
    }
    page->end = page->body + body_bytes(lacing, page->segments);
    return page->end <= file.size;
}

static bool find_page(FlacInput file, uint32_t from, uint32_t serial, Page *page)
{
    uint8_t window[OGG_SCAN_BYTES];
    uint8_t lacing[OGG_MAX_SEGMENTS];
    uint32_t limit = file.size - from > MAX_PAGE_BYTES ? from + MAX_PAGE_BYTES : file.size;
    for (uint32_t start = from; start + 4 <= limit; start += OGG_SCAN_BYTES - SCAN_OVERLAP) {
        uint32_t length = limit - start < OGG_SCAN_BYTES ? limit - start : OGG_SCAN_BYTES;
        if (!read_at(file, start, window, length)) {
            return false;
        }
        for (uint32_t index = 0; index + 4 <= length; index++) {
            if (is_capture(window + index) && read_page(file, start + index, page, lacing) &&
                page->serial == serial) {
                return true;
            }
        }
    }
    return false;
}

static bool find_last_granule(FlacInput file, const OggOpusInfo *info, uint32_t *granule)
{
    uint8_t window[OGG_SCAN_BYTES];
    uint8_t lacing[OGG_MAX_SEGMENTS];
    uint32_t lowest = file.size - info->audio_start > MAX_PAGE_BYTES ? file.size - MAX_PAGE_BYTES
                                                                     : info->audio_start;
    uint32_t end = file.size;
    while (end > lowest + 4) {
        uint32_t start = end - lowest > OGG_SCAN_BYTES ? end - OGG_SCAN_BYTES : lowest;
        if (!read_at(file, start, window, end - start)) {
            return false;
        }
        for (uint32_t index = end - start - 4 + 1; index-- > 0;) {
            Page page;
            if (is_capture(window + index) && read_page(file, start + index, &page, lacing) &&
                page.serial == info->serial && page.has_granule) {
                *granule = page.granule;
                return true;
            }
        }
        if (start == lowest) {
            break;
        }
        end = start + SCAN_OVERLAP;
    }
    return false;
}

static bool read_head(FlacInput file, const Page *page, const uint8_t *lacing, OggOpusInfo *info)
{
    uint8_t head[HEAD_BYTES];
    static const char MAGIC[] = "OpusHead";
    if ((page->flags & FLAG_FIRST) == 0 || page->segments == 0 || lacing[0] < HEAD_BYTES ||
        lacing[0] == LAST_SEGMENT || !read_at(file, page->body, head, sizeof head)) {
        return false;
    }
    for (int index = 0; index < 8; index++) {
        if (head[index] != (uint8_t)MAGIC[index]) {
            return false;
        }
    }
    *info = (OggOpusInfo){
        .serial = page->serial,
        .input_rate = le32(head + HEAD_RATE_AT),
        .pre_skip = (uint16_t)le16(head + HEAD_PRE_SKIP_AT),
        .gain = (int16_t)le16(head + HEAD_GAIN_AT),
        .channels = head[HEAD_CHANNELS_AT],
    };
    return (head[HEAD_VERSION_AT] & MAJOR_VERSION_MASK) == 0 && head[HEAD_FAMILY_AT] == 0 &&
           info->channels >= 1 && info->channels <= FLAC_MAX_CHANNELS;
}

bool ogg_opus_probe(FlacInput file, OggOpusInfo *info)
{
    Page page;
    uint8_t lacing[OGG_MAX_SEGMENTS];
    if (!read_page(file, 0, &page, lacing) || !read_head(file, &page, lacing, info)) {
        return false;
    }
    bool tags_done = false;
    while (!tags_done) {
        if (!read_page(file, page.end, &page, lacing)) {
            return false;
        }
        tags_done = page.serial == info->serial && page.segments > 0 &&
                    lacing[page.segments - 1] < LAST_SEGMENT;
    }
    info->audio_start = page.end;
    uint32_t last = 0;
    if (!find_last_granule(file, info, &last) || last <= info->pre_skip) {
        return false;
    }
    info->last_granule = last;
    return true;
}

uint32_t ogg_opus_seek(FlacInput file, const OggOpusInfo *info, uint32_t granule)
{
    uint32_t margin = OGG_OPUS_PREROLL + MAX_PACKET_SAMPLES;
    if (granule <= margin) {
        return info->audio_start;
    }
    uint32_t goal = granule - margin;
    uint32_t low = info->audio_start;
    uint32_t high = file.size;
    Page page;
    while (high - low > BISECT_SPAN) {
        uint32_t middle = low + (high - low) / 2;
        if (find_page(file, middle, info->serial, &page) && page.has_granule &&
            page.granule <= goal) {
            low = page.end;
        } else {
            high = middle;
        }
    }
    uint32_t start = low;
    uint8_t lacing[OGG_MAX_SEGMENTS];
    while (read_page(file, low, &page, lacing)) {
        if (page.serial == info->serial && page.has_granule) {
            if (page.granule > goal) {
                break;
            }
            start = page.end;
        }
        low = page.end;
    }
    return start;
}

uint32_t ogg_opus_packet_samples(const uint8_t *packet, uint32_t length)
{
    if (length == 0) {
        return 0;
    }
    uint32_t config = packet[0] >> CONFIG_SHIFT;
    uint32_t frame = (uint32_t)SHORTEST_CELT << (config & 3u);
    if (config < SILK_CONFIGS) {
        frame = SILK_FRAMES[config & 3u];
    } else if (config < HYBRID_CONFIGS) {
        frame = (config & 1u) != 0 ? SILK_FRAMES[1] : SILK_FRAMES[0];
    }
    uint32_t code = packet[0] & CODE_MASK;
    if (code == 0) {
        return frame;
    }
    if (code < 3) {
        return 2 * frame;
    }
    return length < TOC_BYTES ? 0 : frame * (packet[1] & COUNT_MASK);
}

void ogg_reader_start(OggReader *reader, FlacInput input, uint32_t position, uint32_t serial,
                      uint8_t *buffer, uint32_t capacity)
{
    *reader = (OggReader){.input = input,
                          .buffer = buffer,
                          .capacity = capacity,
                          .serial = serial,
                          .position = position};
}

static OggResult peek_page(const OggReader *reader, uint32_t offset, Page *page, uint8_t *lacing)
{
    FlacInput input = reader->input;
    uint8_t header[OGG_PAGE_HEADER_BYTES];
    if (offset >= input.size || input.size - offset < sizeof header) {
        return OGG_END;
    }
    if (!input.read(input.context, offset, header, sizeof header)) {
        return OGG_MISSING;
    }
    if (!page_header(header, offset, page)) {
        return OGG_BAD;
    }
    if (page->body > input.size) {
        return OGG_END;
    }
    if (page->segments > 0 &&
        !input.read(input.context, offset + sizeof header, lacing, page->segments)) {
        return OGG_MISSING;
    }
    page->end = page->body + body_bytes(lacing, page->segments);
    uint8_t last = 0;
    if (page->end > input.size) {
        return OGG_END;
    }
    if (page->end > page->body && !input.read(input.context, page->end - 1, &last, 1)) {
        return OGG_MISSING;
    }
    return OGG_PACKET;
}

static OggResult enter(OggReader *reader)
{
    uint8_t lacing[OGG_MAX_SEGMENTS];
    uint32_t offset = reader->position;
    bool skipping = reader->skipping;
    uint32_t length = 0;
    uint32_t start = 0;
    uint32_t samples = 0;
    for (;;) {
        Page page;
        OggResult result = peek_page(reader, offset, &page, lacing);
        if (result != OGG_PACKET) {
            return result;
        }
        if (page.serial == reader->serial) {
            if (offset != reader->position && (page.flags & FLAG_CONTINUED) == 0) {
                length = 0;
                skipping = false;
            }
            uint32_t at = page.body;
            for (uint32_t index = 0; index < page.segments; index++) {
                uint32_t lace = lacing[index];
                if (!skipping) {
                    start = length == 0 ? at : start;
                    length += lace;
                }
                at += lace;
                if (lace == LAST_SEGMENT) {
                    continue;
                }
                if (!skipping && length > 0) {
                    uint8_t toc[TOC_BYTES] = {0};
                    if (!read_at(reader->input, start, toc,
                                 length < TOC_BYTES ? length : TOC_BYTES)) {
                        return OGG_MISSING;
                    }
                    samples += ogg_opus_packet_samples(toc, length);
                }
                skipping = false;
                length = 0;
            }
            if (page.has_granule) {
                reader->next_sample = page.granule > samples ? page.granule - samples : 0;
                reader->entered = true;
                return OGG_PACKET;
            }
        }
        offset = page.end;
    }
}

static OggResult load_page(OggReader *reader)
{
    Page page;
    OggResult result = peek_page(reader, reader->position, &page, reader->lacing);
    if (result != OGG_PACKET) {
        return result;
    }
    if (reader->adopt_serial) {
        reader->serial = page.serial;
        reader->adopt_serial = false;
    }
    if (page.serial != reader->serial) {
        reader->position = page.end;
        return OGG_PACKET;
    }
    bool continued = (page.flags & FLAG_CONTINUED) != 0;
    if (!continued) {
        reader->packet_length = 0;
    }
    reader->skipping = continued && (!reader->entered || reader->packet_length == 0);
    if (!reader->entered) {
        OggResult entered = enter(reader);
        if (entered == OGG_MISSING || entered == OGG_BAD) {
            return entered;
        }
        reader->entered = true;
    }
    reader->segments = page.segments;
    reader->segment = 0;
    reader->body = page.body;
    reader->page_end = page.end;
    reader->page_loaded = true;
    return OGG_PACKET;
}

void ogg_reader_follow_first(OggReader *reader)
{
    reader->adopt_serial = true;
}

OggResult ogg_reader_next(OggReader *reader, OggPacket *packet)
{
    for (;;) {
        if (!reader->page_loaded) {
            OggResult result = load_page(reader);
            if (result != OGG_PACKET) {
                return result;
            }
            continue;
        }
        if (reader->segment == reader->segments) {
            reader->position = reader->page_end;
            reader->page_loaded = false;
            continue;
        }
        uint32_t lace = reader->lacing[reader->segment];
        uint32_t from = reader->body;
        if (reader->skipping) {
            reader->skipping = lace == LAST_SEGMENT;
        } else if (reader->packet_length + lace > reader->capacity) {
            return OGG_BAD;
        } else if (lace > 0 && !reader->input.read(reader->input.context, from,
                                                   reader->buffer + reader->packet_length, lace)) {
            return OGG_MISSING;
        } else {
            reader->packet_length += lace;
        }
        reader->segment++;
        reader->body += lace;
        bool skipped = reader->skipping || (lace == LAST_SEGMENT);
        if (skipped || reader->packet_length == 0) {
            continue;
        }
        uint32_t length = reader->packet_length;
        reader->packet_length = 0;
        uint32_t samples = ogg_opus_packet_samples(reader->buffer, length);
        *packet = (OggPacket){reader->buffer, length, reader->next_sample, samples};
        reader->next_sample += samples;
        return OGG_PACKET;
    }
}
