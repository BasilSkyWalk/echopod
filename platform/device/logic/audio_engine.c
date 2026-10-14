#include "device/audio_engine.h"

#include <stddef.h>

enum {
    UNITY_GAIN = 65536,
    GAIN_SHIFT = 16,
    OUTPUT_BITS = 16,
    SAMPLE_MAX = 32767,
    SAMPLE_MIN = -32768,
    MILLISECONDS_PER_SECOND = 1000,
    STEP_GAIN_Q16 = 58409
};

static const uint32_t SUPPORTED_RATES[] = {44100, 48000, 88200, 96000};

static void copy_path(char *destination, const char *source)
{
    size_t index = 0;
    while (source[index] != '\0' && index + 1 < AUDIO_ENGINE_PATH_CAPACITY) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static bool rate_supported(uint32_t rate)
{
    for (size_t index = 0; index < sizeof SUPPORTED_RATES / sizeof SUPPORTED_RATES[0]; index++) {
        if (SUPPORTED_RATES[index] == rate) {
            return true;
        }
    }
    return false;
}

void audio_engine_init(AudioEngine *engine, AudioEngineConfig config)
{
    *engine = (AudioEngine){.config = config, .state = AUDIO_ENGINE_STOPPED, .gain = UNITY_GAIN};
}

void audio_engine_set_volume(AudioEngine *engine, uint8_t level)
{
    if (level == 0) {
        engine->gain = 0;
        return;
    }
    uint32_t gain = UNITY_GAIN;
    for (int step = level; step < AUDIO_ENGINE_VOLUME_MAX; step++) {
        gain = (uint32_t)(((uint64_t)gain * STEP_GAIN_Q16 + UNITY_GAIN / 2) >> GAIN_SHIFT);
    }
    engine->gain = gain;
}

static void close_file(AudioEngine *engine)
{
    if (engine->decoding) {
        engine->config.close(engine->config.context);
        engine->decoding = false;
    }
}

static bool open_track(AudioEngine *engine, const char *path, uint32_t start_ms)
{
    FlacInput input;
    FlacDecoder *decoder = engine->config.decoder;
    if (!engine->config.open(engine->config.context, path, &input)) {
        return false;
    }
    engine->decoding = true;
    if (flac_open(decoder, input) != FLAC_OK || !rate_supported(decoder->info.sample_rate)) {
        close_file(engine);
        return false;
    }
    uint32_t target =
        (uint32_t)((uint64_t)start_ms * decoder->info.sample_rate / MILLISECONDS_PER_SECOND);
    engine->skip_frames = target;
    if (target > 0 && flac_seek(decoder, target) != FLAC_OK) {
        close_file(engine);
        return false;
    }
    engine->decode_rate = decoder->info.sample_rate;
    engine->pending_frames = 0;
    engine->pending_offset = 0;
    return true;
}

static void push_segment(AudioEngine *engine, const char *path, uint32_t token, uint32_t start,
                         uint32_t base_ms)
{
    if (engine->segment_count == AUDIO_ENGINE_SEGMENTS) {
        for (int index = 1; index < AUDIO_ENGINE_SEGMENTS; index++) {
            engine->segments[index - 1] = engine->segments[index];
        }
        engine->segment_count--;
    }
    AudioSegment *segment = &engine->segments[engine->segment_count++];
    segment->token = token;
    segment->start = start;
    segment->base_ms = base_ms;
    segment->rate = engine->decode_rate;
    copy_path(segment->path, path);
}

static void reset_positions(AudioEngine *engine)
{
    engine->produced = 0;
    engine->consumed = 0;
    engine->segment_count = 0;
    engine->ended = false;
    engine->end_at = 0;
    engine->rate_pending = false;
}

static bool start_track(AudioEngine *engine, const char *path, uint32_t start_ms, uint32_t token)
{
    close_file(engine);
    reset_positions(engine);
    engine->failed = false;
    if (!open_track(engine, path, start_ms)) {
        engine->state = AUDIO_ENGINE_STOPPED;
        engine->failed = true;
        engine->segments[0] = (AudioSegment){.token = token, .rate = 0};
        engine->segment_count = 1;
        return false;
    }
    engine->output_rate = engine->decode_rate;
    push_segment(engine, path, token, 0, start_ms);
    engine->state = AUDIO_ENGINE_PLAYING;
    return true;
}

bool audio_engine_play(AudioEngine *engine, const char *path, uint32_t start_ms, uint32_t token)
{
    return start_track(engine, path, start_ms, token);
}

void audio_engine_queue_next(AudioEngine *engine, const char *path, uint32_t token)
{
    engine->has_next = path != NULL;
    engine->next_token = token;
    if (path != NULL) {
        copy_path(engine->next_path, path);
    }
}

bool audio_engine_seek(AudioEngine *engine, uint32_t position_ms)
{
    if (engine->segment_count == 0) {
        return false;
    }
    AudioSegment playing = engine->segments[0];
    AudioEngineState state = engine->state;
    bool started = start_track(engine, playing.path, position_ms, playing.token);
    if (started && state == AUDIO_ENGINE_PAUSED) {
        engine->state = AUDIO_ENGINE_PAUSED;
    }
    return started;
}

void audio_engine_pause(AudioEngine *engine)
{
    if (engine->state == AUDIO_ENGINE_PLAYING) {
        engine->state = AUDIO_ENGINE_PAUSED;
    }
}

void audio_engine_resume(AudioEngine *engine)
{
    if (engine->state == AUDIO_ENGINE_PAUSED) {
        engine->state = AUDIO_ENGINE_PLAYING;
    }
}

void audio_engine_stop(AudioEngine *engine)
{
    close_file(engine);
    engine->state = AUDIO_ENGINE_STOPPED;
    engine->ended = true;
    engine->end_at = engine->produced;
}

static int64_t floor_shift(int64_t value, int shift)
{
    int64_t divisor = INT64_C(1) << shift;
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

static int32_t to_output(int32_t sample, int bits, uint32_t gain)
{
    int64_t value = (int64_t)sample * gain;
    int shift = GAIN_SHIFT;
    if (bits > OUTPUT_BITS) {
        shift += bits - OUTPUT_BITS;
    } else {
        value *= INT64_C(1) << (OUTPUT_BITS - bits);
    }
    int64_t rounded = floor_shift(value + (INT64_C(1) << (shift - 1)), shift);
    return rounded > SAMPLE_MAX ? SAMPLE_MAX : rounded < SAMPLE_MIN ? SAMPLE_MIN : (int32_t)rounded;
}

static uint32_t free_space(const AudioEngine *engine)
{
    return engine->config.ring_words - (engine->produced - engine->consumed);
}

static void write_word(AudioEngine *engine, uint32_t word)
{
    engine->config.ring[engine->produced % engine->config.ring_words] = word;
    engine->produced++;
}

static void copy_pending(AudioEngine *engine)
{
    const FlacDecoder *decoder = engine->config.decoder;
    const int32_t *left = flac_channel(decoder, 0);
    const int32_t *right = flac_channel(decoder, decoder->info.channels > 1 ? 1 : 0);
    int bits = decoder->info.bits_per_sample;
    while (engine->pending_frames > 0 && free_space(engine) > 0) {
        uint32_t index = engine->pending_offset;
        int32_t l = to_output(left[index], bits, engine->gain);
        int32_t r = to_output(right[index], bits, engine->gain);
        write_word(engine, (uint32_t)(uint16_t)l | (uint32_t)(uint16_t)r << 16);
        engine->pending_offset++;
        engine->pending_frames--;
    }
}

static void finish_decoding(AudioEngine *engine)
{
    close_file(engine);
    if (!engine->has_next) {
        engine->ended = true;
        engine->end_at = engine->produced;
        return;
    }
    engine->has_next = false;
    if (!open_track(engine, engine->next_path, 0)) {
        engine->ended = true;
        engine->end_at = engine->produced;
        engine->failed = true;
        return;
    }
    if (engine->decode_rate != engine->output_rate) {
        engine->rate_pending = true;
        return;
    }
    push_segment(engine, engine->next_path, engine->next_token, engine->produced, 0);
}

static void decode_one(AudioEngine *engine)
{
    FlacDecoder *decoder = engine->config.decoder;
    FlacResult result = flac_decode_frame(decoder);
    if (result == FLAC_END) {
        finish_decoding(engine);
        return;
    }
    if (result != FLAC_OK) {
        engine->failed = true;
        close_file(engine);
        engine->ended = true;
        engine->end_at = engine->produced;
        return;
    }
    uint32_t skip = 0;
    if (engine->skip_frames > decoder->frame_first_sample) {
        skip = engine->skip_frames - decoder->frame_first_sample;
        skip = skip < decoder->block_samples ? skip : decoder->block_samples;
    }
    engine->skip_frames = 0;
    engine->pending_offset = skip;
    engine->pending_frames = decoder->block_samples - skip;
}

static void drop_finished_segments(AudioEngine *engine)
{
    while (engine->segment_count > 1 &&
           (int32_t)(engine->consumed - engine->segments[1].start) >= 0) {
        for (int index = 1; index < engine->segment_count; index++) {
            engine->segments[index - 1] = engine->segments[index];
        }
        engine->segment_count--;
    }
}

void audio_engine_pump(AudioEngine *engine, uint32_t consumed)
{
    engine->consumed = consumed;
    if ((int32_t)(engine->produced - engine->consumed) < 0) {
        engine->produced = engine->consumed;
    }
    drop_finished_segments(engine);
    if (engine->ended && (int32_t)(engine->consumed - engine->end_at) >= 0) {
        engine->state = AUDIO_ENGINE_STOPPED;
    }
    if (engine->rate_pending) {
        return;
    }
    copy_pending(engine);
    if (engine->pending_frames == 0 && engine->decoding && free_space(engine) > 0) {
        decode_one(engine);
        if (!engine->rate_pending) {
            copy_pending(engine);
        }
    }
    if (!engine->decoding) {
        while (free_space(engine) > 0) {
            write_word(engine, 0);
        }
    }
}

bool audio_engine_wants_rate(const AudioEngine *engine, uint32_t *rate)
{
    if (!engine->rate_pending || (int32_t)(engine->consumed - engine->produced) < 0) {
        return false;
    }
    *rate = engine->decode_rate;
    return true;
}

void audio_engine_restart(AudioEngine *engine)
{
    if (!engine->rate_pending) {
        return;
    }
    reset_positions(engine);
    engine->output_rate = engine->decode_rate;
    push_segment(engine, engine->next_path, engine->next_token, 0, 0);
}

uint32_t audio_engine_rate(const AudioEngine *engine)
{
    return engine->output_rate;
}

uint32_t audio_engine_buffered(const AudioEngine *engine, uint32_t consumed)
{
    return engine->produced - consumed;
}

AudioEngineStatus audio_engine_status(const AudioEngine *engine)
{
    if (engine->segment_count == 0) {
        return (AudioEngineStatus){.state = engine->state, .failed = engine->failed};
    }
    const AudioSegment *segment = &engine->segments[0];
    uint32_t played = engine->consumed - segment->start;
    uint32_t position =
        segment->rate == 0 ? segment->base_ms
                           : segment->base_ms + (uint32_t)((uint64_t)played *
                                                           MILLISECONDS_PER_SECOND / segment->rate);
    return (AudioEngineStatus){.state = engine->state,
                               .token = segment->token,
                               .position_ms = position,
                               .failed = engine->failed};
}
