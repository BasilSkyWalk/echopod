#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"
#include "dsp/equalizer.h"
#include "dsp/speed.h"

enum {
    AUDIO_ENGINE_PATH_CAPACITY = 256,
    AUDIO_ENGINE_SEGMENTS = 4,
    AUDIO_ENGINE_VOLUME_MAX = 50,
    AUDIO_ENGINE_MAX_REPEAT = 6
};

typedef enum AudioEngineState {
    AUDIO_ENGINE_STOPPED,
    AUDIO_ENGINE_PLAYING,
    AUDIO_ENGINE_PAUSED
} AudioEngineState;

typedef enum AudioFrameResult {
    AUDIO_FRAME_READY,
    AUDIO_FRAME_WAIT,
    AUDIO_FRAME_END,
    AUDIO_FRAME_ERROR
} AudioFrameResult;

typedef struct AudioFrame {
    const int32_t *left;
    const int32_t *right;
    uint32_t first_sample;
    uint32_t samples;
    uint8_t bits;
    bool equalized;
} AudioFrame;

typedef enum AudioCodec {
    AUDIO_CODEC_FLAC,
    AUDIO_CODEC_WAV,
    AUDIO_CODEC_MP3,
    AUDIO_CODEC_ALAC,
    AUDIO_CODEC_AAC,
    AUDIO_CODEC_OPUS,
    AUDIO_CODEC_QOA
} AudioCodec;

typedef struct AudioTrackInfo {
    FlacInfo stream;
    uint32_t audio_bytes;
    AudioCodec codec;
} AudioTrackInfo;

typedef struct AudioSource {
    bool (*open)(void *context, const char *path, uint32_t start_ms, AudioTrackInfo *track);
    AudioFrameResult (*next)(void *context, AudioFrame *frame);
    void (*release)(void *context);
    void (*close)(void *context);
    void *context;
} AudioSource;

typedef struct AudioEngineConfig {
    AudioSource source;
    uint32_t *ring;
    uint32_t ring_words;
    SpeedStage *speed_stage;
} AudioEngineConfig;

typedef enum AudioRefusalProblem {
    AUDIO_REFUSAL_NONE,
    AUDIO_REFUSAL_UNREADABLE,
    AUDIO_REFUSAL_RATE
} AudioRefusalProblem;

typedef struct AudioRefusal {
    AudioRefusalProblem problem;
    uint32_t sample_rate;
} AudioRefusal;

typedef struct AudioSegment {
    uint32_t token;
    uint32_t start;
    uint32_t base_ms;
    uint32_t rate;
    uint8_t quarters;
    AudioTrackInfo track;
    char path[AUDIO_ENGINE_PATH_CAPACITY];
} AudioSegment;

typedef struct AudioEngineStatus {
    AudioEngineState state;
    uint32_t token;
    uint32_t position_ms;
    bool failed;
    AudioTrackInfo track;
} AudioEngineStatus;

typedef struct AudioEngineStats {
    uint32_t underruns;
    uint32_t frames;
} AudioEngineStats;

typedef struct AudioEngine {
    AudioEngineConfig config;
    AudioEngineState state;
    bool failed;
    uint32_t produced;
    uint32_t consumed;
    AudioSegment segments[AUDIO_ENGINE_SEGMENTS];
    int segment_count;
    bool decoding;
    uint32_t decode_rate;
    uint32_t source_rate;
    uint32_t repeat;
    int32_t held[2];
    AudioTrackInfo decode_track;
    AudioFrame frame;
    bool holding_frame;
    uint32_t pending_frames;
    uint32_t pending_offset;
    uint32_t skip_frames;
    bool ended;
    uint32_t end_at;
    bool rate_pending;
    uint32_t rate_end;
    uint32_t lead_frames;
    uint32_t ramp_frames;
    uint32_t ramp_done;
    uint32_t output_rate;
    bool has_next;
    bool next_replaced;
    uint32_t next_token;
    char next_path[AUDIO_ENGINE_PATH_CAPACITY];
    uint32_t gain;
    Equalizer equalizer;
    SpeedSetting speed;
    bool speeding;
    bool draining;
    bool stage_equalized;
    uint8_t track_quarters;
    AudioEngineStats stats;
    AudioRefusal refusal;
} AudioEngine;

void audio_engine_init(AudioEngine *engine, AudioEngineConfig config);
void audio_engine_set_volume(AudioEngine *engine, uint8_t level);
void audio_engine_set_equalizer(AudioEngine *engine, const EqualizerCurve *curve);
void audio_engine_set_speed(AudioEngine *engine, SpeedSetting speed);
bool audio_engine_play(AudioEngine *engine, const char *path, uint32_t start_ms, uint32_t token);
const char *audio_engine_current_path(const AudioEngine *engine);
void audio_engine_queue_next(AudioEngine *engine, const char *path, uint32_t token);
bool audio_engine_seek(AudioEngine *engine, uint32_t position_ms);
void audio_engine_lead_in(AudioEngine *engine, uint32_t silence_frames, uint32_t ramp_frames);
void audio_engine_close_source(AudioEngine *engine);
bool audio_engine_next_pending(const AudioEngine *engine);
void audio_engine_pause(AudioEngine *engine);
void audio_engine_resume(AudioEngine *engine);
void audio_engine_stop(AudioEngine *engine);
void audio_engine_observe(AudioEngine *engine, uint32_t consumed);
void audio_engine_pump(AudioEngine *engine, uint32_t consumed);
AudioEngineStats audio_engine_stats(const AudioEngine *engine);
bool audio_engine_wants_rate(const AudioEngine *engine, uint32_t *rate);
void audio_engine_restart(AudioEngine *engine);
uint32_t audio_engine_rate(const AudioEngine *engine);
uint32_t audio_engine_buffered(const AudioEngine *engine, uint32_t consumed);
AudioEngineStatus audio_engine_status(const AudioEngine *engine);
AudioRefusal audio_engine_refusal(const AudioEngine *engine);
