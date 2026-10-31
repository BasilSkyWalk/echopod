#include "codec/mp4.h"

#include <stddef.h>

#include "codec/alac.h"

enum {
    TYPE_FTYP = 0x66747970,
    TYPE_MOOV = 0x6D6F6F76,
    TYPE_TRAK = 0x7472616B,
    TYPE_MDIA = 0x6D646961,
    TYPE_MDHD = 0x6D646864,
    TYPE_HDLR = 0x68646C72,
    TYPE_MINF = 0x6D696E66,
    TYPE_STBL = 0x7374626C,
    TYPE_STSD = 0x73747364,
    TYPE_STTS = 0x73747473,
    TYPE_STSC = 0x73747363,
    TYPE_STSZ = 0x7374737A,
    TYPE_STCO = 0x7374636F,
    TYPE_CO64 = 0x636F3634,
    TYPE_EDTS = 0x65647473,
    TYPE_ELST = 0x656C7374,
    TYPE_ALAC = 0x616C6163,
    TYPE_MP4A = 0x6D703461,
    TYPE_ESDS = 0x65736473,
    TYPE_WAVE = 0x77617665,
    HANDLER_SOUND = 0x736F756E,
    ATOM_HEADER = 8,
    LARGE_ATOM_HEADER = 16,
    FULL_HEADER = 4,
    SAMPLE_ENTRY_BYTES = 28,
    SAMPLE_ENTRY_V1_EXTRA = 16,
    SAMPLE_ENTRY_V2_EXTRA = 36,
    ENTRY_VERSION_AT = 8,
    ENTRY_CHANNELS_AT = 16,
    ENTRY_BITS_AT = 18,
    ENTRY_RATE_AT = 24,
    FIXED_POINT_SHIFT = 16,
    HANDLER_TYPE_AT = 8,
    MDHD_TIMESCALE_V0 = 12,
    MDHD_TIMESCALE_V1 = 20,
    STSZ_COUNT_AT = 8,
    ELST_COUNT_AT = 4,
    ELST_FIRST_ENTRY = 8,
    ELST_V0_ENTRY = 12,
    ELST_V1_ENTRY = 20,
    EMPTY_EDIT = -1,
    TIME_WIDTH = 8,
    CHUNK_WIDTH = 12,
    SIZE_WIDTH = 4,
    OFFSET_WIDTH = 4,
    LARGE_OFFSET_WIDTH = 8,
    DESCRIPTOR_ES = 3,
    DESCRIPTOR_DECODER = 4,
    DESCRIPTOR_SPECIFIC = 5,
    DESCRIPTOR_LENGTH_BYTES = 4,
    ES_FIXED_BYTES = 3,
    ES_DEPENDS = 0x80,
    ES_URL = 0x40,
    ES_OCR = 0x20,
    DECODER_FIXED_BYTES = 13,
    OBJECT_MPEG4_AUDIO = 0x40,
    OBJECT_MPEG2_AAC_MAIN = 0x66,
    OBJECT_MPEG2_AAC_SSR = 0x68,
    AOT_ESCAPE = 31,
    AOT_ESCAPE_BASE = 32,
    AOT_SBR = 5,
    AOT_PS = 29,
    RATE_ESCAPE = 15,
    RATE_INDEXES = 13,
    AAC_FRAME_SAMPLES = 1024,
    AAC_SHORT_FRAME_SAMPLES = 960,
    BYTE_BITS = 8,
    SEVEN_BITS = 0x7F,
    MORE_BIT = 0x80
};

static const uint32_t AAC_RATES[RATE_INDEXES] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                                 22050, 16000, 12000, 11025, 8000,  7350};

typedef struct Atom {
    uint32_t type;
    uint32_t body;
    uint32_t end;
} Atom;

typedef struct ConfigBits {
    const uint8_t *bytes;
    uint32_t length;
    uint32_t position;
} ConfigBits;

static uint32_t big_endian(const uint8_t *bytes, int count)
{
    uint32_t value = 0;
    for (int index = 0; index < count; index++) {
        value = value << BYTE_BITS | bytes[index];
    }
    return value;
}

static uint32_t smaller(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static uint32_t scale(uint64_t value, uint32_t to, uint32_t from)
{
    uint64_t scaled = to == from ? value : value * to / from;
    return scaled > UINT32_MAX ? UINT32_MAX : (uint32_t)scaled;
}

static bool read_at(FlacInput file, uint32_t offset, void *buffer, uint32_t length)
{
    return offset <= file.size && length <= file.size - offset &&
           file.read(file.context, offset, buffer, length);
}

static bool read_word(FlacInput file, uint32_t offset, uint32_t *value)
{
    uint8_t bytes[4];
    if (!read_at(file, offset, bytes, sizeof bytes)) {
        return false;
    }
    *value = big_endian(bytes, 4);
    return true;
}

bool mp4_signature(const uint8_t *bytes)
{
    return big_endian(bytes + 4, 4) == TYPE_FTYP;
}

uint32_t mp4_record_length(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool atom_at(FlacInput file, uint32_t offset, uint32_t limit, Atom *atom)
{
    uint8_t header[LARGE_ATOM_HEADER];
    if (offset > limit || limit - offset < ATOM_HEADER ||
        !read_at(file, offset, header, ATOM_HEADER)) {
        return false;
    }
    uint32_t size = big_endian(header, 4);
    atom->type = big_endian(header + 4, 4);
    atom->body = offset + ATOM_HEADER;
    if (size == 1) {
        if (limit - offset < LARGE_ATOM_HEADER ||
            !read_at(file, offset + ATOM_HEADER, header + ATOM_HEADER, ATOM_HEADER) ||
            big_endian(header + ATOM_HEADER, 4) != 0) {
            return false;
        }
        size = big_endian(header + ATOM_HEADER + 4, 4);
        atom->body = offset + LARGE_ATOM_HEADER;
    } else if (size == 0) {
        size = limit - offset;
    }
    if (size < atom->body - offset || size > limit - offset) {
        return false;
    }
    atom->end = offset + size;
    return true;
}

static bool find_atom(FlacInput file, uint32_t from, uint32_t limit, uint32_t type, Atom *atom)
{
    uint32_t offset = from;
    while (atom_at(file, offset, limit, atom)) {
        if (atom->type == type) {
            return true;
        }
        offset = atom->end;
    }
    return false;
}

static bool find_inside(FlacInput file, Atom parent, uint32_t type, Atom *atom)
{
    return find_atom(file, parent.body, parent.end, type, atom);
}

static bool find_codec_atom(FlacInput file, uint32_t from, uint32_t limit, uint32_t type,
                            Atom *atom)
{
    if (find_atom(file, from, limit, type, atom)) {
        return true;
    }
    Atom wave;
    return find_atom(file, from, limit, TYPE_WAVE, &wave) && find_inside(file, wave, type, atom);
}

static bool read_table(FlacInput file, Atom atom, uint32_t count_at, uint32_t width,
                       Mp4Table *table)
{
    uint32_t count = 0;
    if (!read_word(file, atom.body + count_at, &count)) {
        return false;
    }
    *table = (Mp4Table){.offset = atom.body + count_at + 4, .count = count, .width = width};
    return table->offset <= atom.end && count <= (atom.end - table->offset) / width;
}

static uint32_t config_read(ConfigBits *bits, uint32_t count)
{
    uint32_t value = 0;
    for (uint32_t index = 0; index < count; index++) {
        uint32_t position = bits->position++;
        uint32_t bit = 0;
        if (position / BYTE_BITS < bits->length) {
            bit = bits->bytes[position / BYTE_BITS] >> (BYTE_BITS - 1 - position % BYTE_BITS) & 1u;
        }
        value = value << 1 | bit;
    }
    return value;
}

static uint32_t config_rate(ConfigBits *bits)
{
    uint32_t index = config_read(bits, 4);
    if (index == RATE_ESCAPE) {
        return config_read(bits, 24);
    }
    return index < RATE_INDEXES ? AAC_RATES[index] : 0;
}

static bool read_audio_config(Mp4Track *track)
{
    ConfigBits bits = {track->config, track->config_bytes, 0};
    uint32_t object = config_read(&bits, 5);
    if (object == AOT_ESCAPE) {
        object = AOT_ESCAPE_BASE + config_read(&bits, 6);
    }
    track->sample_rate = config_rate(&bits);
    track->channels = (uint8_t)config_read(&bits, 4);
    track->frame_samples = AAC_FRAME_SAMPLES;
    if (object == AOT_SBR || object == AOT_PS) {
        track->sample_rate = config_rate(&bits);
    } else if (config_read(&bits, 1) != 0) {
        track->frame_samples = AAC_SHORT_FRAME_SAMPLES;
    }
    track->object_type = (uint8_t)object;
    track->bits_per_sample = 0;
    track->codec = MP4_CODEC_AAC;
    return true;
}

static bool read_descriptor(FlacInput file, uint32_t *offset, uint32_t end, uint32_t *tag,
                            uint32_t *length)
{
    uint8_t byte = 0;
    if (!read_at(file, *offset, &byte, 1)) {
        return false;
    }
    *tag = byte;
    (*offset)++;
    uint32_t value = 0;
    for (int index = 0; index < DESCRIPTOR_LENGTH_BYTES; index++) {
        if (!read_at(file, (*offset)++, &byte, 1)) {
            return false;
        }
        value = value << 7 | (byte & SEVEN_BITS);
        if ((byte & MORE_BIT) == 0) {
            break;
        }
    }
    *length = value;
    return *offset <= end && value <= end - *offset;
}

static uint32_t es_extra(FlacInput file, uint32_t offset, uint8_t flags)
{
    uint32_t extra = 0;
    if ((flags & ES_DEPENDS) != 0) {
        extra += 2;
    }
    if ((flags & ES_URL) != 0) {
        uint8_t length = 0;
        read_at(file, offset + extra, &length, 1);
        extra += 1u + length;
    }
    if ((flags & ES_OCR) != 0) {
        extra += 2;
    }
    return extra;
}

static bool aac_object(uint8_t object)
{
    return object == OBJECT_MPEG4_AUDIO ||
           (object >= OBJECT_MPEG2_AAC_MAIN && object <= OBJECT_MPEG2_AAC_SSR);
}

static bool read_aac(FlacInput file, uint32_t from, uint32_t limit, Mp4Track *track)
{
    Atom esds;
    if (!find_codec_atom(file, from, limit, TYPE_ESDS, &esds)) {
        return false;
    }
    uint32_t offset = esds.body + FULL_HEADER;
    uint32_t tag = 0;
    uint32_t length = 0;
    uint8_t fixed[ES_FIXED_BYTES];
    bool decoder_ok = false;
    while (read_descriptor(file, &offset, esds.end, &tag, &length)) {
        if (tag == DESCRIPTOR_ES && read_at(file, offset, fixed, sizeof fixed)) {
            offset += ES_FIXED_BYTES + es_extra(file, offset + ES_FIXED_BYTES, fixed[2]);
        } else if (tag == DESCRIPTOR_DECODER && read_at(file, offset, fixed, 1)) {
            decoder_ok = aac_object(fixed[0]);
            offset += DECODER_FIXED_BYTES;
        } else if (tag == DESCRIPTOR_SPECIFIC) {
            uint32_t bytes = smaller(length, MP4_CONFIG_CAPACITY);
            if (!decoder_ok || bytes == 0 || !read_at(file, offset, track->config, bytes)) {
                return false;
            }
            track->config_bytes = bytes;
            return read_audio_config(track);
        } else {
            offset += length;
        }
    }
    return false;
}

static bool read_alac(FlacInput file, uint32_t from, uint32_t limit, Mp4Track *track)
{
    Atom cookie;
    AlacConfig config;
    if (!find_codec_atom(file, from, limit, TYPE_ALAC, &cookie) ||
        cookie.end - cookie.body < FULL_HEADER + ALAC_CONFIG_BYTES ||
        !read_at(file, cookie.body + FULL_HEADER, track->config, ALAC_CONFIG_BYTES) ||
        !alac_parse_config(track->config, ALAC_CONFIG_BYTES, &config)) {
        return false;
    }
    track->config_bytes = ALAC_CONFIG_BYTES;
    track->codec = MP4_CODEC_ALAC;
    track->sample_rate = config.sample_rate;
    track->channels = config.channels;
    track->bits_per_sample = config.bit_depth;
    track->frame_samples = config.frame_length;
    return true;
}

static bool read_sample_entry(FlacInput file, Atom stsd, Mp4Track *track)
{
    Atom entry;
    uint8_t fields[SAMPLE_ENTRY_BYTES];
    if (!atom_at(file, stsd.body + ATOM_HEADER, stsd.end, &entry) ||
        !read_at(file, entry.body, fields, sizeof fields)) {
        return false;
    }
    uint32_t version = big_endian(fields + ENTRY_VERSION_AT, 2);
    track->channels = (uint8_t)big_endian(fields + ENTRY_CHANNELS_AT, 2);
    track->bits_per_sample = (uint8_t)big_endian(fields + ENTRY_BITS_AT, 2);
    track->sample_rate = big_endian(fields + ENTRY_RATE_AT, 4) >> FIXED_POINT_SHIFT;
    uint32_t children = entry.body + SAMPLE_ENTRY_BYTES;
    if (version == 1) {
        children += SAMPLE_ENTRY_V1_EXTRA;
    } else if (version == 2) {
        children += SAMPLE_ENTRY_V2_EXTRA;
    }
    if (entry.type == TYPE_ALAC) {
        return read_alac(file, children, entry.end, track);
    }
    if (entry.type == TYPE_MP4A) {
        return read_aac(file, children, entry.end, track);
    }
    return false;
}

static bool sound_handler(FlacInput file, Atom mdia)
{
    Atom hdlr;
    uint32_t handler = 0;
    return find_inside(file, mdia, TYPE_HDLR, &hdlr) &&
           read_word(file, hdlr.body + HANDLER_TYPE_AT, &handler) && handler == HANDLER_SOUND;
}

static bool read_timescale(FlacInput file, Atom mdia, uint32_t *timescale)
{
    Atom mdhd;
    uint8_t version = 0;
    if (!find_inside(file, mdia, TYPE_MDHD, &mdhd) || !read_at(file, mdhd.body, &version, 1)) {
        return false;
    }
    uint32_t at = version == 1 ? MDHD_TIMESCALE_V1 : MDHD_TIMESCALE_V0;
    return read_word(file, mdhd.body + at, timescale) && *timescale > 0;
}

static uint32_t read_lead(FlacInput file, Atom trak)
{
    Atom edts;
    Atom elst;
    uint8_t version = 0;
    uint32_t count = 0;
    if (!find_inside(file, trak, TYPE_EDTS, &edts) || !find_inside(file, edts, TYPE_ELST, &elst) ||
        !read_at(file, elst.body, &version, 1) ||
        !read_word(file, elst.body + ELST_COUNT_AT, &count)) {
        return 0;
    }
    uint32_t entry_bytes = version == 1 ? ELST_V1_ENTRY : ELST_V0_ENTRY;
    uint32_t time_at = version == 1 ? 8 : 4;
    for (uint32_t index = 0; index < count; index++) {
        uint32_t entry = elst.body + ELST_FIRST_ENTRY + index * entry_bytes;
        uint32_t high = 0;
        uint32_t low = 0;
        if (entry + entry_bytes > elst.end || !read_word(file, entry + time_at, &high)) {
            return 0;
        }
        if (version == 1 && !read_word(file, entry + time_at + 4, &low)) {
            return 0;
        }
        uint32_t time = version == 1 ? low : high;
        bool empty =
            version == 1 ? (high == UINT32_MAX && low == UINT32_MAX) : (int32_t)high == EMPTY_EDIT;
        if (!empty) {
            return (version == 1 && high != 0) ? 0 : time;
        }
    }
    return 0;
}

static void cache_start(Mp4Cache *cache, Mp4Table table)
{
    cache->table = table;
    cache->first = 0;
    cache->count = 0;
}

static const uint8_t *cache_entry(Mp4Cache *cache, FlacInput file, uint32_t index)
{
    const Mp4Table *table = &cache->table;
    if (index >= table->count) {
        return NULL;
    }
    if (index < cache->first || index - cache->first >= cache->count) {
        uint32_t count = smaller(table->count - index, MP4_CACHE_BYTES / table->width);
        if (!read_at(file, table->offset + index * table->width, cache->bytes,
                     count * table->width)) {
            cache->count = 0;
            return NULL;
        }
        cache->first = index;
        cache->count = count;
    }
    return cache->bytes + (index - cache->first) * table->width;
}

static bool read_times(FlacInput file, Mp4Track *track)
{
    Mp4Cache times;
    cache_start(&times, track->times);
    uint64_t total = 0;
    for (uint32_t index = 0; index < track->times.count; index++) {
        const uint8_t *entry = cache_entry(&times, file, index);
        if (entry == NULL) {
            return false;
        }
        total += (uint64_t)big_endian(entry, 4) * big_endian(entry + 4, 4);
    }
    track->total_samples = scale(total, track->sample_rate, track->timescale);
    track->lead_trim = scale(track->lead_trim, track->sample_rate, track->timescale);
    if (track->lead_trim >= track->total_samples) {
        track->lead_trim = 0;
    }
    return track->total_samples > 0;
}

static bool read_sample_tables(FlacInput file, Atom stbl, Mp4Track *track)
{
    Atom atom;
    if (!find_inside(file, stbl, TYPE_STSD, &atom) || !read_sample_entry(file, atom, track)) {
        return false;
    }
    if (!find_inside(file, stbl, TYPE_STTS, &atom) ||
        !read_table(file, atom, FULL_HEADER, TIME_WIDTH, &track->times)) {
        return false;
    }
    if (!find_inside(file, stbl, TYPE_STSC, &atom) ||
        !read_table(file, atom, FULL_HEADER, CHUNK_WIDTH, &track->chunks)) {
        return false;
    }
    if (!find_inside(file, stbl, TYPE_STSZ, &atom) ||
        !read_word(file, atom.body + FULL_HEADER, &track->frame_size) ||
        !read_word(file, atom.body + STSZ_COUNT_AT, &track->frames)) {
        return false;
    }
    if (track->frame_size == 0 &&
        !read_table(file, atom, STSZ_COUNT_AT, SIZE_WIDTH, &track->sizes)) {
        return false;
    }
    if (find_inside(file, stbl, TYPE_STCO, &atom)) {
        return read_table(file, atom, FULL_HEADER, OFFSET_WIDTH, &track->offsets);
    }
    return find_inside(file, stbl, TYPE_CO64, &atom) &&
           read_table(file, atom, FULL_HEADER, LARGE_OFFSET_WIDTH, &track->offsets);
}

static bool read_track(FlacInput file, Atom trak, Mp4Track *track)
{
    Atom mdia;
    Atom minf;
    Atom stbl;
    *track = (Mp4Track){.codec = MP4_CODEC_NONE};
    if (!find_inside(file, trak, TYPE_MDIA, &mdia) || !sound_handler(file, mdia) ||
        !read_timescale(file, mdia, &track->timescale) ||
        !find_inside(file, mdia, TYPE_MINF, &minf) || !find_inside(file, minf, TYPE_STBL, &stbl) ||
        !read_sample_tables(file, stbl, track)) {
        return false;
    }
    track->lead_trim = read_lead(file, trak);
    bool shape_ok = track->sample_rate > 0 && track->channels >= 1 &&
                    track->channels <= FLAC_MAX_CHANNELS && track->frames > 0 &&
                    track->chunks.count > 0 && track->offsets.count > 0;
    return shape_ok && read_times(file, track);
}

bool mp4_probe(FlacInput file, Mp4Track *track)
{
    Atom moov;
    Atom trak;
    if (!find_atom(file, 0, file.size, TYPE_MOOV, &moov)) {
        return false;
    }
    uint32_t offset = moov.body;
    while (find_atom(file, offset, moov.end, TYPE_TRAK, &trak)) {
        if (read_track(file, trak, track)) {
            return true;
        }
        offset = trak.end;
    }
    *track = (Mp4Track){.codec = MP4_CODEC_NONE};
    return false;
}

static bool frame_size(Mp4Stream *stream, uint32_t frame, uint32_t *size)
{
    if (stream->track.frame_size != 0) {
        *size = stream->track.frame_size;
        return true;
    }
    const uint8_t *entry = cache_entry(&stream->sizes, stream->file, frame);
    if (entry == NULL) {
        return false;
    }
    *size = big_endian(entry, 4);
    return true;
}

static bool chunk_offset(Mp4Stream *stream, uint32_t chunk, uint32_t *offset)
{
    const uint8_t *entry = cache_entry(&stream->offsets, stream->file, chunk);
    if (entry == NULL) {
        return false;
    }
    if (stream->track.offsets.width == LARGE_OFFSET_WIDTH) {
        *offset = big_endian(entry + 4, 4);
        return big_endian(entry, 4) == 0;
    }
    *offset = big_endian(entry, 4);
    return true;
}

static bool load_entry(Mp4Stream *stream, uint32_t index, uint32_t *first_chunk)
{
    const uint8_t *entry = cache_entry(&stream->chunks, stream->file, index);
    if (entry == NULL) {
        return false;
    }
    uint32_t first = big_endian(entry, 4);
    stream->per_chunk = big_endian(entry + 4, 4);
    uint32_t end = stream->track.offsets.count;
    if (index + 1 < stream->track.chunks.count) {
        const uint8_t *next = cache_entry(&stream->chunks, stream->file, index + 1);
        if (next == NULL) {
            return false;
        }
        end = smaller(big_endian(next, 4) - 1, end);
    }
    stream->entry = index;
    stream->entry_end = end;
    *first_chunk = first - 1;
    return first > 0 && stream->per_chunk > 0;
}

static bool enter_chunk(Mp4Stream *stream, uint32_t chunk, uint32_t within)
{
    stream->chunk = chunk;
    stream->chunk_left = stream->per_chunk - within;
    if (!chunk_offset(stream, chunk, &stream->file_offset)) {
        return false;
    }
    for (uint32_t index = stream->frame - within; index < stream->frame; index++) {
        uint32_t size = 0;
        if (!frame_size(stream, index, &size)) {
            return false;
        }
        stream->file_offset += size;
    }
    return frame_size(stream, stream->frame, &stream->frame_bytes);
}

static bool seek_frame(Mp4Stream *stream, uint32_t frame)
{
    stream->frame = frame;
    if (frame >= stream->track.frames) {
        stream->frame = stream->track.frames;
        return true;
    }
    uint32_t base = 0;
    for (uint32_t index = 0; index < stream->track.chunks.count; index++) {
        uint32_t first = 0;
        if (!load_entry(stream, index, &first) || first > stream->entry_end) {
            return false;
        }
        uint64_t span = (uint64_t)(stream->entry_end - first) * stream->per_chunk;
        if (frame - base < span) {
            uint32_t offset = frame - base;
            return enter_chunk(stream, first + offset / stream->per_chunk,
                               offset % stream->per_chunk);
        }
        base += (uint32_t)span;
    }
    return false;
}

static bool advance(Mp4Stream *stream)
{
    stream->frame++;
    if (stream->frame >= stream->track.frames) {
        return true;
    }
    stream->file_offset += stream->frame_bytes;
    if (--stream->chunk_left > 0) {
        return frame_size(stream, stream->frame, &stream->frame_bytes);
    }
    uint32_t chunk = stream->chunk + 1;
    while (chunk >= stream->entry_end) {
        uint32_t first = 0;
        if (stream->entry + 1 >= stream->track.chunks.count ||
            !load_entry(stream, stream->entry + 1, &first) || first != chunk) {
            return false;
        }
    }
    return enter_chunk(stream, chunk, 0);
}

static bool locate(Mp4Stream *stream, uint32_t sample, uint32_t *frame, uint32_t *start)
{
    const Mp4Track *track = &stream->track;
    Mp4Cache times;
    cache_start(&times, track->times);
    uint64_t target = (uint64_t)sample * track->timescale / track->sample_rate;
    uint64_t time = 0;
    uint32_t index = 0;
    for (uint32_t entry_index = 0; entry_index < track->times.count; entry_index++) {
        const uint8_t *entry = cache_entry(&times, stream->file, entry_index);
        if (entry == NULL) {
            return false;
        }
        uint32_t count = big_endian(entry, 4);
        uint32_t delta = big_endian(entry + 4, 4);
        uint64_t span = (uint64_t)count * delta;
        if (delta > 0 && target < time + span) {
            uint32_t skip = (uint32_t)((target - time) / delta);
            index += skip;
            time += (uint64_t)skip * delta;
            break;
        }
        index += count;
        time += span;
    }
    *frame = index;
    *start = scale(time, track->sample_rate, track->timescale);
    return true;
}

static bool count_bytes(Mp4Stream *stream, uint32_t from, uint64_t *after)
{
    uint64_t before = 0;
    *after = 0;
    if (stream->track.frame_size != 0) {
        before = (uint64_t)stream->track.frame_size * from;
        *after = (uint64_t)stream->track.frame_size * (stream->track.frames - from);
    }
    for (uint32_t index = 0; stream->track.frame_size == 0 && index < stream->track.frames;
         index++) {
        uint32_t size = 0;
        if (!frame_size(stream, index, &size)) {
            return false;
        }
        if (index < from) {
            before += size;
        } else {
            *after += size;
        }
    }
    uint64_t total = before + *after;
    stream->data_bytes = total > UINT32_MAX ? UINT32_MAX : (uint32_t)total;
    return true;
}

bool mp4_stream_open(Mp4Stream *stream, FlacInput file, const Mp4Track *track, uint32_t sample,
                     uint32_t *first_sample)
{
    *stream = (Mp4Stream){.track = *track, .file = file};
    cache_start(&stream->sizes, track->sizes);
    cache_start(&stream->chunks, track->chunks);
    cache_start(&stream->offsets, track->offsets);
    uint32_t frame = 0;
    uint64_t after = 0;
    if (!locate(stream, sample, &frame, first_sample)) {
        return false;
    }
    frame = smaller(frame, track->frames);
    if (!count_bytes(stream, frame, &after)) {
        return false;
    }
    uint64_t size = MP4_LENGTH_BYTES + (uint64_t)track->config_bytes + after +
                    (uint64_t)MP4_LENGTH_BYTES * (track->frames - frame);
    if (size > UINT32_MAX) {
        return false;
    }
    stream->size = (uint32_t)size;
    return seek_frame(stream, frame);
}

static void length_bytes(uint32_t value, uint32_t from, uint8_t *out, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        out[index] = (uint8_t)(value >> (BYTE_BITS * (from + index)));
    }
}

static uint32_t emit_config(Mp4Stream *stream, uint8_t *out, uint32_t length)
{
    uint32_t at = stream->position;
    uint32_t piece = smaller(length, MP4_LENGTH_BYTES + stream->track.config_bytes - at);
    for (uint32_t index = 0; index < piece; index++) {
        uint32_t place = at + index;
        if (place < MP4_LENGTH_BYTES) {
            length_bytes(stream->track.config_bytes, place, &out[index], 1);
        } else {
            out[index] = stream->track.config[place - MP4_LENGTH_BYTES];
        }
    }
    return piece;
}

static uint32_t emit_frame(Mp4Stream *stream, uint8_t *out, uint32_t length)
{
    if (stream->frame >= stream->track.frames) {
        return 0;
    }
    uint32_t piece = 0;
    if (stream->emitted < MP4_LENGTH_BYTES) {
        piece = smaller(length, MP4_LENGTH_BYTES - stream->emitted);
        length_bytes(stream->frame_bytes, stream->emitted, out, piece);
    } else {
        piece = smaller(length, MP4_LENGTH_BYTES + stream->frame_bytes - stream->emitted);
        uint32_t from = stream->file_offset + stream->emitted - MP4_LENGTH_BYTES;
        if (!read_at(stream->file, from, out, piece)) {
            return 0;
        }
    }
    stream->emitted += piece;
    if (stream->emitted == MP4_LENGTH_BYTES + stream->frame_bytes) {
        stream->emitted = 0;
        if (!advance(stream)) {
            return 0;
        }
    }
    return piece;
}

static bool read_stream(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    Mp4Stream *stream = context;
    if (offset != stream->position || offset > stream->size || length > stream->size - offset) {
        return false;
    }
    uint8_t *out = buffer;
    while (length > 0) {
        bool in_config = stream->position < MP4_LENGTH_BYTES + stream->track.config_bytes;
        uint32_t piece =
            in_config ? emit_config(stream, out, length) : emit_frame(stream, out, length);
        if (piece == 0) {
            return false;
        }
        out += piece;
        length -= piece;
        stream->position += piece;
    }
    return true;
}

FlacInput mp4_stream_input(Mp4Stream *stream)
{
    return (FlacInput){.read = read_stream, .context = stream, .size = stream->size};
}
