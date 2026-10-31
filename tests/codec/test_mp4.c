#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "codec/mp4.h"

enum {
    CAPACITY = 8192,
    FRAMES = 8,
    CHUNKS = 4,
    FRAME_SAMPLES = 4096,
    RATE = 44100,
    LEAD = 2112,
    GAP_BYTES = 5,
    FIRST_SIZE = 10
};

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t size;
} Buffer;

typedef struct Writer {
    uint8_t bytes[CAPACITY];
    uint32_t size;
} Writer;

typedef struct Layout {
    bool large_offsets;
    bool constant_size;
    bool moov_first;
    bool other_track;
} Layout;

static const uint32_t PER_CHUNK[CHUNKS] = {3, 2, 1, 2};
static const uint8_t COOKIE[24] = {0, 0, 0x10, 0, 0, 16, 40, 10, 14, 2, 0,    255,
                                   0, 0, 0,    0, 0, 0,  0,  0,  0,  0, 0xAC, 0x44};

static Writer writer;
static uint32_t chunk_offsets[CHUNKS];

static void put8(uint32_t value)
{
    writer.bytes[writer.size++] = (uint8_t)value;
}

static void put16(uint32_t value)
{
    put8(value >> 8);
    put8(value);
}

static void put32(uint32_t value)
{
    put16(value >> 16);
    put16(value);
}

static void put_type(const char *type)
{
    for (int index = 0; index < 4; index++) {
        put8((uint8_t)type[index]);
    }
}

static uint32_t begin(const char *type)
{
    uint32_t start = writer.size;
    put32(0);
    put_type(type);
    return start;
}

static void end(uint32_t start)
{
    uint32_t size = writer.size - start;
    writer.bytes[start] = (uint8_t)(size >> 24);
    writer.bytes[start + 1] = (uint8_t)(size >> 16);
    writer.bytes[start + 2] = (uint8_t)(size >> 8);
    writer.bytes[start + 3] = (uint8_t)size;
}

static uint32_t size_of(uint32_t frame, Layout layout)
{
    return layout.constant_size ? FIRST_SIZE : FIRST_SIZE + frame;
}

static uint8_t frame_byte(uint32_t frame, uint32_t index)
{
    return (uint8_t)(frame * 16 + index);
}

static void write_mdat(Layout layout)
{
    uint32_t mdat = begin("mdat");
    uint32_t frame = 0;
    for (uint32_t chunk = 0; chunk < CHUNKS; chunk++) {
        for (uint32_t gap = 0; gap < GAP_BYTES; gap++) {
            put8(0xEE);
        }
        chunk_offsets[chunk] = writer.size;
        for (uint32_t index = 0; index < PER_CHUNK[chunk]; index++, frame++) {
            for (uint32_t byte = 0; byte < size_of(frame, layout); byte++) {
                put8(frame_byte(frame, byte));
            }
        }
    }
    end(mdat);
}

static void write_full(uint32_t version_flags)
{
    put32(version_flags);
}

static void write_handler(const char *type)
{
    uint32_t hdlr = begin("hdlr");
    write_full(0);
    put32(0);
    put_type(type);
    put32(0);
    put32(0);
    put32(0);
    put8(0);
    end(hdlr);
}

static void write_sample_entry(void)
{
    uint32_t stsd = begin("stsd");
    write_full(0);
    put32(1);
    uint32_t entry = begin("alac");
    for (int index = 0; index < 6; index++) {
        put8(0);
    }
    put16(1);
    put16(0);
    put16(0);
    put32(0);
    put16(2);
    put16(16);
    put16(0);
    put16(0);
    put32((uint32_t)RATE << 16);
    uint32_t cookie = begin("alac");
    write_full(0);
    for (uint32_t index = 0; index < sizeof COOKIE; index++) {
        put8(COOKIE[index]);
    }
    end(cookie);
    end(entry);
    end(stsd);
}

static void write_tables(Layout layout)
{
    uint32_t stts = begin("stts");
    write_full(0);
    put32(1);
    put32(FRAMES);
    put32(FRAME_SAMPLES);
    end(stts);
    uint32_t stsc = begin("stsc");
    write_full(0);
    put32(CHUNKS);
    for (uint32_t chunk = 0; chunk < CHUNKS; chunk++) {
        put32(chunk + 1);
        put32(PER_CHUNK[chunk]);
        put32(1);
    }
    end(stsc);
    uint32_t stsz = begin("stsz");
    write_full(0);
    put32(layout.constant_size ? FIRST_SIZE : 0);
    put32(FRAMES);
    for (uint32_t frame = 0; !layout.constant_size && frame < FRAMES; frame++) {
        put32(size_of(frame, layout));
    }
    end(stsz);
    uint32_t stco = begin(layout.large_offsets ? "co64" : "stco");
    write_full(0);
    put32(CHUNKS);
    for (uint32_t chunk = 0; chunk < CHUNKS; chunk++) {
        if (layout.large_offsets) {
            put32(0);
        }
        put32(chunk_offsets[chunk]);
    }
    end(stco);
}

static void write_sound_track(Layout layout)
{
    uint32_t trak = begin("trak");
    uint32_t edts = begin("edts");
    uint32_t elst = begin("elst");
    write_full(0);
    put32(2);
    put32(100);
    put32(UINT32_MAX);
    put32(0x10000);
    put32(1000);
    put32(LEAD);
    put32(0x10000);
    end(elst);
    end(edts);
    uint32_t mdia = begin("mdia");
    uint32_t mdhd = begin("mdhd");
    write_full(0);
    put32(0);
    put32(0);
    put32(RATE);
    put32(FRAMES * FRAME_SAMPLES);
    put32(0);
    end(mdhd);
    write_handler("soun");
    uint32_t minf = begin("minf");
    uint32_t stbl = begin("stbl");
    write_sample_entry();
    write_tables(layout);
    end(stbl);
    end(minf);
    end(mdia);
    end(trak);
}

static void write_other_track(void)
{
    uint32_t trak = begin("trak");
    uint32_t mdia = begin("mdia");
    write_handler("text");
    end(mdia);
    end(trak);
}

static void write_moov(Layout layout)
{
    uint32_t moov = begin("moov");
    if (layout.other_track) {
        write_other_track();
    }
    write_sound_track(layout);
    end(moov);
}

static Buffer build(Layout layout)
{
    writer.size = 0;
    uint32_t ftyp = begin("ftyp");
    put_type("M4A ");
    put32(0);
    end(ftyp);
    if (layout.moov_first) {
        uint32_t start = writer.size;
        write_moov(layout);
        write_mdat(layout);
        writer.size = start;
        write_moov(layout);
        write_mdat(layout);
    } else {
        write_mdat(layout);
        write_moov(layout);
    }
    return (Buffer){writer.bytes, writer.size};
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

static Buffer drain(FlacInput input, uint32_t piece_bytes)
{
    Buffer out = {malloc(input.size), input.size};
    uint32_t done = 0;
    while (out.bytes != NULL && done < input.size) {
        uint32_t piece = input.size - done < piece_bytes ? input.size - done : piece_bytes;
        if (!input.read(input.context, done, out.bytes + done, piece)) {
            free(out.bytes);
            return (Buffer){NULL, 0};
        }
        done += piece;
    }
    return out;
}

static bool records_match(const Buffer *records, uint32_t first_frame, Layout layout)
{
    bool ok = records->size >= 4 && mp4_record_length(records->bytes) == sizeof COOKIE &&
              memcmp(records->bytes + 4, COOKIE, sizeof COOKIE) == 0;
    uint32_t at = 4 + sizeof COOKIE;
    for (uint32_t frame = first_frame; ok && frame < FRAMES; frame++) {
        uint32_t size = size_of(frame, layout);
        ok = at + 4 + size <= records->size && mp4_record_length(records->bytes + at) == size;
        for (uint32_t byte = 0; ok && byte < size; byte++) {
            ok = records->bytes[at + 4 + byte] == frame_byte(frame, byte);
        }
        at += 4 + size;
    }
    return ok && at == records->size;
}

static bool streams_from(Layout layout, uint32_t sample, uint32_t frame, uint32_t piece_bytes)
{
    Buffer file = build(layout);
    Mp4Track track;
    Mp4Stream stream;
    uint32_t first = 0;
    bool ok = mp4_probe(input_of(&file), &track) &&
              mp4_stream_open(&stream, input_of(&file), &track, sample, &first) &&
              first == frame * FRAME_SAMPLES;
    Buffer records = ok ? drain(mp4_stream_input(&stream), piece_bytes) : (Buffer){NULL, 0};
    ok = ok && records.bytes != NULL && records_match(&records, frame, layout);
    free(records.bytes);
    return ok;
}

static void the_probe_reads_the_sound_track(void)
{
    Buffer file = build((Layout){.other_track = true});
    Mp4Track track;
    CHECK(mp4_signature(file.bytes));
    CHECK(mp4_probe(input_of(&file), &track));
    CHECK_EQUAL(MP4_CODEC_ALAC, track.codec);
    CHECK_EQUAL(RATE, track.sample_rate);
    CHECK_EQUAL(2, track.channels);
    CHECK_EQUAL(16, track.bits_per_sample);
    CHECK_EQUAL(FRAMES, track.frames);
    CHECK_EQUAL(FRAMES * FRAME_SAMPLES, track.total_samples);
    CHECK_EQUAL(LEAD, track.lead_trim);
}

static void frames_come_out_in_order_across_chunks(void)
{
    CHECK(streams_from((Layout){0}, 0, 0, 1));
    CHECK(streams_from((Layout){0}, 0, 0, 7));
    CHECK(streams_from((Layout){0}, 0, 0, 4096));
}

static void seeking_lands_inside_any_chunk(void)
{
    for (uint32_t frame = 0; frame <= FRAMES; frame++) {
        CHECK(streams_from((Layout){0}, frame * FRAME_SAMPLES + 17, frame, 5));
    }
}

static void large_offsets_and_constant_sizes_work(void)
{
    CHECK(streams_from((Layout){.large_offsets = true}, 3 * FRAME_SAMPLES, 3, 9));
    CHECK(streams_from((Layout){.constant_size = true}, 4 * FRAME_SAMPLES, 4, 3));
    CHECK(streams_from((Layout){.constant_size = true, .large_offsets = true}, 0, 0, 11));
}

static void the_index_can_come_first(void)
{
    CHECK(
        streams_from((Layout){.moov_first = true, .other_track = true}, 5 * FRAME_SAMPLES, 5, 13));
}

static void reads_must_follow_on(void)
{
    Buffer file = build((Layout){0});
    Mp4Track track;
    Mp4Stream stream;
    uint32_t first = 0;
    uint8_t bytes[8];
    CHECK(mp4_probe(input_of(&file), &track));
    CHECK(mp4_stream_open(&stream, input_of(&file), &track, 0, &first));
    FlacInput input = mp4_stream_input(&stream);
    CHECK(input.read(input.context, 0, bytes, sizeof bytes));
    CHECK(!input.read(input.context, 0, bytes, sizeof bytes));
    CHECK(!input.read(input.context, 16, bytes, sizeof bytes));
    CHECK(input.read(input.context, 8, bytes, sizeof bytes));
    CHECK(!input.read(input.context, 16, bytes, input.size));
}

static void a_file_without_sound_is_refused(void)
{
    writer.size = 0;
    uint32_t ftyp = begin("ftyp");
    put_type("M4A ");
    end(ftyp);
    uint32_t moov = begin("moov");
    write_other_track();
    end(moov);
    Buffer file = {writer.bytes, writer.size};
    Mp4Track track;
    CHECK(!mp4_probe(input_of(&file), &track));
    CHECK_EQUAL(MP4_CODEC_NONE, track.codec);
    uint8_t flac[12] = {'f', 'L', 'a', 'C'};
    CHECK(!mp4_signature(flac));
}

static Buffer load_fixture(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/tags/%s", ECHO_FIXTURE_DIR, name);
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

static void aac_tracks_report_their_config(void)
{
    Buffer file = load_fixture("aac.m4a");
    Mp4Track track;
    CHECK(mp4_probe(input_of(&file), &track));
    CHECK_EQUAL(MP4_CODEC_AAC, track.codec);
    CHECK_EQUAL(44100, track.sample_rate);
    CHECK_EQUAL(2, track.channels);
    CHECK_EQUAL(2, track.object_type);
    CHECK_EQUAL(0, track.bits_per_sample);
    CHECK_EQUAL(45, track.frames);
    CHECK_EQUAL(1024, track.lead_trim);
    CHECK_EQUAL(45124, track.total_samples);
    CHECK(track.config_bytes >= 2);
    CHECK_EQUAL(0x12, track.config[0]);
    CHECK_EQUAL(0x10, track.config[1]);
    free(file.bytes);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_probe_reads_the_sound_track),
        TEST_CASE(frames_come_out_in_order_across_chunks),
        TEST_CASE(seeking_lands_inside_any_chunk),
        TEST_CASE(large_offsets_and_constant_sizes_work),
        TEST_CASE(the_index_can_come_first),
        TEST_CASE(reads_must_follow_on),
        TEST_CASE(a_file_without_sound_is_refused),
        TEST_CASE(aac_tracks_report_their_config),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
