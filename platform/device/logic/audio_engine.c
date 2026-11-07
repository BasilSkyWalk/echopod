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
    STEP_GAIN_Q16 = 58409,
    CHUNK_FRAMES = 32
};

static const uint32_t SUPPORTED_RATES[] = {44100, 48000, 88200, 96000, 176400, 192000};

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

static uint32_t repeat_for(uint32_t rate)
{
    for (uint32_t repeat = 1; repeat <= AUDIO_ENGINE_MAX_REPEAT; repeat++) {
        if (rate_supported(rate * repeat)) {
            return repeat;
        }
    }
    return 0;
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

void audio_engine_set_equalizer(AudioEngine *engine, const EqualizerCurve *curve)
{
    equalizer_set_curve(&engine->equalizer, curve);
}

static void use_output_rate(AudioEngine *engine)
{
    engine->output_rate = engine->decode_rate;
    equalizer_set_rate(&engine->equalizer, engine->source_rate);
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
    uint32_t repeat = repeat_for(info.sample_rate);
    if (repeat == 0) {
        close_file(engine);
        engine->refusal =
            (AudioRefusal){.problem = AUDIO_REFUSAL_RATE, .sample_rate = info.sample_rate};
        return false;
    }
    engine->skip_frames =
        (uint32_t)((uint64_t)start_ms * info.sample_rate / MILLISECONDS_PER_SECOND);
    engine->decode_rate = info.sample_rate * repeat;
    engine->source_rate = info.sample_rate;
    engine->repeat = repeat;
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
    engine->lead_frames = 0;
    engine->ramp_frames = 0;
    engine->ramp_done = 0;
}

static bool start_track(AudioEngine *engine, const char *path, uint32_t start_ms, uint32_t token)
{
    close_file(engine);
    reset_positions(engine);
    equalizer_reset(&engine->equalizer);
    engine->held[0] = 0;
    engine->held[1] = 0;
    engine->stats = (AudioEngineStats){0};
    engine->failed = false;
    if (!open_track(engine, path, start_ms)) {
        engine->state = AUDIO_ENGINE_STOPPED;
        engine->failed = true;
        engine->segments[0] = (AudioSegment){.token = token, .rate = 0};
        engine->segment_count = 1;
        return false;
    }
    use_output_rate(engine);
    push_segment(engine, path, token, 0, start_ms);
    engine->state = AUDIO_ENGINE_PLAYING;
    return true;
}

bool audio_engine_play(AudioEngine *engine, const char *path, uint32_t start_ms, uint32_t token)
{
    engine->has_next = false;
    engine->next_path[0] = '\0';
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
    if (engine->segment_count > 1) {
        audio_engine_queue_next(engine, engine->segments[1].path, engine->segments[1].token);
    } else if (engine->rate_pending) {
        engine->has_next = true;
    }
    bool started = start_track(engine, playing.path, position_ms, playing.token);
    if (started && state == AUDIO_ENGINE_PAUSED) {
        engine->state = AUDIO_ENGINE_PAUSED;
    }
    return started;
}

void audio_engine_lead_in(AudioEngine *engine, uint32_t silence_frames, uint32_t ramp_frames)
{
    if (engine->produced != 0 || engine->segment_count != 1) {
        return;
    }
    engine->lead_frames = silence_frames;
    engine->segments[0].start = silence_frames;
    uint32_t repeat = engine->repeat > 0 ? engine->repeat : 1;
    engine->ramp_frames = ramp_frames / repeat;
    engine->ramp_done = 0;
}

void audio_engine_close_source(AudioEngine *engine)
{
    close_file(engine);
}

bool audio_engine_next_pending(const AudioEngine *engine)
{
    return engine->has_next;
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

static int32_t clamped(int32_t sample)
{
    return sample > SAMPLE_MAX ? SAMPLE_MAX : sample < SAMPLE_MIN ? SAMPLE_MIN : sample;
}

static void widen_chunk(const int32_t *source, int32_t *chunk, uint32_t count, int shift)
{
    for (uint32_t index = 0; index < count; index++) {
        chunk[WORDS_PER_FRAME * index] = clamped(source[index] * (1 << shift));
    }
}

static void scale_chunk(const AudioEngine *engine, const int32_t *source, int32_t *chunk,
                        uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        chunk[WORDS_PER_FRAME * index] = to_output(source[index], engine->frame.bits, engine->gain);
    }
}

static void convert_chunk(const AudioEngine *engine, int32_t *chunk, uint32_t count)
{
    const int32_t *left = engine->frame.left + engine->pending_offset;
    const int32_t *right = engine->frame.right + engine->pending_offset;
    int bits = engine->frame.bits;
    if (engine->gain == UNITY_GAIN && bits <= OUTPUT_BITS) {
        widen_chunk(left, chunk, count, OUTPUT_BITS - bits);
        widen_chunk(right, chunk + 1, count, OUTPUT_BITS - bits);
        return;
    }
    scale_chunk(engine, left, chunk, count);
    scale_chunk(engine, right, chunk + 1, count);
}

static void write_chunk(AudioEngine *engine, const int32_t *chunk, uint32_t count)
{
    uint32_t frames = ring_frames(engine);
    uint32_t slot = engine->produced % frames;
    int32_t repeat = (int32_t)engine->repeat;
    engine->produced += count * engine->repeat;
    if (repeat == 1) {
        for (uint32_t index = 0; index < count; index++) {
            uint32_t *words = &engine->config.ring[WORDS_PER_FRAME * slot];
            words[0] = output_word(chunk[WORDS_PER_FRAME * index]);
            words[1] = output_word(chunk[WORDS_PER_FRAME * index + 1]);
            slot = slot + 1 == frames ? 0 : slot + 1;
        }
        engine->held[0] = chunk[WORDS_PER_FRAME * (count - 1)];
        engine->held[1] = chunk[WORDS_PER_FRAME * (count - 1) + 1];
        return;
    }
    for (uint32_t index = 0; index < count; index++) {
        for (int32_t step = 1; step <= repeat; step++) {
            uint32_t *words = &engine->config.ring[WORDS_PER_FRAME * slot];
            for (int channel = 0; channel < WORDS_PER_FRAME; channel++) {
                int32_t sample = chunk[WORDS_PER_FRAME * index + (uint32_t)channel];
                int32_t from = engine->held[channel];
                words[channel] = output_word(from + (sample - from) * step / repeat);
            }
            slot = slot + 1 == frames ? 0 : slot + 1;
        }
        engine->held[0] = chunk[WORDS_PER_FRAME * index];
        engine->held[1] = chunk[WORDS_PER_FRAME * index + 1];
    }
}

static uint32_t smaller(uint32_t first, uint32_t second)
{
    return first < second ? first : second;
}

static void ramp_chunk(AudioEngine *engine, int32_t *chunk, uint32_t count)
{
    for (uint32_t index = 0; index < count && engine->ramp_done < engine->ramp_frames; index++) {
        uint32_t gain = (engine->ramp_done << GAIN_SHIFT) / engine->ramp_frames;
        for (uint32_t channel = 0; channel < WORDS_PER_FRAME; channel++) {
            int32_t *sample = &chunk[WORDS_PER_FRAME * index + channel];
            *sample = (int32_t)floor_shift((int64_t)*sample * gain, GAIN_SHIFT);
        }
        engine->ramp_done++;
    }
}

static void copy_pending(AudioEngine *engine)
{
    int32_t chunk[WORDS_PER_FRAME * CHUNK_FRAMES];
    while (engine->pending_frames > 0 && free_space(engine) >= engine->repeat) {
        uint32_t room = free_space(engine) / engine->repeat;
        uint32_t count = smaller(smaller(engine->pending_frames, room), CHUNK_FRAMES);
        convert_chunk(engine, chunk, count);
        if (equalizer_active(&engine->equalizer) && !engine->frame.equalized) {
            equalizer_process(&engine->equalizer, chunk, count);
        }
        ramp_chunk(engine, chunk, count);
        write_chunk(engine, chunk, count);
        engine->pending_offset += count;
        engine->pending_frames -= count;
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
    uint32_t previous_rate = engine->source_rate;
    if (!open_track(engine, engine->next_path, 0)) {
        engine->ended = true;
        engine->end_at = engine->produced;
        engine->failed = true;
        return;
    }
    if (engine->decode_rate != engine->output_rate) {
        engine->rate_pending = true;
        engine->rate_end = engine->produced;
        return;
    }
    if (engine->source_rate != previous_rate) {
        equalizer_set_rate(&engine->equalizer, engine->source_rate);
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

static void write_silence(AudioEngine *engine, uint32_t *frames)
{
    while (*frames > 0 && free_space(engine) > 0) {
        write_frame(engine, 0, 0);
        (*frames)--;
    }
}

void audio_engine_pump(AudioEngine *engine, uint32_t consumed)
{
    audio_engine_observe(engine, consumed);
    if (engine->rate_pending) {
        uint32_t room = free_space(engine);
        write_silence(engine, &room);
        return;
    }
    write_silence(engine, &engine->lead_frames);
    if (engine->lead_frames > 0) {
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
        uint32_t room = free_space(engine);
        write_silence(engine, &room);
    }
}

bool audio_engine_wants_rate(const AudioEngine *engine, uint32_t *rate)
{
    if (!engine->rate_pending || (int32_t)(engine->consumed - engine->rate_end) < 0) {
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
    use_output_rate(engine);
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
    uint32_t played =
        (int32_t)(engine->consumed - segment->start) > 0 ? engine->consumed - segment->start : 0;
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
