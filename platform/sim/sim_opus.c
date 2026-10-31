#include "sim_opus.h"

#include <stdlib.h>

#include "opus.h"

enum {
    SAMPLE_SCALE = 32768
};

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    SimOpus *opus = context;
    if (offset > opus->file_size || length > opus->file_size - offset ||
        fseek(opus->file, (long)offset, SEEK_SET) != 0) {
        return false;
    }
    return fread(buffer, 1, length, opus->file) == length;
}

static FlacInput file_input(SimOpus *opus)
{
    return (FlacInput){.read = read_file, .context = opus, .size = opus->file_size};
}

static bool start_at(SimOpus *opus, uint32_t sample)
{
    uint32_t playable = sim_opus_playable(opus);
    uint32_t target = (sample < playable ? sample : playable) + opus->info.pre_skip;
    if (opus_decoder_init(opus->decoder, OGG_OPUS_RATE, opus->info.channels) != OPUS_OK) {
        return false;
    }
    uint32_t start = ogg_opus_seek(file_input(opus), &opus->info, target);
    ogg_reader_start(&opus->reader, file_input(opus), start, opus->info.serial, opus->packet,
                     sizeof opus->packet);
    opus->target = target;
    opus->buffered = 0;
    opus->used = 0;
    return true;
}

bool sim_opus_open(SimOpus *opus, const char *path)
{
    *opus = (SimOpus){.file = fopen(path, "rb")};
    if (opus->file == NULL) {
        return false;
    }
    fseek(opus->file, 0, SEEK_END);
    long size = ftell(opus->file);
    opus->file_size = size > 0 ? (uint32_t)size : 0;
    bool probed = ogg_opus_probe(file_input(opus), &opus->info);
    if (probed) {
        opus->decoder = malloc((size_t)opus_decoder_get_size(opus->info.channels));
    }
    bool opened = probed && opus->decoder != NULL && start_at(opus, 0);
    if (!opened) {
        sim_opus_close(opus);
    }
    return opened;
}

static bool decode_next(SimOpus *opus)
{
    OggPacket packet;
    if (ogg_reader_next(&opus->reader, &packet) != OGG_PACKET) {
        return false;
    }
    int frames = opus_decode(opus->decoder, packet.data, (opus_int32)packet.length, opus->output,
                             SIM_OPUS_MAX_FRAME, 0);
    if (frames < 0) {
        return false;
    }
    opus->frame_first = packet.first_sample;
    opus->buffered = (uint32_t)frames;
    opus->used = 0;
    return true;
}

uint64_t sim_opus_read(SimOpus *opus, float *frames, uint64_t count, uint32_t channels)
{
    uint64_t done = 0;
    while (done < count) {
        if (opus->used == opus->buffered) {
            if (!decode_next(opus)) {
                break;
            }
            continue;
        }
        uint32_t sample = opus->frame_first + opus->used;
        if (sample >= opus->info.last_granule) {
            break;
        }
        if (sample >= opus->target) {
            for (uint32_t channel = 0; channel < channels; channel++) {
                uint32_t source = channel < opus->info.channels ? channel : 0;
                int16_t value = opus->output[opus->used * opus->info.channels + source];
                frames[done * channels + channel] = (float)value / (float)SAMPLE_SCALE;
            }
            done++;
        }
        opus->used++;
    }
    return done;
}

bool sim_opus_seek(SimOpus *opus, uint64_t frame)
{
    return frame <= UINT32_MAX && start_at(opus, (uint32_t)frame);
}

uint32_t sim_opus_playable(const SimOpus *opus)
{
    return opus->info.last_granule - opus->info.pre_skip;
}

void sim_opus_close(SimOpus *opus)
{
    if (opus->file != NULL) {
        fclose(opus->file);
    }
    free(opus->decoder);
    *opus = (SimOpus){0};
}
