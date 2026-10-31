#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    HAL_AUDIO_VOLUME_MAX = 50
};

typedef enum HalAudioState {
    HAL_AUDIO_STOPPED,
    HAL_AUDIO_PLAYING,
    HAL_AUDIO_PAUSED
} HalAudioState;

typedef enum HalAudioGain {
    HAL_AUDIO_GAIN_LOW,
    HAL_AUDIO_GAIN_HIGH
} HalAudioGain;

typedef enum HalAudioOutput {
    HAL_AUDIO_OUTPUT_AUTO,
    HAL_AUDIO_OUTPUT_SINGLE_ENDED,
    HAL_AUDIO_OUTPUT_BALANCED
} HalAudioOutput;

typedef enum HalAudioJack {
    HAL_AUDIO_JACK_NONE,
    HAL_AUDIO_JACK_SINGLE_ENDED,
    HAL_AUDIO_JACK_BALANCED
} HalAudioJack;

typedef enum HalAudioProblem {
    HAL_AUDIO_PROBLEM_NONE,
    HAL_AUDIO_PROBLEM_UNREADABLE,
    HAL_AUDIO_PROBLEM_FORMAT,
    HAL_AUDIO_PROBLEM_RATE
} HalAudioProblem;

typedef struct HalAudioRefusal {
    HalAudioProblem problem;
    uint32_t sample_rate;
} HalAudioRefusal;

typedef enum HalAudioCodec {
    HAL_AUDIO_CODEC_UNKNOWN,
    HAL_AUDIO_CODEC_FLAC,
    HAL_AUDIO_CODEC_MP3,
    HAL_AUDIO_CODEC_WAV,
    HAL_AUDIO_CODEC_ALAC,
    HAL_AUDIO_CODEC_AAC,
    HAL_AUDIO_CODEC_OPUS
} HalAudioCodec;

typedef struct HalAudioFormat {
    HalAudioCodec codec;
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
    uint32_t bitrate_kbps;
} HalAudioFormat;

typedef struct HalAudioStatus {
    HalAudioState state;
    uint32_t token;
    uint32_t position_ms;
    bool failed;
    bool idle;
    HalAudioFormat format;
} HalAudioStatus;

typedef struct HalAudioStats {
    uint32_t buffered_ms;
    uint32_t underruns;
    uint32_t output_underruns;
    uint32_t core1_load;
    uint32_t blocks;
    uint32_t decode_us;
    uint32_t decode_max_us;
    uint32_t decode_total_us;
    uint32_t read_us;
    uint32_t read_max_us;
    uint32_t read_total_us;
    uint32_t busy_us;
    uint32_t elapsed_us;
} HalAudioStats;

typedef enum HalToneResult {
    HAL_TONE_PLAYED,
    HAL_TONE_NO_DAC,
    HAL_TONE_SETUP_FAILED,
    HAL_TONE_NOT_READY,
    HAL_TONE_STALLED,
    HAL_TONE_NO_DMAC
} HalToneResult;

typedef struct HalToneDetail {
    uint8_t stage;
    uint8_t pending;
} HalToneDetail;

typedef struct HalDmaToneStatus {
    bool running;
    uint32_t position;
} HalDmaToneStatus;

typedef struct HalJackPin {
    bool high;
    uint8_t boot_function;
    bool boot_output;
    bool pull;
} HalJackPin;

typedef struct HalJackPins {
    HalJackPin single_ended;
    HalJackPin balanced;
} HalJackPins;

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token);
void hal_audio_queue_next(const char *path, uint32_t token);
void hal_audio_pause(void);
void hal_audio_resume(void);
void hal_audio_seek(uint32_t position_ms);
void hal_audio_set_volume(uint8_t level);
void hal_audio_set_gain(HalAudioGain gain);
void hal_audio_set_output(HalAudioOutput output);
HalAudioStatus hal_audio_status(void);
HalAudioStats hal_audio_stats(void);
void hal_audio_service(void);
HalToneResult hal_audio_test_tone(void);
HalToneDetail hal_audio_tone_detail(void);
HalToneResult hal_audio_dma_tone_start(uint32_t sample_rate);
void hal_audio_dma_tone_stop(void);
HalDmaToneStatus hal_audio_dma_tone_status(void);
HalJackPins hal_audio_jack_pins(void);
HalAudioJack hal_audio_jack(void);
HalAudioRefusal hal_audio_refusal(void);
