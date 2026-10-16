#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "codec/flac.h"

enum {
    AUDIO_ENGINE_PATH_CAPACITY = 256,
    AUDIO_ENGINE_SEGMENTS = 4,
    AUDIO_ENGINE_VOLUME_MAX = 50
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
} AudioFrame;

typedef struct AudioTrackInfo {
    FlacInfo stream;
    uint32_t audio_bytes;
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
} AudioEngineConfig;

typedef struct AudioSegment {
    uint32_t token;
    uint32_t start;
    uint32_t base_ms;
    uint32_t rate;
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
    AudioTrackInfo decode_track;
    AudioFrame frame;
    bool holding_frame;
    uint32_t pending_frames;
    uint32_t pending_offset;
    uint32_t skip_frames;
    bool ended;
    uint32_t end_at;
    bool rate_pending;
    uint32_t output_rate;
    bool has_next;
    uint32_t next_token;
    char next_path[AUDIO_ENGINE_PATH_CAPACITY];
    uint32_t gain;
    AudioEngineStats stats;
} AudioEngine;

void audio_engine_init(AudioEngine *engine, AudioEngineConfig config);
void audio_engine_set_volume(AudioEngine *engine, uint8_t level);
bool audio_engine_play(AudioEngine *engine, const char *path, uint32_t start_ms, uint32_t token);
const char *audio_engine_current_path(const AudioEngine *engine);
void audio_engine_queue_next(AudioEngine *engine, const char *path, uint32_t token);
bool audio_engine_seek(AudioEngine *engine, uint32_t position_ms);
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
