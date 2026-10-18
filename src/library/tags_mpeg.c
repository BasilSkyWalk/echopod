#include "tags_internal.h"

enum {
    HEADER_SIZE = 4,
    ID3_HEADER_SIZE = 10,
    MAX_READ = 1024 * 1024,
    MAX_SYNCS = 1500,
    ENOUGH_FRAMES = 4,
    MIN_FRAMES = 2,
    SCAN_CHUNK = 256,
    MONO = 3,
    XING_FRAMES = 0x1,
    XING_BYTES = 0x2,
    VBRI_OFFSET = 36,
    VBRI_SIZE = 26
};

static const uint16_t BITRATES[5][15] = {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
};

static const uint32_t RATES[3][3] = {
    {44100, 48000, 32000},
    {22050, 24000, 16000},
    {11025, 12000, 8000},
};

typedef struct MpegFrame {
    uint32_t offset;
    uint32_t length;
    uint32_t bitrate;
    uint32_t sample_rate;
    bool sketchy;
    bool has_length;
    double seconds;
} MpegFrame;

static uint32_t synchsafe(const uint8_t *bytes)
{
    return (uint32_t)(bytes[0] & 0x7F) << 21 | (uint32_t)(bytes[1] & 0x7F) << 14 |
           (uint32_t)(bytes[2] & 0x7F) << 7 | (uint32_t)(bytes[3] & 0x7F);
}

static uint32_t skip_id3(const TagReader *reader)
{
    uint32_t offset = 0;
    uint8_t header[ID3_HEADER_SIZE];
    while (tag_read(reader, offset, header, sizeof header) &&
           tag_same_text((const char *)header, 3, "ID3") && synchsafe(header + 6) > 0) {
        offset += ID3_HEADER_SIZE + synchsafe(header + 6);
    }
    return offset;
}

static uint32_t bitrate_row(uint32_t version, uint32_t layer)
{
    if (version == 1) {
        return layer - 1;
    }
    return layer == 1 ? 3 : 4;
}

static bool xing_length(const TagReader *reader, const MpegFrame *frame, uint32_t offset,
                        uint32_t frame_size, MpegFrame *result)
{
    uint8_t xing[12];
    if (!tag_read(reader, offset, xing, 8) || !(tag_same_text((const char *)xing, 4, "Xing") ||
                                                tag_same_text((const char *)xing, 4, "Info"))) {
        return false;
    }
    uint32_t flags = tag_be32(xing + 4);
    uint32_t frames = 0;
    if ((flags & XING_FRAMES) != 0) {
        if (!tag_read(reader, offset + 8, xing + 8, 4)) {
            return false;
        }
        frames = tag_be32(xing + 8);
    }
    *result = *frame;
    result->sketchy = false;
    if ((flags & XING_FRAMES) != 0) {
        result->has_length = true;
        result->seconds = (double)((uint64_t)frame_size * frames) / (double)frame->sample_rate;
    }
    return true;
}

static bool vbri_length(const TagReader *reader, const MpegFrame *frame, uint32_t frame_size,
                        MpegFrame *result)
{
    uint8_t vbri[VBRI_SIZE];
    if (!tag_read(reader, frame->offset + VBRI_OFFSET, vbri, sizeof vbri) ||
        !tag_same_text((const char *)vbri, 4, "VBRI") || tag_be16(vbri + 4) != 1) {
        return false;
    }
    *result = *frame;
    result->sketchy = false;
    result->has_length = true;
    result->seconds =
        (double)((uint64_t)frame_size * tag_be32(vbri + 14)) / (double)frame->sample_rate;
    return true;
}

static uint32_t xing_offset(uint32_t version, uint32_t mode)
{
    if (version == 1) {
        return mode != MONO ? 36 : 21;
    }
    return mode != MONO ? 21 : 13;
}

static bool parse_frame(const TagReader *reader, uint32_t offset, MpegFrame *frame)
{
    uint8_t header[HEADER_SIZE];
    if (!tag_read(reader, offset, header, sizeof header) || header[0] != 0xFF ||
        (header[1] & 0xE0) != 0xE0) {
        return false;
    }
    uint32_t version_bits = header[1] >> 3 & 3;
    uint32_t layer_bits = header[1] >> 1 & 3;
    uint32_t bitrate_index = header[2] >> 4;
    uint32_t rate_index = header[2] >> 2 & 3;
    uint32_t padding = header[2] >> 1 & 1;
    uint32_t mode = header[3] >> 6;
    if (version_bits == 1 || layer_bits == 0 || rate_index == 3 || bitrate_index == 15 ||
        bitrate_index == 0) {
        return false;
    }
    uint32_t version = version_bits == 3 ? 1 : version_bits == 2 ? 2 : 25;
    uint32_t layer = 4 - layer_bits;
    uint32_t bitrate = BITRATES[bitrate_row(version, layer)][bitrate_index] * 1000u;
    uint32_t sample_rate = RATES[version == 1 ? 0 : version == 2 ? 1 : 2][rate_index];
    uint32_t frame_size = layer == 1 ? 384 : (version != 1 && layer == 3) ? 576 : 1152;
    uint32_t slot = layer == 1 ? 4 : 1;
    uint32_t length = ((frame_size / 8 * bitrate) / sample_rate + padding) * slot;
    MpegFrame plain = {.offset = offset,
                       .length = length,
                       .bitrate = bitrate,
                       .sample_rate = sample_rate,
                       .sketchy = true,
                       .has_length = false};
    *frame = plain;
    if (layer == 3 &&
        !xing_length(reader, &plain, offset + xing_offset(version, mode), frame_size, frame)) {
        vbri_length(reader, &plain, frame_size, frame);
    }
    return true;
}

typedef struct SyncSearch {
    uint32_t frames_found;
    MpegFrame frames[ENOUGH_FRAMES];
    MpegFrame chosen;
    bool have_chosen;
} SyncSearch;

static bool try_sync(const TagReader *reader, uint32_t offset, SyncSearch *search)
{
    uint32_t count = 0;
    uint32_t cursor = offset;
    for (; count < ENOUGH_FRAMES; count++) {
        if (!parse_frame(reader, cursor, &search->frames[count])) {
            break;
        }
        cursor = search->frames[count].offset + search->frames[count].length;
        if (!search->frames[count].sketchy) {
            count++;
            break;
        }
    }
    if (count >= MIN_FRAMES && !search->have_chosen) {
        search->chosen = search->frames[0];
        search->have_chosen = true;
    }
    if (count > 0 && !search->frames[count - 1].sketchy) {
        search->chosen = search->frames[count - 1];
        search->have_chosen = true;
        return true;
    }
    if (count >= ENOUGH_FRAMES) {
        search->chosen = search->frames[0];
        search->have_chosen = true;
        return true;
    }
    return false;
}

static bool find_first_frame(const TagReader *reader, uint32_t start, MpegFrame *frame)
{
    SyncSearch search = {.frames_found = 0, .have_chosen = false};
    uint8_t chunk[SCAN_CHUNK + 1];
    uint32_t syncs = MAX_SYNCS;
    uint32_t end = reader->size - start > MAX_READ ? start + MAX_READ : reader->size;
    for (uint32_t base = start; base + 1 < end; base += SCAN_CHUNK) {
        uint32_t length = end - base > SCAN_CHUNK + 1 ? SCAN_CHUNK + 1 : end - base;
        if (!tag_read(reader, base, chunk, length)) {
            break;
        }
        for (uint32_t index = 0; index + 1 < length && index < SCAN_CHUNK; index++) {
            if (chunk[index] != 0xFF || (chunk[index + 1] & 0xE0) != 0xE0) {
                continue;
            }
            if (--syncs == 0) {
                break;
            }
            if (try_sync(reader, base + index, &search)) {
                *frame = search.chosen;
                return true;
            }
        }
        if (syncs == 0) {
            break;
        }
    }
    *frame = search.chosen;
    return search.have_chosen;
}

bool tags_read_mp3(TagReader *reader)
{
    uint32_t tag_size = 0;
    tags_read_id3v2(reader, 0, &tag_size);
    tags_read_id3v1(reader);
    MpegFrame frame;
    if (!find_first_frame(reader, skip_id3(reader), &frame)) {
        return false;
    }
    if (frame.has_length) {
        tag_set_duration(reader, frame.seconds);
    } else {
        double content = (double)(reader->size - frame.offset);
        tag_set_duration(reader, 8.0 * content / (double)frame.bitrate);
    }
    return true;
}
