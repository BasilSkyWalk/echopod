#include "sim_decoder.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "core/format.h"

enum {
    MAX_FRAMES_PER_READ = 1024,
    MAX_CHANNELS = 8,
    STEREO = 2
};

static float native_samples[MAX_FRAMES_PER_READ * MAX_CHANNELS];

static bool has_extension(const char *path, const char *extension)
{
    size_t path_length = strlen(path);
    size_t extension_length = strlen(extension);
    return path_length > extension_length &&
           strcasecmp(path + path_length - extension_length, extension) == 0;
}

static SimCodec codec_for(const char *path)
{
    if (has_extension(path, ".flac")) {
        return SIM_CODEC_FLAC;
    }
    if (has_extension(path, ".mp3")) {
        return SIM_CODEC_MP3;
    }
    if (has_extension(path, ".wav")) {
        return SIM_CODEC_WAV;
    }
    return SIM_CODEC_NONE;
}

static uint64_t file_size(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 ? (uint64_t)info.st_size : 0;
}

static bool open_flac(SimDecoder *decoder, const char *path)
{
    decoder->flac = drflac_open_file(path, NULL);
    if (decoder->flac == NULL) {
        return false;
    }
    decoder->sample_rate = decoder->flac->sampleRate;
    decoder->channels = decoder->flac->channels;
    decoder->bits_per_sample = decoder->flac->bitsPerSample;
    decoder->audio_bytes = file_size(path) - decoder->flac->firstFLACFramePosInBytes;
    decoder->total_frames = decoder->flac->totalPCMFrameCount;
    return true;
}

static bool open_mp3(SimDecoder *decoder, const char *path)
{
    if (!drmp3_init_file(&decoder->mp3, path, NULL)) {
        return false;
    }
    decoder->sample_rate = decoder->mp3.sampleRate;
    decoder->channels = decoder->mp3.channels;
    decoder->bits_per_sample = 0;
    decoder->audio_bytes = file_size(path);
    decoder->total_frames = drmp3_get_pcm_frame_count(&decoder->mp3);
    return true;
}

static bool open_wav(SimDecoder *decoder, const char *path)
{
    if (!drwav_init_file(&decoder->wav, path, NULL)) {
        return false;
    }
    decoder->sample_rate = decoder->wav.sampleRate;
    decoder->channels = decoder->wav.channels;
    decoder->bits_per_sample = decoder->wav.bitsPerSample;
    decoder->audio_bytes = decoder->wav.dataChunkDataSize;
    decoder->total_frames = decoder->wav.totalPCMFrameCount;
    return true;
}

static bool open_codec(SimDecoder *decoder, const char *path)
{
    switch (decoder->codec) {
    case SIM_CODEC_FLAC:
        return open_flac(decoder, path);
    case SIM_CODEC_MP3:
        return open_mp3(decoder, path);
    case SIM_CODEC_WAV:
        return open_wav(decoder, path);
    case SIM_CODEC_NONE:
        return false;
    }
    return false;
}

bool sim_decoder_open(SimDecoder *decoder, const char *host_path)
{
    decoder->codec = codec_for(host_path);
    decoder->frame = 0;
    bool opened = open_codec(decoder, host_path);
    bool usable = opened && decoder->sample_rate > 0 && decoder->channels > 0 &&
                  decoder->channels <= MAX_CHANNELS;
    if (opened && !usable) {
        sim_decoder_close(decoder);
    }
    if (!usable) {
        decoder->codec = SIM_CODEC_NONE;
    }
    return usable;
}

static uint64_t read_native(SimDecoder *decoder, uint64_t count)
{
    switch (decoder->codec) {
    case SIM_CODEC_FLAC:
        return drflac_read_pcm_frames_f32(decoder->flac, count, native_samples);
    case SIM_CODEC_MP3:
        return drmp3_read_pcm_frames_f32(&decoder->mp3, count, native_samples);
    case SIM_CODEC_WAV:
        return drwav_read_pcm_frames_f32(&decoder->wav, count, native_samples);
    case SIM_CODEC_NONE:
        return 0;
    }
    return 0;
}

uint64_t sim_decoder_read_stereo(SimDecoder *decoder, float *frames, uint64_t count)
{
    uint64_t wanted = count < MAX_FRAMES_PER_READ ? count : MAX_FRAMES_PER_READ;
    uint64_t read = read_native(decoder, wanted);
    uint32_t channels = decoder->channels;
    for (uint64_t frame = 0; frame < read; frame++) {
        const float *source = &native_samples[frame * channels];
        frames[frame * STEREO] = source[0];
        frames[frame * STEREO + 1] = channels > 1 ? source[1] : source[0];
    }
    decoder->frame += read;
    return read;
}

bool sim_decoder_seek(SimDecoder *decoder, uint64_t frame)
{
    bool sought = false;
    switch (decoder->codec) {
    case SIM_CODEC_FLAC:
        sought = drflac_seek_to_pcm_frame(decoder->flac, frame);
        break;
    case SIM_CODEC_MP3:
        sought = drmp3_seek_to_pcm_frame(&decoder->mp3, frame);
        break;
    case SIM_CODEC_WAV:
        sought = drwav_seek_to_pcm_frame(&decoder->wav, frame);
        break;
    case SIM_CODEC_NONE:
        break;
    }
    if (sought) {
        decoder->frame = frame;
    }
    return sought;
}

void sim_decoder_close(SimDecoder *decoder)
{
    switch (decoder->codec) {
    case SIM_CODEC_FLAC:
        drflac_close(decoder->flac);
        decoder->flac = NULL;
        break;
    case SIM_CODEC_MP3:
        drmp3_uninit(&decoder->mp3);
        break;
    case SIM_CODEC_WAV:
        drwav_uninit(&decoder->wav);
        break;
    case SIM_CODEC_NONE:
        break;
    }
    decoder->codec = SIM_CODEC_NONE;
}

static HalAudioCodec hal_codec(SimCodec codec)
{
    switch (codec) {
    case SIM_CODEC_FLAC:
        return HAL_AUDIO_CODEC_FLAC;
    case SIM_CODEC_MP3:
        return HAL_AUDIO_CODEC_MP3;
    case SIM_CODEC_WAV:
        return HAL_AUDIO_CODEC_WAV;
    case SIM_CODEC_NONE:
        return HAL_AUDIO_CODEC_UNKNOWN;
    }
    return HAL_AUDIO_CODEC_UNKNOWN;
}

HalAudioFormat sim_decoder_format(const SimDecoder *decoder)
{
    if (decoder->codec == SIM_CODEC_NONE) {
        return (HalAudioFormat){.codec = HAL_AUDIO_CODEC_UNKNOWN};
    }
    return (HalAudioFormat){
        .codec = hal_codec(decoder->codec),
        .sample_rate = decoder->sample_rate,
        .bits_per_sample = (uint8_t)decoder->bits_per_sample,
        .channels = (uint8_t)decoder->channels,
        .bitrate_kbps = average_kbps((uint32_t)decoder->audio_bytes,
                                     (uint32_t)decoder->total_frames, decoder->sample_rate),
    };
}
