#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "codec/ogg.h"

enum {
    PACKET_CAPACITY = 4096,
    MAX_PACKETS = 256,
    REPAGED_CAPACITY = 64 * 1024
};

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t size;
} Buffer;

typedef struct Packets {
    uint32_t count;
    uint32_t length[MAX_PACKETS];
    uint32_t first[MAX_PACKETS];
    uint32_t samples[MAX_PACKETS];
    uint8_t head[MAX_PACKETS][8];
} Packets;

static uint8_t packet_buffer[PACKET_CAPACITY];
static OggReader reader;
static Packets whole;
static Packets part;

static Buffer load(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/opus/%s.opus", ECHO_FIXTURE_DIR, name);
    Buffer buffer = {NULL, 0};
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return buffer;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer.bytes = malloc((size_t)size);
    if (buffer.bytes != NULL && fread(buffer.bytes, 1, (size_t)size, file) == (size_t)size) {
        buffer.size = (uint32_t)size;
    }
    fclose(file);
    return buffer;
}

static bool read_memory(void *context, uint32_t offset, void *destination, uint32_t length)
{
    const Buffer *buffer = context;
    if (offset > buffer->size || length > buffer->size - offset) {
        return false;
    }
    memcpy(destination, buffer->bytes + offset, length);
    return true;
}

static FlacInput input_of(Buffer *buffer)
{
    return (FlacInput){.read = read_memory, .context = buffer, .size = buffer->size};
}

static bool read_packets(Buffer *file, uint32_t start, uint32_t serial, Packets *out)
{
    ogg_reader_start(&reader, input_of(file), start, serial, packet_buffer, sizeof packet_buffer);
    out->count = 0;
    OggPacket packet;
    OggResult result = OGG_PACKET;
    while ((result = ogg_reader_next(&reader, &packet)) == OGG_PACKET && out->count < MAX_PACKETS) {
        uint32_t index = out->count++;
        out->length[index] = packet.length;
        out->first[index] = packet.first_sample;
        out->samples[index] = packet.samples;
        memset(out->head[index], 0, sizeof out->head[index]);
        memcpy(out->head[index], packet.data, packet.length < 8 ? packet.length : 8);
    }
    return result == OGG_END;
}

static bool continues_whole(const Packets *later)
{
    for (uint32_t offset = 0; offset < whole.count; offset++) {
        bool same = later->count > 0 && offset + later->count == whole.count;
        for (uint32_t index = 0; same && index < later->count; index++) {
            same = later->length[index] == whole.length[offset + index] &&
                   later->first[index] == whole.first[offset + index] &&
                   memcmp(later->head[index], whole.head[offset + index], 8) == 0;
        }
        if (same) {
            return true;
        }
    }
    return false;
}

static void put_page(Buffer *out, uint32_t serial, uint32_t sequence, uint8_t flags,
                     uint32_t granule, bool has_granule, const uint8_t *lacing, uint32_t segments,
                     const uint8_t *body, uint32_t body_bytes)
{
    uint8_t *page = out->bytes + out->size;
    memcpy(page, "OggS", 4);
    page[4] = 0;
    page[5] = flags;
    uint32_t low = has_granule ? granule : UINT32_MAX;
    uint32_t high = has_granule ? 0 : UINT32_MAX;
    for (int index = 0; index < 4; index++) {
        page[6 + index] = (uint8_t)(low >> (8 * index));
        page[10 + index] = (uint8_t)(high >> (8 * index));
        page[14 + index] = (uint8_t)(serial >> (8 * index));
        page[18 + index] = (uint8_t)(sequence >> (8 * index));
        page[22 + index] = 0;
    }
    page[26] = (uint8_t)segments;
    memcpy(page + 27, lacing, segments);
    memcpy(page + 27 + segments, body, body_bytes);
    out->size += 27 + segments + body_bytes;
}

static Buffer repage(Buffer *file, const OggOpusInfo *info, uint32_t per_page)
{
    Buffer out = {malloc(REPAGED_CAPACITY), 0};
    memcpy(out.bytes, file->bytes, info->audio_start);
    out.size = info->audio_start;
    uint8_t lacing[255];
    uint8_t body[255 * 255];
    uint32_t segments = 0;
    uint32_t body_bytes = 0;
    uint32_t sequence = 100;
    uint32_t granule = 0;
    bool ended = false;
    bool continued = false;
    ogg_reader_start(&reader, input_of(file), info->audio_start, info->serial, packet_buffer,
                     sizeof packet_buffer);
    OggPacket packet;
    while (ogg_reader_next(&reader, &packet) == OGG_PACKET) {
        uint32_t left = packet.length;
        const uint8_t *data = packet.data;
        for (;;) {
            uint32_t lace = left >= 255 ? 255 : left;
            lacing[segments++] = (uint8_t)lace;
            memcpy(body + body_bytes, data, lace);
            body_bytes += lace;
            data += lace;
            left -= lace;
            bool last = lace < 255;
            if (last) {
                granule = packet.first_sample + packet.samples;
                ended = true;
            }
            if (segments == per_page) {
                put_page(&out, info->serial, sequence++, continued ? 1 : 0, granule, ended, lacing,
                         segments, body, body_bytes);
                continued = !last;
                segments = 0;
                body_bytes = 0;
                ended = false;
            }
            if (last) {
                break;
            }
        }
    }
    if (segments > 0) {
        put_page(&out, info->serial, sequence, (uint8_t)(4 | (continued ? 1 : 0)),
                 info->last_granule, true, lacing, segments, body, body_bytes);
    } else {
        uint32_t last = out.size;
        while (last > 0 && memcmp(out.bytes + last - 1, "OggS", 4) != 0) {
            last--;
        }
        for (uint32_t index = 0; last > 0 && index < 4; index++) {
            out.bytes[last - 1 + 6 + index] = (uint8_t)(info->last_granule >> (8 * index));
        }
    }
    return out;
}

static void the_probe_reads_the_opus_header(void)
{
    Buffer file = load("stereo_small_pages");
    OggOpusInfo info;
    CHECK(ogg_signature(file.bytes));
    CHECK(ogg_opus_probe(input_of(&file), &info));
    CHECK_EQUAL(2, info.channels);
    CHECK_EQUAL(312, info.pre_skip);
    CHECK_EQUAL(48000, info.input_rate);
    CHECK_EQUAL(0, info.gain);
    CHECK_EQUAL(19512, info.last_granule);
    CHECK(info.audio_start > 40 && info.audio_start < 400);
    CHECK(memcmp(file.bytes + info.audio_start, "OggS", 4) == 0);
    free(file.bytes);
}

static void every_fixture_reads_as_whole_packets(void)
{
    static const char *const NAMES[] = {"stereo_small_pages", "mono_60ms_speech", "stereo_low_rate",
                                        "stereo_high_rate"};
    static const uint32_t FRAMES[] = {960, 2880, 960, 960};
    for (uint32_t name = 0; name < 4; name++) {
        Buffer file = load(NAMES[name]);
        OggOpusInfo info;
        CHECK(ogg_opus_probe(input_of(&file), &info));
        CHECK(read_packets(&file, info.audio_start, info.serial, &whole));
        CHECK(whole.count > 2);
        CHECK_EQUAL(0, whole.first[0]);
        bool steady = true;
        for (uint32_t index = 0; index < whole.count; index++) {
            steady = steady && whole.samples[index] == FRAMES[name] &&
                     whole.first[index] == index * FRAMES[name];
        }
        CHECK(steady);
        uint32_t end = whole.first[whole.count - 1] + whole.samples[whole.count - 1];
        CHECK(end >= info.last_granule && end - info.last_granule < FRAMES[name]);
        free(file.bytes);
    }
}

static void packets_that_span_pages_come_out_whole(void)
{
    Buffer file = load("stereo_high_rate");
    OggOpusInfo info;
    CHECK(ogg_opus_probe(input_of(&file), &info));
    CHECK(read_packets(&file, info.audio_start, info.serial, &whole));
    CHECK(whole.length[3] > 255);
    Buffer paged = repage(&file, &info, 2);
    OggOpusInfo repaged;
    CHECK(ogg_opus_probe(input_of(&paged), &repaged));
    CHECK_EQUAL(info.last_granule, repaged.last_granule);
    CHECK(read_packets(&paged, repaged.audio_start, repaged.serial, &part));
    CHECK_EQUAL(whole.count, part.count);
    CHECK(continues_whole(&part));
    free(paged.bytes);
    free(file.bytes);
}

static bool seek_lands_early_enough(Buffer *file, uint32_t granule)
{
    OggOpusInfo info;
    if (!ogg_opus_probe(input_of(file), &info) ||
        !read_packets(file, info.audio_start, info.serial, &whole)) {
        return false;
    }
    uint32_t start = ogg_opus_seek(input_of(file), &info, granule);
    bool ok = start >= info.audio_start && memcmp(file->bytes + start, "OggS", 4) == 0 &&
              read_packets(file, start, info.serial, &part) && continues_whole(&part);
    uint32_t preroll = granule > OGG_OPUS_PREROLL ? granule - OGG_OPUS_PREROLL : 0;
    return ok && part.first[0] <= preroll;
}

static void seeking_starts_before_the_preroll(void)
{
    Buffer file = load("stereo_small_pages");
    CHECK(seek_lands_early_enough(&file, 0));
    CHECK(seek_lands_early_enough(&file, 5000));
    CHECK(seek_lands_early_enough(&file, 15000));
    CHECK(seek_lands_early_enough(&file, 19512));
    OggOpusInfo info;
    CHECK(ogg_opus_probe(input_of(&file), &info));
    CHECK(ogg_opus_seek(input_of(&file), &info, 18000) > info.audio_start);
    free(file.bytes);
}

static void seeking_into_spanning_packets_skips_the_cut_one(void)
{
    Buffer file = load("stereo_high_rate");
    OggOpusInfo info;
    CHECK(ogg_opus_probe(input_of(&file), &info));
    Buffer paged = repage(&file, &info, 2);
    CHECK(seek_lands_early_enough(&paged, 8000));
    CHECK(seek_lands_early_enough(&paged, 10000));
    free(paged.bytes);
    free(file.bytes);
}

static void a_missing_byte_can_be_retried(void)
{
    Buffer file = load("stereo_small_pages");
    OggOpusInfo info;
    CHECK(ogg_opus_probe(input_of(&file), &info));
    CHECK(read_packets(&file, info.audio_start, info.serial, &whole));
    Buffer partial = {file.bytes, info.audio_start + 500};
    FlacInput input = {.read = read_memory, .context = &partial, .size = file.size};
    ogg_reader_start(&reader, input, info.audio_start, info.serial, packet_buffer,
                     sizeof packet_buffer);
    OggPacket packet;
    uint32_t count = 0;
    OggResult result = OGG_PACKET;
    while ((result = ogg_reader_next(&reader, &packet)) == OGG_PACKET) {
        count++;
    }
    CHECK_EQUAL(OGG_MISSING, result);
    partial.size = file.size;
    while ((result = ogg_reader_next(&reader, &packet)) == OGG_PACKET) {
        CHECK_EQUAL(whole.first[count], packet.first_sample);
        CHECK_EQUAL(whole.length[count], packet.length);
        count++;
    }
    CHECK_EQUAL(OGG_END, result);
    CHECK_EQUAL(whole.count, count);
    free(file.bytes);
}

static void durations_follow_the_toc(void)
{
    uint8_t silk_60[] = {(uint8_t)(3 << 3)};
    uint8_t hybrid_10[] = {(uint8_t)(12 << 3)};
    uint8_t celt_2_5_pair[] = {(uint8_t)(16 << 3 | 1)};
    uint8_t celt_20_many[] = {(uint8_t)(31 << 3 | 3), 5};
    CHECK_EQUAL(2880, ogg_opus_packet_samples(silk_60, 1));
    CHECK_EQUAL(480, ogg_opus_packet_samples(hybrid_10, 1));
    CHECK_EQUAL(240, ogg_opus_packet_samples(celt_2_5_pair, 1));
    CHECK_EQUAL(4800, ogg_opus_packet_samples(celt_20_many, 2));
    CHECK_EQUAL(0, ogg_opus_packet_samples(celt_20_many, 1));
    CHECK_EQUAL(0, ogg_opus_packet_samples(silk_60, 0));
}

static void vorbis_and_broken_files_are_refused(void)
{
    Buffer file = load("stereo_small_pages");
    OggOpusInfo info;
    file.bytes[28 + 4] = 'X';
    CHECK(!ogg_opus_probe(input_of(&file), &info));
    file.bytes[28 + 4] = 'H';
    CHECK(ogg_opus_probe(input_of(&file), &info));
    Buffer cut = {file.bytes, 100};
    CHECK(!ogg_opus_probe(input_of(&cut), &info));
    uint8_t flac[4] = {'f', 'L', 'a', 'C'};
    CHECK(!ogg_signature(flac));
    free(file.bytes);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_probe_reads_the_opus_header),
        TEST_CASE(every_fixture_reads_as_whole_packets),
        TEST_CASE(packets_that_span_pages_come_out_whole),
        TEST_CASE(seeking_starts_before_the_preroll),
        TEST_CASE(seeking_into_spanning_packets_skips_the_cut_one),
        TEST_CASE(a_missing_byte_can_be_retried),
        TEST_CASE(durations_follow_the_toc),
        TEST_CASE(vorbis_and_broken_files_are_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
