#include "device/audio_engine.h"

#include <stddef.h>

enum {
    UNITY_GAIN = 65536,
    GAIN_SHIFT = 16,
    OUTPUT_BITS = 24,
    WORD_BITS = 32,
    WORDS_PER_FRAME = 2,
    SAMPLE_MAX = 8388607,
    SAMPLE_MIN = -8388608,
    MILLISECONDS_PER_SECOND = 1000,
    STEP_GAIN_Q16 = 58409
};

static const uint32_t SUPPORTED_RATES[] = {44100, 48000, 88200, 96000, 192000};

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

static void release_frame(AudioEngine *engine)
{
    if (engine->holding_frame) {
        engine->config.source.release(engine->config.source.context);
        engine->holding_frame = false;
    }
    engine->pending_frames = 0;
}

static void close_file(AudioEngine *engine)
{
    if (engine->decoding) {
        release_frame(engine);
        engine->config.source.close(engine->config.source.context);
        engine->decoding = false;
    }
}

static bool open_track(AudioEngine *engine, const char *path, uint32_t start_ms)
{
    AudioTrackInfo track;
    if (!engine->config.source.open(engine->config.source.context, path, start_ms, &track)) {
        engine->refusal = (AudioRefusal){.problem = AUDIO_REFUSAL_UNREADABLE};
        return false;
    }
    FlacInfo info = track.stream;
    engine->decoding = true;
    if (!rate_supported(info.sample_rate)) {
        close_file(engine);
        engine->refusal =
            (AudioRefusal){.problem = AUDIO_REFUSAL_RATE, .sample_rate = info.sample_rate};
        return false;
    }
    engine->skip_frames =
        (uint32_t)((uint64_t)start_ms * info.sample_rate / MILLISECONDS_PER_SECOND);
    engine->decode_rate = info.sample_rate;
    engine->decode_track = track;
    engine->holding_frame = false;
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
    segment->track = engine->decode_track;
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
    engine->stats = (AudioEngineStats){0};
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
    return value >= 0 ? value >> shift : -((-value - 1) >> shift) - 1;
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

static uint32_t ring_frames(const AudioEngine *engine)
{
    return engine->config.ring_words / WORDS_PER_FRAME;
}

static uint32_t free_space(const AudioEngine *engine)
{
    return ring_frames(engine) - (engine->produced - engine->consumed);
}

static uint32_t output_word(int32_t sample)
{
    return (uint32_t)sample << (WORD_BITS - OUTPUT_BITS);
}

static void write_frame(AudioEngine *engine, int32_t left, int32_t right)
{
    uint32_t *slot =
        &engine->config.ring[WORDS_PER_FRAME * (engine->produced % ring_frames(engine))];
    slot[0] = output_word(left);
    slot[1] = output_word(right);
    engine->produced++;
}

static void copy_pending(AudioEngine *engine)
{
    const int32_t *left = engine->frame.left;
    const int32_t *right = engine->frame.right;
    int bits = engine->frame.bits;
    while (engine->pending_frames > 0 && free_space(engine) > 0) {
        uint32_t index = engine->pending_offset;
        write_frame(engine, to_output(left[index], bits, engine->gain),
                    to_output(right[index], bits, engine->gain));
        engine->pending_offset++;
        engine->pending_frames--;
    }
    if (engine->pending_frames == 0) {
        release_frame(engine);
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
    AudioFrame frame;
    AudioFrameResult result = engine->config.source.next(engine->config.source.context, &frame);
    if (result == AUDIO_FRAME_WAIT) {
        return;
    }
    if (result == AUDIO_FRAME_END) {
        finish_decoding(engine);
        return;
    }
    if (result != AUDIO_FRAME_READY) {
        engine->failed = true;
        close_file(engine);
        engine->ended = true;
        engine->end_at = engine->produced;
        return;
    }
    uint32_t skip = 0;
    if (engine->skip_frames > frame.first_sample) {
        skip = engine->skip_frames - frame.first_sample;
        skip = skip < frame.samples ? skip : frame.samples;
    }
    engine->frame = frame;
    engine->holding_frame = true;
    engine->skip_frames = 0;
    engine->pending_offset = skip;
    engine->pending_frames = frame.samples - skip;
    engine->stats.frames++;
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

void audio_engine_observe(AudioEngine *engine, uint32_t consumed)
{
    engine->consumed = consumed;
    if ((int32_t)(engine->produced - engine->consumed) < 0) {
        if (engine->decoding) {
            engine->stats.underruns++;
        }
        engine->produced = engine->consumed;
    }
    drop_finished_segments(engine);
    if (engine->ended && (int32_t)(engine->consumed - engine->end_at) >= 0) {
        engine->state = AUDIO_ENGINE_STOPPED;
    }
}

void audio_engine_pump(AudioEngine *engine, uint32_t consumed)
{
    audio_engine_observe(engine, consumed);
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
            write_frame(engine, 0, 0);
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

AudioEngineStats audio_engine_stats(const AudioEngine *engine)
{
    return engine->stats;
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
                               .failed = engine->failed,
                               .track = segment->track};
}

const char *audio_engine_current_path(const AudioEngine *engine)
{
    return engine->segment_count == 0 ? NULL : engine->segments[0].path;
}

AudioRefusal audio_engine_refusal(const AudioEngine *engine)
{
    return engine->refusal;
}
