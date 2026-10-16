#include "device/flac_source.h"

enum {
    MILLISECONDS_PER_SECOND = 1000
};

void flac_source_init(FlacSource *source, FlacFileOpener files, FlacDecoder *decoder)
{
    *source = (FlacSource){.files = files, .decoder = decoder, .open = false};
}

void flac_source_close(FlacSource *source)
{
    if (source->open) {
        source->files.close(source->files.context);
        source->open = false;
    }
}

bool flac_source_open_at(FlacSource *source, const char *path, uint32_t start_ms, FlacInfo *info)
{
    flac_source_close(source);
    FlacInput input;
    if (!source->files.open(source->files.context, path, &input)) {
        return false;
    }
    source->open = true;
    FlacDecoder *decoder = source->decoder;
    if (flac_open(decoder, input) != FLAC_OK) {
        flac_source_close(source);
        return false;
    }
    uint32_t target =
        (uint32_t)((uint64_t)start_ms * decoder->info.sample_rate / MILLISECONDS_PER_SECOND);
    if (target > 0 && flac_seek(decoder, target) != FLAC_OK) {
        flac_source_close(source);
        return false;
    }
    *info = decoder->info;
    return true;
}

static bool open_source(void *context, const char *path, uint32_t start_ms, FlacInfo *info)
{
    return flac_source_open_at(context, path, start_ms, info);
}

static AudioFrameResult next_frame(void *context, AudioFrame *frame)
{
    FlacSource *source = context;
    FlacDecoder *decoder = source->decoder;
    FlacResult result = flac_decode_frame(decoder);
    if (result == FLAC_END) {
        return AUDIO_FRAME_END;
    }
    if (result != FLAC_OK) {
        return AUDIO_FRAME_ERROR;
    }
    *frame = (AudioFrame){
        .left = flac_channel(decoder, 0),
        .right = flac_channel(decoder, decoder->info.channels > 1 ? 1 : 0),
        .first_sample = decoder->frame_first_sample,
        .samples = decoder->block_samples,
        .bits = decoder->info.bits_per_sample,
    };
    return AUDIO_FRAME_READY;
}

static void release_frame(void *context)
{
    (void)context;
}

static void close_source(void *context)
{
    flac_source_close(context);
}

AudioSource flac_source_interface(FlacSource *source)
{
    return (AudioSource){open_source, next_frame, release_frame, close_source, source};
}
