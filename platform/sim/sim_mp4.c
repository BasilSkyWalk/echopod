#include "sim_mp4.h"

#include <stdlib.h>
#include <string.h>

#include "neaacdec.h"

enum {
    AAC_OUTPUT_BITS = 24,
    AAC_OBJECT_LC = 2
};

typedef struct Memory {
    const uint8_t *bytes;
    uint32_t size;
} Memory;

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    SimMp4 *mp4 = context;
    if (offset > mp4->file_size || length > mp4->file_size - offset ||
        fseek(mp4->file, (long)offset, SEEK_SET) != 0) {
        return false;
    }
    return fread(buffer, 1, length, mp4->file) == length;
}

static bool read_memory(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const Memory *memory = context;
    if (offset > memory->size || length > memory->size - offset) {
        return false;
    }
    memcpy(buffer, memory->bytes + offset, length);
    return true;
}

static FlacInput file_input(SimMp4 *mp4)
{
    return (FlacInput){.read = read_file, .context = mp4, .size = mp4->file_size};
}

static bool next_record(SimMp4 *mp4, uint32_t *length)
{
    uint8_t bytes[MP4_LENGTH_BYTES];
    if (mp4->position >= mp4->records.size ||
        !mp4->records.read(mp4->records.context, mp4->position, bytes, sizeof bytes)) {
        return false;
    }
    *length = mp4_record_length(bytes);
    mp4->position += MP4_LENGTH_BYTES;
    if (*length > mp4->frame_capacity) {
        uint8_t *grown = realloc(mp4->frame, *length);
        if (grown == NULL) {
            return false;
        }
        mp4->frame = grown;
        mp4->frame_capacity = *length;
    }
    if (*length > 0 &&
        !mp4->records.read(mp4->records.context, mp4->position, mp4->frame, *length)) {
        return false;
    }
    mp4->position += *length;
    return true;
}

static void close_aac(SimMp4 *mp4)
{
    if (mp4->aac != NULL) {
        NeAACDecClose(mp4->aac);
        mp4->aac = NULL;
    }
}

static bool configure_aac(SimMp4 *mp4, uint32_t length)
{
    close_aac(mp4);
    mp4->aac = NeAACDecOpen();
    if (mp4->aac == NULL) {
        return false;
    }
    NeAACDecConfigurationPtr settings = NeAACDecGetCurrentConfiguration(mp4->aac);
    settings->defObjectType = AAC_OBJECT_LC;
    settings->outputFormat = FAAD_FMT_24BIT;
    settings->dontUpSampleImplicitSBR = 1;
    unsigned long rate = 0;
    unsigned char channels = 0;
    return NeAACDecSetConfiguration(mp4->aac, settings) &&
           (signed char)NeAACDecInit2(mp4->aac, mp4->frame, length, &rate, &channels) >= 0 &&
           channels == mp4->track.channels;
}

static bool configure(SimMp4 *mp4, uint32_t length)
{
    if (mp4->track.codec == MP4_CODEC_AAC) {
        return configure_aac(mp4, length);
    }
    AlacConfig config;
    if (!alac_parse_config(mp4->frame, length, &config)) {
        return false;
    }
    alac_start(mp4->decoder, config);
    return true;
}

static bool start_at(SimMp4 *mp4, uint32_t sample)
{
    uint32_t playable = sim_mp4_playable(mp4);
    uint32_t target = (sample < playable ? sample : playable) + mp4->track.lead_trim;
    uint32_t preroll = mp4->track.codec == MP4_CODEC_AAC ? mp4->track.frame_samples : 0;
    uint32_t from = target > preroll ? target - preroll : 0;
    uint32_t first = 0;
    uint32_t length = 0;
    if (!mp4_stream_open(&mp4->stream, file_input(mp4), &mp4->track, from, &first)) {
        return false;
    }
    mp4->records = mp4_stream_input(&mp4->stream);
    mp4->position = 0;
    if (!next_record(mp4, &length) || !configure(mp4, length)) {
        return false;
    }
    mp4->target = target;
    mp4->next_sample = first;
    mp4->buffered = 0;
    mp4->used = 0;
    return true;
}

bool sim_mp4_open(SimMp4 *mp4, const char *path)
{
    *mp4 = (SimMp4){.file = fopen(path, "rb")};
    if (mp4->file == NULL) {
        return false;
    }
    fseek(mp4->file, 0, SEEK_END);
    long size = ftell(mp4->file);
    mp4->file_size = size > 0 ? (uint32_t)size : 0;
    mp4->decoder = malloc(sizeof *mp4->decoder);
    bool supported =
        mp4_probe(file_input(mp4), &mp4->track) &&
        (mp4->track.codec == MP4_CODEC_ALAC || mp4->track.object_type == MP4_OBJECT_AAC_LC);
    bool opened = mp4->decoder != NULL && supported && start_at(mp4, 0);
    if (!opened) {
        sim_mp4_close(mp4);
    }
    return opened;
}

static bool decode_aac(SimMp4 *mp4, uint32_t length)
{
    NeAACDecFrameInfo info;
    mp4->interleaved = NeAACDecDecode(mp4->aac, &info, mp4->frame, length);
    if (info.error != 0 || (info.samples > 0 && mp4->interleaved == NULL)) {
        return false;
    }
    mp4->buffered = info.channels > 0 ? (uint32_t)info.samples / info.channels : 0;
    mp4->frame_first = mp4->next_sample;
    mp4->next_sample += mp4->track.frame_samples;
    return true;
}

static bool decode_alac(SimMp4 *mp4, uint32_t length)
{
    Memory memory = {mp4->frame, length};
    FlacInput input = {.read = read_memory, .context = &memory, .size = length};
    if (alac_decode_frame(mp4->decoder, input, 0, length) != ALAC_OK) {
        return false;
    }
    mp4->buffered = mp4->decoder->block_samples;
    mp4->frame_first = mp4->next_sample;
    mp4->next_sample += mp4->buffered;
    return true;
}

static bool decode_next(SimMp4 *mp4)
{
    uint32_t length = 0;
    if (!next_record(mp4, &length)) {
        return false;
    }
    mp4->used = 0;
    return mp4->track.codec == MP4_CODEC_AAC ? decode_aac(mp4, length) : decode_alac(mp4, length);
}

static int32_t sample_at(const SimMp4 *mp4, uint32_t channel, uint32_t index)
{
    if (mp4->track.codec == MP4_CODEC_AAC) {
        return mp4->interleaved[index * mp4->track.channels + channel];
    }
    return alac_channel(mp4->decoder, (int)channel)[index];
}

uint64_t sim_mp4_read(SimMp4 *mp4, float *frames, uint64_t count, uint32_t channels)
{
    uint64_t done = 0;
    uint32_t bits =
        mp4->track.codec == MP4_CODEC_AAC ? AAC_OUTPUT_BITS : mp4->track.bits_per_sample;
    float scale = 1.0f / (float)(1u << (bits - 1));
    while (done < count) {
        if (mp4->used == mp4->buffered) {
            if (!decode_next(mp4)) {
                break;
            }
            continue;
        }
        uint32_t sample = mp4->frame_first + mp4->used;
        if (sample >= mp4->track.total_samples) {
            break;
        }
        if (sample >= mp4->target) {
            for (uint32_t channel = 0; channel < channels; channel++) {
                uint32_t source = channel < mp4->track.channels ? channel : 0;
                frames[done * channels + channel] =
                    (float)sample_at(mp4, source, mp4->used) * scale;
            }
            done++;
        }
        mp4->used++;
    }
    return done;
}

bool sim_mp4_seek(SimMp4 *mp4, uint64_t frame)
{
    return frame <= UINT32_MAX && start_at(mp4, (uint32_t)frame);
}

uint32_t sim_mp4_playable(const SimMp4 *mp4)
{
    return mp4->track.total_samples - mp4->track.lead_trim;
}

void sim_mp4_close(SimMp4 *mp4)
{
    close_aac(mp4);
    if (mp4->file != NULL) {
        fclose(mp4->file);
    }
    free(mp4->decoder);
    free(mp4->frame);
    *mp4 = (SimMp4){0};
}
