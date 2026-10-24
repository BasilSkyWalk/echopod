#include "device/mp3_probe.h"

enum {
    TAG_HEADER_BYTES = 10,
    TAG_FOOTER_FLAG = 0x10,
    TAG_FOOTER_BYTES = 10,
    ID3V1_BYTES = 128,
    APE_FOOTER_BYTES = 32,
    APE_HEADER_FLAG = 0x80,
    SCAN_LIMIT = 65536,
    SCAN_WINDOW = 256,
    HEADER_BYTES = 4,
    INFO_BYTES = 156,
    VBRI_OFFSET = 36,
    XING_FRAMES_FLAG = 1,
    XING_BYTES_FLAG = 2,
    XING_TOC_FLAG = 4,
    MPEG_1 = 3,
    MPEG_2 = 2,
    MPEG_2_5 = 0,
    LAYER_1_SAMPLES = 384,
    MPEG_1_SAMPLES = 1152,
    MPEG_2_LAYER_3_SAMPLES = 576,
    TOC_SCALE = 256,
    PERCENT = 100,
    BITS_PER_BYTE = 8,
    KILO = 1000
};

static const uint16_t MPEG_1_BITRATES[3][15] = {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
};
static const uint16_t MPEG_2_BITRATES[2][15] = {
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
};
static const uint32_t BASE_RATES[3] = {44100, 48000, 32000};

typedef struct Header {
    uint32_t version;
    uint32_t layer;
    uint32_t kbps;
    uint32_t rate;
    bool mono;
    uint32_t length;
    uint16_t samples;
} Header;

static uint32_t big_endian(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 |
           (uint32_t)bytes[3];
}

static uint32_t little_endian(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool same_tag(const uint8_t *bytes, const char *tag, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        if (bytes[index] != (uint8_t)tag[index]) {
            return false;
        }
    }
    return true;
}

static uint16_t samples_per_frame(uint32_t version, uint32_t layer)
{
    if (layer == 1) {
        return LAYER_1_SAMPLES;
    }
    return layer == 3 && version != MPEG_1 ? MPEG_2_LAYER_3_SAMPLES : MPEG_1_SAMPLES;
}

static bool parse_header(const uint8_t *bytes, Header *header)
{
    if (bytes[0] != 0xFF || (bytes[1] & 0xE0) != 0xE0) {
        return false;
    }
    uint32_t version = (uint32_t)(bytes[1] >> 3) & 3u;
    uint32_t layer_bits = (uint32_t)(bytes[1] >> 1) & 3u;
    uint32_t bitrate_index = (uint32_t)bytes[2] >> 4;
    uint32_t rate_index = (uint32_t)(bytes[2] >> 2) & 3u;
    if (version == 1 || layer_bits == 0 || bitrate_index == 0 || bitrate_index == 15 ||
        rate_index == 3) {
        return false;
    }
    uint32_t layer = 4 - layer_bits;
    uint32_t kbps = version == MPEG_1 ? MPEG_1_BITRATES[layer - 1][bitrate_index]
                                      : MPEG_2_BITRATES[layer == 1 ? 0 : 1][bitrate_index];
    uint32_t shift = version == MPEG_1 ? 0 : version == MPEG_2 ? 1 : 2;
    uint32_t rate = BASE_RATES[rate_index] >> shift;
    uint32_t padding = (uint32_t)(bytes[2] >> 1) & 1u;
    uint16_t samples = samples_per_frame(version, layer);
    uint32_t length = layer == 1 ? (12 * kbps * KILO / rate + padding) * 4
                                 : samples / BITS_PER_BYTE * kbps * KILO / rate + padding;
    *header = (Header){.version = version,
                       .layer = layer,
                       .kbps = kbps,
                       .rate = rate,
                       .mono = (bytes[3] >> 6) == 3,
                       .length = length,
                       .samples = samples};
    return true;
}

bool mp3_signature(const uint8_t bytes[MP3_SIGNATURE_BYTES])
{
    Header header;
    return same_tag(bytes, "ID3", 3) || parse_header(bytes, &header);
}

static uint32_t syncsafe(const uint8_t *bytes)
{
    return (uint32_t)(bytes[0] & 0x7F) << 21 | (uint32_t)(bytes[1] & 0x7F) << 14 |
           (uint32_t)(bytes[2] & 0x7F) << 7 | (uint32_t)(bytes[3] & 0x7F);
}

static uint32_t audio_start(FlacInput input)
{
    uint8_t bytes[TAG_HEADER_BYTES];
    if (input.size < TAG_HEADER_BYTES || !input.read(input.context, 0, bytes, sizeof bytes) ||
        !same_tag(bytes, "ID3", 3)) {
        return 0;
    }
    uint32_t footer = (bytes[5] & TAG_FOOTER_FLAG) != 0 ? TAG_FOOTER_BYTES : 0;
    return TAG_HEADER_BYTES + syncsafe(bytes + 6) + footer;
}

static uint32_t audio_end(FlacInput input, uint32_t start)
{
    uint32_t end = input.size;
    uint8_t bytes[APE_FOOTER_BYTES];
    if (end - start >= ID3V1_BYTES && input.read(input.context, end - ID3V1_BYTES, bytes, 3) &&
        same_tag(bytes, "TAG", 3)) {
        end -= ID3V1_BYTES;
    }
    if (end - start >= APE_FOOTER_BYTES &&
        input.read(input.context, end - APE_FOOTER_BYTES, bytes, sizeof bytes) &&
        same_tag(bytes, "APETAGEX", 8)) {
        uint32_t size = little_endian(bytes + 12);
        uint32_t whole = size + ((bytes[23] & APE_HEADER_FLAG) != 0 ? APE_FOOTER_BYTES : 0);
        if (whole <= end - start) {
            end -= whole;
        }
    }
    return end;
}

static bool confirmed(FlacInput input, uint32_t offset, const Header *header, uint32_t end)
{
    uint8_t bytes[HEADER_BYTES];
    Header next;
    uint32_t following = offset + header->length;
    if (following + HEADER_BYTES > end) {
        return following <= end;
    }
    return input.read(input.context, following, bytes, sizeof bytes) &&
           parse_header(bytes, &next) && next.version == header->version &&
           next.layer == header->layer && next.rate == header->rate;
}

static bool find_frame(FlacInput input, uint32_t start, uint32_t end, uint32_t *offset,
                       Header *header)
{
    uint8_t window[SCAN_WINDOW + HEADER_BYTES - 1];
    uint32_t limit = end - start > SCAN_LIMIT ? start + SCAN_LIMIT : end;
    for (uint32_t base = start; base + HEADER_BYTES <= limit; base += SCAN_WINDOW) {
        uint32_t length = end - base < sizeof window ? end - base : (uint32_t)sizeof window;
        if (!input.read(input.context, base, window, length)) {
            return false;
        }
        for (uint32_t index = 0; index + HEADER_BYTES <= length && index < SCAN_WINDOW; index++) {
            if (parse_header(window + index, header) &&
                confirmed(input, base + index, header, end)) {
                *offset = base + index;
                return true;
            }
        }
    }
    return false;
}

static uint32_t side_info_bytes(const Header *header)
{
    if (header->version == MPEG_1) {
        return header->mono ? 17 : 32;
    }
    return header->mono ? 9 : 17;
}

static bool read_info(FlacInput input, uint32_t frame, const Header *header, uint32_t end,
                      Mp3Stream *stream)
{
    uint8_t bytes[INFO_BYTES] = {0};
    uint32_t length = end - frame < INFO_BYTES ? end - frame : INFO_BYTES;
    if (header->layer != 3 || !input.read(input.context, frame, bytes, length)) {
        return false;
    }
    uint32_t at = HEADER_BYTES + side_info_bytes(header);
    if (same_tag(bytes + at, "Xing", 4) || same_tag(bytes + at, "Info", 4)) {
        uint32_t flags = big_endian(bytes + at + 4);
        uint32_t field = at + 8;
        if ((flags & XING_FRAMES_FLAG) == 0) {
            return false;
        }
        stream->total_samples = big_endian(bytes + field) * header->samples;
        field += 4;
        if ((flags & XING_BYTES_FLAG) != 0) {
            field += 4;
        }
        if ((flags & XING_TOC_FLAG) != 0 && field + MP3_TOC_ENTRIES <= length) {
            for (uint32_t index = 0; index < MP3_TOC_ENTRIES; index++) {
                stream->toc[index] = bytes[field + index];
            }
            stream->has_toc = true;
        }
        return true;
    }
    if (same_tag(bytes + VBRI_OFFSET, "VBRI", 4)) {
        stream->total_samples = big_endian(bytes + VBRI_OFFSET + 14) * header->samples;
        return true;
    }
    return false;
}

bool mp3_probe(FlacInput input, Mp3Stream *stream)
{
    uint32_t start = audio_start(input);
    if (start + HEADER_BYTES > input.size) {
        return false;
    }
    uint32_t end = audio_end(input, start);
    uint32_t frame = 0;
    Header header;
    if (!find_frame(input, start, end, &frame, &header)) {
        return false;
    }
    *stream = (Mp3Stream){.sample_rate = header.rate,
                          .channels = header.mono ? 1 : 2,
                          .samples_per_frame = header.samples,
                          .audio_start = frame,
                          .audio_end = end};
    if (read_info(input, frame, &header, end, stream)) {
        stream->audio_start = frame + header.length;
        return true;
    }
    uint64_t bits = (uint64_t)(end - frame) * BITS_PER_BYTE * header.rate;
    stream->total_samples = (uint32_t)(bits / (header.kbps * KILO));
    return true;
}

uint32_t mp3_seek_offset(const Mp3Stream *stream, uint32_t sample)
{
    uint32_t bytes = stream->audio_end - stream->audio_start;
    if (stream->total_samples == 0 || sample >= stream->total_samples) {
        return sample == 0 ? stream->audio_start : stream->audio_end;
    }
    if (!stream->has_toc) {
        return stream->audio_start + (uint32_t)((uint64_t)sample * bytes / stream->total_samples);
    }
    uint32_t scaled = (uint32_t)((uint64_t)sample * PERCENT * TOC_SCALE / stream->total_samples);
    uint32_t index = scaled / TOC_SCALE;
    uint32_t fraction = scaled % TOC_SCALE;
    uint32_t low = stream->toc[index];
    uint32_t high = index + 1 < MP3_TOC_ENTRIES ? stream->toc[index + 1] : TOC_SCALE;
    uint32_t position = low * TOC_SCALE + (high > low ? (high - low) * fraction : 0);
    return stream->audio_start + (uint32_t)((uint64_t)position * bytes / (TOC_SCALE * TOC_SCALE));
}
