#include "tags_internal.h"

enum {
    RIFF_HEADER = 12,
    CHUNK_HEADER = 8,
    DSF_FMT_OFFSET = 28,
    DSF_FMT_SIZE = 52,
    DFF_HEADER = 16,
    DFF_CHUNK_HEADER = 12
};

static bool same(const uint8_t *bytes, const char *text)
{
    return tag_same_text((const char *)bytes, 4, text);
}

bool tags_read_wav(TagReader *reader)
{
    uint8_t header[16];
    if (!tag_read(reader, 0, header, RIFF_HEADER) || !same(header, "RIFF") ||
        !same(header + 8, "WAVE")) {
        return false;
    }
    uint32_t rate = 0;
    uint32_t block_align = 0;
    uint32_t data_size = 0;
    bool has_data = false;
    uint32_t cursor = RIFF_HEADER;
    while (cursor <= reader->size - CHUNK_HEADER &&
           tag_read(reader, cursor, header, CHUNK_HEADER)) {
        uint32_t size = tag_le32(header + 4);
        uint32_t body = cursor + CHUNK_HEADER;
        uint8_t format[16];
        if (same(header, "fmt ") && size >= sizeof format &&
            tag_read(reader, body, format, sizeof format)) {
            rate = tag_le32(format + 4);
            block_align = tag_le16(format + 12);
        } else if (same(header, "data")) {
            data_size = size;
            has_data = true;
        } else if (same(header, "id3 ") || same(header, "ID3 ")) {
            uint32_t tag_size = 0;
            tags_read_id3v2(reader, body, &tag_size);
        }
        if (size > reader->size - body) {
            break;
        }
        cursor = body + size + (size & 1u);
    }
    if (rate > 0 && block_align > 0 && has_data) {
        tag_set_duration(reader, (double)data_size / (double)block_align / (double)rate);
    }
    return rate > 0;
}

bool tags_read_dsf(TagReader *reader)
{
    uint8_t header[DSF_FMT_OFFSET];
    uint8_t format[DSF_FMT_SIZE];
    if (!tag_read(reader, 0, header, sizeof header) || !same(header, "DSD ") ||
        !tag_read(reader, DSF_FMT_OFFSET, format, sizeof format) || !same(format, "fmt ")) {
        return false;
    }
    uint32_t rate = tag_le32(format + 28);
    uint64_t samples = tag_le64(format + 36);
    if (rate > 0) {
        tag_set_duration(reader, (double)samples / (double)rate);
    }
    uint64_t metadata = tag_le64(header + 20);
    uint32_t tag_size = 0;
    if (metadata > 0 && metadata < reader->size) {
        tags_read_id3v2(reader, (uint32_t)metadata, &tag_size);
    }
    return true;
}

typedef struct DffSound {
    uint32_t rate;
    uint32_t channels;
    bool uncompressed;
} DffSound;

static void read_properties(const TagReader *reader, uint32_t start, uint32_t end, DffSound *sound)
{
    uint8_t chunk[DFF_CHUNK_HEADER + 4];
    uint32_t cursor = start + 4;
    while (cursor + DFF_CHUNK_HEADER <= end && tag_read(reader, cursor, chunk, sizeof chunk)) {
        uint64_t size = tag_be64(chunk + 4);
        if (same(chunk, "FS  ")) {
            sound->rate = tag_be32(chunk + 12);
        } else if (same(chunk, "CHNL")) {
            sound->channels = tag_be16(chunk + 12);
        } else if (same(chunk, "CMPR")) {
            sound->uncompressed = same(chunk + 12, "DSD ");
        }
        if (size > end - cursor - DFF_CHUNK_HEADER) {
            return;
        }
        cursor += DFF_CHUNK_HEADER + (uint32_t)size + (uint32_t)(size & 1u);
    }
}

bool tags_read_dff(TagReader *reader)
{
    uint8_t chunk[DFF_HEADER];
    if (!tag_read(reader, 0, chunk, DFF_HEADER) || !same(chunk, "FRM8") ||
        !same(chunk + 12, "DSD ")) {
        return false;
    }
    DffSound sound = {.rate = 0, .channels = 0, .uncompressed = false};
    uint64_t data_size = 0;
    bool has_data = false;
    uint32_t cursor = DFF_HEADER;
    while (cursor <= reader->size - DFF_CHUNK_HEADER &&
           tag_read(reader, cursor, chunk, DFF_CHUNK_HEADER)) {
        uint64_t size = tag_be64(chunk + 4);
        uint32_t body = cursor + DFF_CHUNK_HEADER;
        if (size > reader->size - body) {
            break;
        }
        if (same(chunk, "PROP")) {
            read_properties(reader, body, body + (uint32_t)size, &sound);
        } else if (same(chunk, "DSD ")) {
            data_size = size;
            has_data = true;
        } else if (same(chunk, "ID3 ")) {
            uint32_t tag_size = 0;
            tags_read_id3v2(reader, body, &tag_size);
        }
        cursor = body + (uint32_t)size + (uint32_t)(size & 1u);
    }
    if (sound.uncompressed && has_data && sound.rate > 0) {
        double channels = sound.channels > 0 ? (double)sound.channels : 1.0;
        tag_set_duration(reader, (double)data_size * 8.0 / channels / (double)sound.rate);
    }
    return true;
}
