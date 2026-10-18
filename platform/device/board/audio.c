#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "core/format.h"
#include "core1_region.h"
#include "dac.h"
#include "device/audio_engine.h"
#include "device/audio_math.h"
#include "device/dac_sequence.h"
#include "device/dma_loop.h"
#include "device/flac_source.h"
#include "dmac.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/storage.h"
#include "hal/system.h"

enum {
    RING_BLOCKS = 4,
    BLOCK_WORDS = 4000,
    RING_WORDS = RING_BLOCKS * BLOCK_WORDS,
    PREFILL_WORDS = 8000,
    PREFILL_TIMEOUT_MS = 1000,
    FILL_BUDGET_BYTES = 8192,
    IDLE_SHUTDOWN_MS = 3000,
    AUDIO_CLOCK_MHZ = 200,
    TONE_RATE_HZ = 44100,
    TONE_PERIOD_SAMPLES = 100,
    TONE_AMPLITUDE = 9000,
    MILLISECONDS_PER_SECOND = 1000
};

static FlacDecoder decoder;
static FlacSource software_source;
static Core1Source core1_source;
static bool using_core1;
static bool resume_after_clock_change;
static bool paused_before_clock_change;
static uint32_t position_before_clock_change;
static uint32_t ring[RING_WORDS];
static AudioEngine engine;
static DmaItem items[RING_BLOCKS];
static HalFile file;
static bool engine_ready;
static bool path_up;
static uint32_t path_rate;
static bool transferring;
static uint32_t consumed;
static uint32_t last_position;
static uint32_t ring_words;
static bool stopped_waiting;
static uint32_t stopped_since_ms;
static bool tone_active;
static HalAudioStats stats;
static uint32_t read_us_pending;
static uint32_t output_started_us;

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const HalFile *open = context;
    uint32_t started_us = hal_clock_microseconds();
    bool read = hal_file_read(*open, offset, buffer, length);
    uint32_t read_us = hal_clock_microseconds() - started_us;
    read_us_pending += read_us;
    stats.read_total_us += read_us;
    return read;
}

static uint32_t larger(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

static void pump(uint32_t consumed_words)
{
    uint32_t frames = audio_engine_stats(&engine).frames;
    read_us_pending = 0;
    uint32_t decode_started_us = hal_clock_microseconds();
    if (using_core1) {
        core1_source_fill(&core1_source);
    }
    audio_engine_pump(&engine, consumed_words);
    uint32_t elapsed_us = hal_clock_microseconds() - decode_started_us;
    stats.busy_us += elapsed_us;
    if (audio_engine_stats(&engine).frames == frames) {
        return;
    }
    stats.blocks++;
    stats.decode_us = elapsed_us;
    stats.decode_max_us = larger(stats.decode_max_us, stats.decode_us);
    stats.decode_total_us += stats.decode_us;
    stats.read_us = read_us_pending;
    stats.read_max_us = larger(stats.read_max_us, stats.read_us);
}

static bool open_file(void *context, const char *path, FlacInput *input)
{
    (void)context;
    if (!hal_file_open(&file, path)) {
        return false;
    }
    *input = (FlacInput){.read = read_file, .context = &file, .size = file.size};
    return true;
}

static void close_file(void *context)
{
    (void)context;
    hal_file_close(&file);
}

static AudioSource chosen_source(void)
{
    return using_core1 ? core1_source_interface(&core1_source)
                       : flac_source_interface(&software_source);
}

static bool open_chosen(void *context, const char *path, uint32_t start_ms, AudioTrackInfo *track)
{
    (void)context;
    using_core1 = board_core1_start_stream();
    AudioSource source = chosen_source();
    return source.open(source.context, path, start_ms, track);
}

static AudioFrameResult next_chosen(void *context, AudioFrame *frame)
{
    (void)context;
    AudioSource source = chosen_source();
    return source.next(source.context, frame);
}

static void release_chosen(void *context)
{
    (void)context;
    AudioSource source = chosen_source();
    source.release(source.context);
}

static void close_chosen(void *context)
{
    (void)context;
    AudioSource source = chosen_source();
    source.close(source.context);
}

static void ensure_engine(void)
{
    if (engine_ready) {
        return;
    }
    FlacFileOpener files = {.open = open_file, .close = close_file, .context = NULL};
    flac_source_init(&software_source, files, &decoder);
    core1_source_init(&core1_source, core1_region(), files, &decoder, FILL_BUDGET_BYTES);
    AudioSource source = {open_chosen, next_chosen, release_chosen, close_chosen, NULL};
    audio_engine_init(
        &engine, (AudioEngineConfig){.source = source, .ring = ring, .ring_words = RING_WORDS});
    engine_ready = true;
}

static uint32_t ring_address(void)
{
    return (uint32_t)(uintptr_t)ring;
}

static void update_consumed(void)
{
    if (!transferring) {
        return;
    }
    uint32_t position = dma_loop_position(dmac_source(), ring_address(), ring_words);
    consumed += (position + ring_words - last_position) % ring_words;
    last_position = position;
}

static void stop_transfer(void)
{
    if (!transferring) {
        return;
    }
    dac_path_set_muted(true);
    dac_path_stop_transfer();
    dmac_stop();
    transferring = false;
}

static void stop_path(void)
{
    stop_transfer();
    if (path_up) {
        dac_path_stop();
        path_up = false;
    }
}

static bool ensure_path(uint32_t rate)
{
    if (path_up && path_rate == rate) {
        return true;
    }
    stop_path();
    if (dac_path_start(rate) != HAL_TONE_PLAYED) {
        return false;
    }
    path_up = true;
    path_rate = rate;
    dac_path_output_on();
    return true;
}

static bool start_transfer(uint32_t words)
{
    ring_words = words;
    consumed = 0;
    last_position = 0;
    uint32_t blocks = words / BLOCK_WORDS;
    if (!dma_ring_items(items, blocks, ring_address(), BLOCK_WORDS, (uint32_t)(uintptr_t)items) ||
        !dmac_start_loop(&items[0])) {
        return false;
    }
    dac_path_set_muted(false);
    dac_path_start_transfer();
    transferring = true;
    return true;
}

static bool start_output(void)
{
    if (!ensure_path(audio_engine_rate(&engine))) {
        return false;
    }
    uint32_t started_ms = hal_clock_milliseconds();
    while (audio_engine_buffered(&engine, 0) < PREFILL_WORDS &&
           hal_clock_milliseconds() - started_ms < PREFILL_TIMEOUT_MS) {
        pump(0);
    }
    stats = (HalAudioStats){0};
    output_started_us = hal_clock_microseconds();
    stopped_waiting = false;
    return start_transfer(RING_WORDS);
}

static void stop_tone(void)
{
    if (tone_active) {
        stop_path();
        tone_active = false;
    }
}

static void ensure_audio_clock(void)
{
    if (hal_system_clock_mhz() < AUDIO_CLOCK_MHZ) {
        hal_system_set_clock_mhz(AUDIO_CLOCK_MHZ);
    }
}

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token)
{
    ensure_engine();
    ensure_audio_clock();
    stop_tone();
    stop_transfer();
    if (!audio_engine_play(&engine, path, start_ms, token)) {
        return false;
    }
    if (!start_output()) {
        audio_engine_stop(&engine);
        return false;
    }
    return true;
}

void hal_audio_queue_next(const char *path, uint32_t token)
{
    ensure_engine();
    audio_engine_queue_next(&engine, path, token);
}

void hal_audio_pause(void)
{
    ensure_engine();
    if (transferring) {
        dac_path_set_muted(true);
        dmac_suspend();
    }
    audio_engine_pause(&engine);
}

void hal_audio_resume(void)
{
    ensure_engine();
    if (transferring) {
        dmac_resume();
        dac_path_set_muted(false);
    }
    audio_engine_resume(&engine);
}

void hal_audio_seek(uint32_t position_ms)
{
    ensure_engine();
    ensure_audio_clock();
    bool paused = audio_engine_status(&engine).state == AUDIO_ENGINE_PAUSED;
    stop_transfer();
    if (!audio_engine_seek(&engine, position_ms) || !start_output()) {
        audio_engine_stop(&engine);
        return;
    }
    if (paused) {
        dac_path_set_muted(true);
        dmac_suspend();
    }
}

void hal_audio_set_volume(uint8_t level)
{
    dac_path_set_volume(dac_volume_value(level));
}

void hal_audio_set_gain(HalAudioGain gain)
{
    dac_path_set_gain(gain == HAL_AUDIO_GAIN_HIGH);
}

void hal_audio_set_output(HalAudioOutput output)
{
    (void)output;
}

static HalAudioState state_of(AudioEngineState state)
{
    switch (state) {
    case AUDIO_ENGINE_PLAYING:
        return HAL_AUDIO_PLAYING;
    case AUDIO_ENGINE_PAUSED:
        return HAL_AUDIO_PAUSED;
    case AUDIO_ENGINE_STOPPED:
        return HAL_AUDIO_STOPPED;
    }
    return HAL_AUDIO_STOPPED;
}

static HalAudioFormat format_of(AudioTrackInfo track)
{
    FlacInfo stream = track.stream;
    if (stream.sample_rate == 0) {
        return (HalAudioFormat){.codec = HAL_AUDIO_CODEC_UNKNOWN};
    }
    return (HalAudioFormat){
        .codec = HAL_AUDIO_CODEC_FLAC,
        .sample_rate = stream.sample_rate,
        .bits_per_sample = stream.bits_per_sample,
        .channels = stream.channels,
        .bitrate_kbps = average_kbps(track.audio_bytes, stream.total_samples, stream.sample_rate),
    };
}

static bool output_idle(void)
{
    return !path_up && !board_core1_streaming();
}

HalAudioStatus hal_audio_status(void)
{
    ensure_engine();
    update_consumed();
    audio_engine_observe(&engine, consumed);
    AudioEngineStatus status = audio_engine_status(&engine);
    return (HalAudioStatus){.state = state_of(status.state),
                            .token = status.token,
                            .position_ms = status.position_ms,
                            .failed = status.failed,
                            .idle = output_idle(),
                            .format = format_of(status.track)};
}

static void change_rate(void)
{
    uint32_t rate = 0;
    if (!audio_engine_wants_rate(&engine, &rate)) {
        return;
    }
    stop_transfer();
    audio_engine_restart(&engine);
    if (!start_output()) {
        audio_engine_stop(&engine);
    }
}

static void shut_down_when_idle(void)
{
    if (audio_engine_status(&engine).state != AUDIO_ENGINE_STOPPED) {
        stopped_waiting = false;
        return;
    }
    uint32_t now_ms = hal_clock_milliseconds();
    if (!stopped_waiting) {
        stopped_waiting = true;
        stopped_since_ms = now_ms;
        return;
    }
    if (now_ms - stopped_since_ms >= IDLE_SHUTDOWN_MS) {
        stop_path();
        board_core1_stop();
        using_core1 = false;
        stopped_waiting = false;
    }
}

void hal_audio_service(void)
{
    if (!engine_ready || !transferring || tone_active) {
        return;
    }
    update_consumed();
    pump(consumed);
    change_rate();
    shut_down_when_idle();
}

HalAudioStats hal_audio_stats(void)
{
    HalAudioStats result = stats;
    if (!engine_ready || !transferring || tone_active) {
        return result;
    }
    result.underruns = audio_engine_stats(&engine).underruns;
    result.elapsed_us = hal_clock_microseconds() - output_started_us;
    uint32_t rate = audio_engine_rate(&engine);
    if (rate > 0) {
        result.buffered_ms = (uint32_t)((uint64_t)audio_engine_buffered(&engine, consumed) *
                                        MILLISECONDS_PER_SECOND / rate);
    }
    return result;
}

HalToneResult hal_audio_dma_tone_start(void)
{
    ensure_engine();
    stop_tone();
    stop_transfer();
    audio_engine_stop(&engine);
    if (!dmac_present()) {
        return HAL_TONE_NO_DMAC;
    }
    stop_path();
    HalToneResult result = dac_path_start(TONE_RATE_HZ);
    if (result != HAL_TONE_PLAYED) {
        return result;
    }
    path_up = true;
    path_rate = TONE_RATE_HZ;
    int16_t samples[TONE_PERIOD_SAMPLES];
    tone_fill(samples, TONE_PERIOD_SAMPLES, TONE_AMPLITUDE);
    for (uint32_t index = 0; index < RING_WORDS; index++) {
        uint16_t sample = (uint16_t)samples[index % TONE_PERIOD_SAMPLES];
        ring[index] = (uint32_t)sample | (uint32_t)sample << 16;
    }
    dac_path_output_on();
    if (!start_transfer(RING_WORDS)) {
        stop_path();
        return HAL_TONE_NO_DMAC;
    }
    tone_active = true;
    return HAL_TONE_PLAYED;
}

void hal_audio_dma_tone_stop(void)
{
    stop_tone();
}

HalDmaToneStatus hal_audio_dma_tone_status(void)
{
    if (!tone_active) {
        return (HalDmaToneStatus){.running = false, .position = 0};
    }
    uint32_t position = dma_loop_position(dmac_source(), ring_address(), ring_words);
    return (HalDmaToneStatus){.running = dmac_running(), .position = position};
}

const char *board_audio_current_path(void)
{
    return engine_ready ? audio_engine_current_path(&engine) : NULL;
}

void board_audio_before_clock_change(void)
{
    resume_after_clock_change = false;
    if (!board_core1_streaming()) {
        return;
    }
    if (engine_ready && transferring) {
        HalAudioStatus status = hal_audio_status();
        resume_after_clock_change = status.state != HAL_AUDIO_STOPPED;
        paused_before_clock_change = status.state == HAL_AUDIO_PAUSED;
        position_before_clock_change = status.position_ms;
        audio_engine_stop(&engine);
    }
    board_core1_stop();
    using_core1 = false;
}

void board_audio_after_clock_change(void)
{
    if (!resume_after_clock_change) {
        return;
    }
    resume_after_clock_change = false;
    hal_audio_seek(position_before_clock_change);
    if (paused_before_clock_change) {
        hal_audio_pause();
    }
}
