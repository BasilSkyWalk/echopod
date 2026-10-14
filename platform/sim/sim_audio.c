#include "sim_audio.h"

#include <SDL.h>
#include <math.h>

#include "hal/audio.h"
#include "sim_decoder.h"
#include "sim_storage.h"

enum {
    DEVICE_RATE = 48000,
    STEREO = 2,
    DEVICE_BUFFER_FRAMES = 1024,
    CHUNK_FRAMES = 1024,
    BYTES_PER_FRAME = STEREO * (int)sizeof(float),
    SLOT_COUNT = 2,
    HOST_PATH_CAPACITY = 1024,
    MILLISECONDS_PER_SECOND = 1000
};

static const float DECIBELS_PER_FACTOR_OF_TEN = 20.0f;

typedef struct SimTrack {
    SimDecoder decoder;
    uint32_t token;
    bool open;
} SimTrack;

typedef struct SimAudio {
    bool virtual_output;
    SDL_AudioDeviceID device;
    SDL_AudioStream *stream;
    uint32_t stream_rate;
    SimTrack slots[SLOT_COUNT];
    int current;
    HalAudioState state;
    bool failed;
    float gain;
    HalAudioGain analog_gain;
    HalAudioOutput output;
} SimAudio;

static SimAudio audio;
static float chunk[CHUNK_FRAMES * STEREO];

static void lock(void)
{
    if (audio.device != 0) {
        SDL_LockAudioDevice(audio.device);
    }
}

static void unlock(void)
{
    if (audio.device != 0) {
        SDL_UnlockAudioDevice(audio.device);
    }
}

static SimTrack *current_track(void)
{
    return &audio.slots[audio.current];
}

static SimTrack *next_track(void)
{
    return &audio.slots[1 - audio.current];
}

static void close_track(SimTrack *track)
{
    if (track->open) {
        sim_decoder_close(&track->decoder);
        track->open = false;
    }
}

static bool open_track(SimTrack *track, const char *path, uint32_t token)
{
    char host_path[HOST_PATH_CAPACITY];
    close_track(track);
    track->token = token;
    track->open = sim_storage_host_path(path, host_path, sizeof host_path) &&
                  sim_decoder_open(&track->decoder, host_path);
    return track->open;
}

static void use_stream_rate(uint32_t rate)
{
    if (audio.virtual_output || (audio.stream != NULL && audio.stream_rate == rate)) {
        return;
    }
    if (audio.stream != NULL) {
        SDL_FreeAudioStream(audio.stream);
    }
    audio.stream = SDL_NewAudioStream(AUDIO_F32, STEREO, (int)rate, AUDIO_F32, STEREO, DEVICE_RATE);
    audio.stream_rate = rate;
}

static void clear_stream(void)
{
    if (audio.stream != NULL) {
        SDL_AudioStreamClear(audio.stream);
    }
}

static bool switch_to_next(void)
{
    close_track(current_track());
    if (!next_track()->open) {
        audio.state = HAL_AUDIO_STOPPED;
        return false;
    }
    audio.current = 1 - audio.current;
    use_stream_rate(current_track()->decoder.sample_rate);
    return true;
}

static bool decode_chunk(void)
{
    SimTrack *track = current_track();
    if (!track->open) {
        return false;
    }
    uint64_t frames = sim_decoder_read_stereo(&track->decoder, chunk, CHUNK_FRAMES);
    if (frames == 0) {
        return switch_to_next();
    }
    for (uint64_t sample = 0; sample < frames * STEREO; sample++) {
        chunk[sample] *= audio.gain;
    }
    SDL_AudioStreamPut(audio.stream, chunk, (int)(frames * BYTES_PER_FRAME));
    return true;
}

static void fill_audio(void *userdata, Uint8 *output, int length)
{
    (void)userdata;
    int filled = 0;
    while (audio.state == HAL_AUDIO_PLAYING && filled < length) {
        int available = SDL_AudioStreamAvailable(audio.stream);
        if (available <= 0) {
            if (!decode_chunk()) {
                break;
            }
            continue;
        }
        int wanted = SDL_min(available, length - filled);
        int got = SDL_AudioStreamGet(audio.stream, output + filled, wanted);
        if (got <= 0) {
            break;
        }
        filled += got;
    }
    SDL_memset(output + filled, 0, (size_t)(length - filled));
}

static uint64_t frames_for(uint32_t milliseconds, uint32_t rate)
{
    return (uint64_t)milliseconds * rate / MILLISECONDS_PER_SECOND;
}

static bool open_device(void)
{
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    SDL_zero(desired);
    desired.freq = DEVICE_RATE;
    desired.format = AUDIO_F32;
    desired.channels = STEREO;
    desired.samples = DEVICE_BUFFER_FRAMES;
    desired.callback = fill_audio;
    audio.device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (audio.device == 0) {
        return false;
    }
    use_stream_rate(DEVICE_RATE);
    SDL_PauseAudioDevice(audio.device, 0);
    return true;
}

bool sim_audio_start(bool virtual_output)
{
    SDL_memset(&audio, 0, sizeof audio);
    audio.virtual_output = virtual_output;
    audio.gain = 1.0f;
    audio.state = HAL_AUDIO_STOPPED;
    if (virtual_output) {
        return true;
    }
    if (open_device()) {
        return true;
    }
    SDL_Log("no audio device (%s): playback runs silently", SDL_GetError());
    audio.virtual_output = true;
    return false;
}

void sim_audio_tick(uint32_t elapsed_ms)
{
    SimTrack *track = current_track();
    if (!audio.virtual_output || audio.state != HAL_AUDIO_PLAYING || !track->open) {
        return;
    }
    track->decoder.frame += frames_for(elapsed_ms, track->decoder.sample_rate);
    if (track->decoder.frame >= track->decoder.total_frames) {
        switch_to_next();
    }
}

void sim_audio_stop(void)
{
    if (audio.device != 0) {
        SDL_CloseAudioDevice(audio.device);
        audio.device = 0;
    }
    if (audio.stream != NULL) {
        SDL_FreeAudioStream(audio.stream);
        audio.stream = NULL;
    }
    close_track(&audio.slots[0]);
    close_track(&audio.slots[1]);
}

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token)
{
    lock();
    close_track(next_track());
    bool opened = open_track(current_track(), path, token);
    if (opened) {
        SimDecoder *decoder = &current_track()->decoder;
        sim_decoder_seek(decoder, frames_for(start_ms, decoder->sample_rate));
        use_stream_rate(decoder->sample_rate);
        clear_stream();
    }
    audio.state = opened ? HAL_AUDIO_PLAYING : HAL_AUDIO_STOPPED;
    audio.failed = !opened;
    unlock();
    return opened;
}

void hal_audio_queue_next(const char *path, uint32_t token)
{
    lock();
    close_track(next_track());
    if (path != NULL) {
        open_track(next_track(), path, token);
    }
    unlock();
}

void hal_audio_pause(void)
{
    lock();
    if (audio.state == HAL_AUDIO_PLAYING) {
        audio.state = HAL_AUDIO_PAUSED;
    }
    unlock();
}

void hal_audio_resume(void)
{
    lock();
    if (audio.state == HAL_AUDIO_PAUSED) {
        audio.state = HAL_AUDIO_PLAYING;
    }
    unlock();
}

void hal_audio_seek(uint32_t position_ms)
{
    lock();
    SimTrack *track = current_track();
    if (track->open) {
        sim_decoder_seek(&track->decoder, frames_for(position_ms, track->decoder.sample_rate));
        clear_stream();
    }
    unlock();
}

void hal_audio_set_volume(uint8_t level)
{
    float decibels = (float)level - (float)HAL_AUDIO_VOLUME_MAX;
    lock();
    audio.gain = level == 0 ? 0.0f : powf(10.0f, decibels / DECIBELS_PER_FACTOR_OF_TEN);
    unlock();
}

void hal_audio_set_gain(HalAudioGain gain)
{
    lock();
    audio.analog_gain = gain;
    unlock();
}

void hal_audio_set_output(HalAudioOutput output)
{
    lock();
    audio.output = output;
    unlock();
}

static bool dma_tone_running;

HalToneDetail hal_audio_tone_detail(void)
{
    return (HalToneDetail){.stage = 0, .pending = 0};
}

HalToneResult hal_audio_dma_tone_start(void)
{
    dma_tone_running = true;
    return HAL_TONE_PLAYED;
}

void hal_audio_dma_tone_stop(void)
{
    dma_tone_running = false;
}

HalDmaToneStatus hal_audio_dma_tone_status(void)
{
    return (HalDmaToneStatus){.running = dma_tone_running, .position = 0};
}

HalToneResult hal_audio_test_tone(void)
{
    return HAL_TONE_PLAYED;
}

HalAudioStatus hal_audio_status(void)
{
    lock();
    SimTrack *track = current_track();
    uint32_t rate = track->decoder.sample_rate;
    uint64_t position =
        track->open && rate > 0 ? track->decoder.frame * MILLISECONDS_PER_SECOND / rate : 0;
    HalAudioStatus status = {
        .state = audio.state,
        .token = track->token,
        .position_ms = (uint32_t)position,
        .failed = audio.failed,
    };
    unlock();
    return status;
}
