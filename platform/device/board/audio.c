#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "codec/mp4.h"
#include "codec/ogg.h"
#include "core/format.h"
#include "core1_region.h"
#include "dac.h"
#include "device/audio_engine.h"
#include "device/audio_math.h"
#include "device/dac_sequence.h"
#include "device/dma_loop.h"
#include "device/flac_source.h"
#include "device/mp3_probe.h"
#include "device/wav_source.h"
#include "dmac.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/storage.h"
#include "hal/system.h"

enum {
    RING_BLOCKS = 7,
    BLOCK_WORDS = 4000,
    RING_WORDS = RING_BLOCKS * BLOCK_WORDS,
    WORDS_PER_FRAME = 2,
    RING_FRAMES = RING_WORDS / WORDS_PER_FRAME,
    PREFILL_DIVISOR = 10,
    PREFILL_TIMEOUT_MS = 1000,
    FILL_BUDGET_BYTES = 8192,
    IDLE_SHUTDOWN_MS = 3000,
    PAUSE_REST_MS = 30000,
    AUDIO_CLOCK_MHZ = 200,
    SLOW_CLOCK_MHZ = 96,
    SLOW_CLOCK_MAX_RATE = 48000,
    FLAC_RATE_OFFSET = 18,
    WAV_FORMAT_OFFSET = 12,
    WAV_RATE_OFFSET = 24,
    TONE_PERIOD_SAMPLES = 100,
    TONE_AMPLITUDE = 9000,
    MILLISECONDS_PER_SECOND = 1000,
    JACK_POLL_MS = 10,
    SNIFF_BYTES = 12,
    LIGHT_SNIFF_BYTES = 28,
    COLD_LEAD_MS = 60,
    COLD_SETTLE_MS = 40,
    WARM_LEAD_MS = 20,
    WARM_SETTLE_MS = 6,
    START_RAMP_MS = 2,
    RESUME_RAMP_MS = 10,
    LEAD_GUARD_MS = 4,
    SETTLE_TIMEOUT_MS = 300
};

typedef enum SourceKind {
    SOURCE_FLAC,
    SOURCE_WAV,
    SOURCE_MP3,
    SOURCE_ALAC,
    SOURCE_AAC,
    SOURCE_OPUS,
    SOURCE_UNKNOWN
} SourceKind;

typedef struct FlacMemory {
    FlacDecoder decoder;
    FlacBlockBuffer block;
} FlacMemory;

static union {
    FlacMemory flac;
    WavBuffers wav;
} decoding __attribute__((section(".high_ram")));
static uint32_t music_ring[RING_WORDS];
static WavSource wav_source;
static SourceKind source_kind;
static bool unknown_format;
static FlacSource software_source;
static Core1Source core1_source;
static bool using_core1;
static bool resume_after_clock_change;
static bool paused_before_clock_change;
static uint32_t position_before_clock_change;
static AudioEngine engine;
static EqualizerCurve equalizer_curve;
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
static char last_path[AUDIO_ENGINE_PATH_CAPACITY];
static uint8_t volume_level;
static uint8_t stream_hardware_value;
static void (*stream_service)(void);
static bool volume_set;
static bool bluetooth_louder;
static HalAudioOutput output_setting;
static uint32_t last_jack_poll_ms;
static HalAudioStats stats;
static uint32_t read_us_pending;
static uint32_t output_started_us;
static uint32_t total_underruns;
static bool resting;
static bool slow_allowed;
static bool deferred;
static bool queued_light;
static int queued_program;
static uint32_t queued_token;
static char queued_path[AUDIO_ENGINE_PATH_CAPACITY];
static uint32_t rest_position_ms;
static uint32_t paused_since_ms;
static uint32_t lead_frames;

static void clear_ring(void);

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

static void pump(uint32_t consumed_frames)
{
    uint32_t frames = audio_engine_stats(&engine).frames;
    read_us_pending = 0;
    uint32_t decode_started_us = hal_clock_microseconds();
    if (using_core1) {
        core1_source_fill(&core1_source);
    }
    audio_engine_pump(&engine, consumed_frames);
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

static bool tag_is(const uint8_t *bytes, const char *tag)
{
    return bytes[0] == (uint8_t)tag[0] && bytes[1] == (uint8_t)tag[1] &&
           bytes[2] == (uint8_t)tag[2] && bytes[3] == (uint8_t)tag[3];
}

static SourceKind kind_of(const char *path)
{
    HalFile local;
    if (!hal_file_open(&local, path)) {
        return SOURCE_FLAC;
    }
    FlacInput input = {.read = read_file, .context = &local, .size = local.size};
    uint8_t magic[SNIFF_BYTES] = {0};
    bool read = local.size >= SNIFF_BYTES && read_file(&local, 0, magic, sizeof magic);
    if (read && mp4_signature(magic)) {
        Mp4Track track;
        bool probed = mp4_probe(input, &track);
        hal_file_close(&local);
        if (probed && track.codec == MP4_CODEC_ALAC) {
            return SOURCE_ALAC;
        }
        return probed && track.object_type == MP4_OBJECT_AAC_LC ? SOURCE_AAC : SOURCE_UNKNOWN;
    }
    if (read && ogg_signature(magic)) {
        OggOpusInfo opus;
        bool probed = ogg_opus_probe(input, &opus);
        hal_file_close(&local);
        return probed ? SOURCE_OPUS : SOURCE_UNKNOWN;
    }
    hal_file_close(&local);
    if (!read || tag_is(magic, "fLaC")) {
        return SOURCE_FLAC;
    }
    if (tag_is(magic, "RIFF") && tag_is(magic + 8, "WAVE")) {
        return SOURCE_WAV;
    }
    return mp3_signature(magic) ? SOURCE_MP3 : SOURCE_UNKNOWN;
}

static Core1Program program_for(SourceKind kind)
{
    if (kind == SOURCE_MP3) {
        return CORE1_PROGRAM_MP3;
    }
    if (kind == SOURCE_OPUS) {
        return CORE1_PROGRAM_OPUS;
    }
    return kind == SOURCE_AAC ? CORE1_PROGRAM_AAC : CORE1_PROGRAM_FLAC;
}

static int program_of(SourceKind kind)
{
    switch (kind) {
    case SOURCE_FLAC:
    case SOURCE_ALAC:
        return CORE1_PROGRAM_FLAC;
    case SOURCE_MP3:
        return CORE1_PROGRAM_MP3;
    case SOURCE_AAC:
        return CORE1_PROGRAM_AAC;
    case SOURCE_OPUS:
        return CORE1_PROGRAM_OPUS;
    default:
        return -1;
    }
}

static AudioSource chosen_source(void)
{
    if (source_kind == SOURCE_WAV) {
        return wav_source_interface(&wav_source);
    }
    if (source_kind == SOURCE_MP3) {
        return core1_source_mp3_interface(&core1_source);
    }
    if (source_kind == SOURCE_ALAC || source_kind == SOURCE_AAC) {
        return core1_source_mp4_interface(&core1_source);
    }
    if (source_kind == SOURCE_OPUS) {
        return core1_source_ogg_interface(&core1_source);
    }
    return using_core1 ? core1_source_interface(&core1_source)
                       : flac_source_interface(&software_source);
}

static void remember(char *copy, const char *path)
{
    uint32_t index = 0;
    while (path[index] != '\0' && index + 1 < AUDIO_ENGINE_PATH_CAPACITY) {
        copy[index] = path[index];
        index++;
    }
    copy[index] = '\0';
}

const char *board_audio_last_path(void)
{
    return last_path[0] != '\0' ? last_path : NULL;
}

static bool open_chosen(void *context, const char *path, uint32_t start_ms, AudioTrackInfo *track)
{
    (void)context;
    remember(last_path, path);
    source_kind = kind_of(path);
    unknown_format = source_kind == SOURCE_UNKNOWN;
    if (unknown_format) {
        return false;
    }
    if (source_kind == SOURCE_FLAC) {
        flac_use_block_buffer(&decoding.flac.decoder, &decoding.flac.block);
        using_core1 = board_core1_start_stream(CORE1_PROGRAM_FLAC);
    }
    if (source_kind != SOURCE_FLAC && source_kind != SOURCE_WAV) {
        using_core1 = board_core1_start_stream(program_for(source_kind));
        if (!using_core1) {
            return false;
        }
    }
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

static uint32_t *ring(void)
{
    return music_ring;
}

static void signal_core1(void)
{
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("sev" ::: "memory");
}

static void ensure_engine(void)
{
    if (engine_ready) {
        return;
    }
    FlacFileOpener files = {.open = open_file, .close = close_file, .context = NULL};
    flac_source_init(&software_source, files, &decoding.flac.decoder);
    core1_source_init(&core1_source, core1_region(), files, &decoding.flac.decoder,
                      FILL_BUDGET_BYTES);
    core1_source_set_wake(&core1_source, signal_core1);
    wav_source_init(&wav_source, files, &decoding.wav);
    AudioSource source = {open_chosen, next_chosen, release_chosen, close_chosen, NULL};
    audio_engine_init(
        &engine, (AudioEngineConfig){.source = source, .ring = ring(), .ring_words = RING_WORDS});
    audio_engine_set_equalizer(&engine, &equalizer_curve);
    engine_ready = true;
}

void *hal_system_borrow_listing(uint32_t *size)
{
    board_audio_shut_down();
    *size = (uint32_t)sizeof music_ring;
    return music_ring;
}

static uint32_t ring_address(void)
{
    return (uint32_t)(uintptr_t)music_ring;
}

static void update_consumed(void)
{
    if (!transferring) {
        return;
    }
    uint32_t frames = ring_words / WORDS_PER_FRAME;
    uint32_t position =
        dma_loop_position(dmac_source(), ring_address(), ring_words) / WORDS_PER_FRAME;
    consumed += (position + frames - last_position) % frames;
    last_position = position;
}

static void serve_fade(void)
{
    if (stream_service != NULL) {
        stream_service();
    } else if (engine_ready && transferring && !tone_active) {
        update_consumed();
        pump(consumed);
    }
}

static uint32_t frames_in(uint32_t milliseconds)
{
    return audio_engine_rate(&engine) * milliseconds / MILLISECONDS_PER_SECOND;
}

static bool in_lead(void)
{
    update_consumed();
    return transferring && consumed + frames_in(LEAD_GUARD_MS) <= lead_frames;
}

static void set_muted(bool mute)
{
    if (in_lead()) {
        dac_path_set_muted_now(mute);
    } else {
        dac_path_set_muted(mute);
    }
}

static void stop_transfer(void)
{
    if (!transferring) {
        return;
    }
    set_muted(true);
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

static bool start_transfer(uint32_t words, bool audible)
{
    ring_words = words;
    consumed = 0;
    last_position = 0;
    lead_frames = 0;
    uint32_t blocks = words / BLOCK_WORDS;
    if (!dma_ring_items(items, blocks, ring_address(), BLOCK_WORDS, (uint32_t)(uintptr_t)items) ||
        !dmac_start_loop(&items[0])) {
        return false;
    }
    dac_path_start_transfer();
    transferring = true;
    dac_path_set_fade_service(serve_fade);
    if (audible) {
        dac_path_set_muted(false);
    }
    return true;
}

static void unmute_after(uint32_t settle_frames)
{
    uint32_t started_ms = hal_clock_milliseconds();
    while (consumed < settle_frames && hal_clock_milliseconds() - started_ms < SETTLE_TIMEOUT_MS) {
        update_consumed();
        pump(consumed);
    }
    set_muted(false);
}

static bool start_output(bool audible)
{
    uint32_t rate = audio_engine_rate(&engine);
    bool cold = !path_up || path_rate != rate;
    if (!ensure_path(rate)) {
        return false;
    }
    clear_ring();
    bool from_start = audio_engine_status(&engine).position_ms == 0;
    uint32_t lead = frames_in(cold ? COLD_LEAD_MS : WARM_LEAD_MS);
    audio_engine_lead_in(&engine, lead, frames_in(from_start ? START_RAMP_MS : RESUME_RAMP_MS));
    uint32_t started_ms = hal_clock_milliseconds();
    uint32_t prefill = rate / PREFILL_DIVISOR + lead;
    prefill = prefill < RING_FRAMES * 3 / 4 ? prefill : RING_FRAMES * 3 / 4;
    while (audio_engine_buffered(&engine, 0) < prefill &&
           hal_clock_milliseconds() - started_ms < PREFILL_TIMEOUT_MS) {
        pump(0);
    }
    stats = (HalAudioStats){0};
    output_started_us = hal_clock_microseconds();
    stopped_waiting = false;
    if (!start_transfer(RING_WORDS, false)) {
        return false;
    }
    lead_frames = lead;
    if (audible) {
        unmute_after(frames_in(cold ? COLD_SETTLE_MS : WARM_SETTLE_MS));
    }
    return true;
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

static uint32_t le32_at(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static uint32_t light_rate(HalFile *open)
{
    uint8_t bytes[LIGHT_SNIFF_BYTES] = {0};
    FlacInput input = {.read = read_file, .context = open, .size = open->size};
    if (open->size < LIGHT_SNIFF_BYTES || !read_file(open, 0, bytes, sizeof bytes)) {
        return 0;
    }
    if (tag_is(bytes, "fLaC")) {
        const uint8_t *rate = bytes + FLAC_RATE_OFFSET;
        return (uint32_t)rate[0] << 12 | (uint32_t)rate[1] << 4 | (uint32_t)rate[2] >> 4;
    }
    if (tag_is(bytes, "RIFF") && tag_is(bytes + 8, "WAVE") &&
        tag_is(bytes + WAV_FORMAT_OFFSET, "fmt ")) {
        return le32_at(bytes + WAV_RATE_OFFSET);
    }
    if (mp4_signature(bytes)) {
        Mp4Track track;
        return mp4_probe(input, &track) && track.codec == MP4_CODEC_ALAC ? track.sample_rate : 0;
    }
    return !ogg_signature(bytes) && mp3_signature(bytes) ? SLOW_CLOCK_MAX_RATE : 0;
}

static bool light_track(const char *path)
{
    HalFile open;
    if (path == NULL || !hal_file_open(&open, path)) {
        return false;
    }
    uint32_t rate = light_rate(&open);
    hal_file_close(&open);
    return rate > 0 && rate <= SLOW_CLOCK_MAX_RATE;
}

static bool equalizer_on(void)
{
    for (int band = 0; band < equalizer_curve.band_count; band++) {
        if (equalizer_curve.bands[band].gain_tenths != 0) {
            return true;
        }
    }
    return false;
}

static uint32_t clock_for(bool light)
{
    bool slow = light && slow_allowed && board_saving(HAL_SAVING_SLOW_CLOCK) && !equalizer_on();
    return slow ? SLOW_CLOCK_MHZ : AUDIO_CLOCK_MHZ;
}

static void choose_clock(const char *path)
{
    uint32_t mhz = clock_for(light_track(path));
    if (hal_system_clock_mhz() != mhz) {
        audio_engine_close_source(&engine);
        hal_system_set_clock_mhz(mhz);
    }
}

static void follow_queue(void)
{
    if (!engine_ready || queued_path[0] == '\0') {
        return;
    }
    bool restart = stream_service == NULL && (clock_for(queued_light) != hal_system_clock_mhz() ||
                                              program_of(source_kind) != queued_program);
    audio_engine_queue_next(&engine, restart ? NULL : queued_path, queued_token);
    deferred = restart;
}

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token)
{
    ensure_engine();
    resting = false;
    deferred = false;
    stop_tone();
    stop_transfer();
    choose_clock(path);
    core1_source_reset_equalizer(&core1_source);
    if (!audio_engine_play(&engine, path, start_ms, token)) {
        return false;
    }
    if (!start_output(true)) {
        audio_engine_stop(&engine);
        return false;
    }
    return true;
}

void hal_audio_queue_next(const char *path, uint32_t token)
{
    ensure_engine();
    deferred = false;
    queued_path[0] = '\0';
    audio_engine_queue_next(&engine, path, token);
    if (path == NULL) {
        queued_program = -1;
        return;
    }
    queued_light = board_saving(HAL_SAVING_SLOW_CLOCK) && light_track(path);
    queued_program = program_of(kind_of(path));
    queued_token = token;
    remember(queued_path, path);
    follow_queue();
}

void hal_audio_allow_slow_clock(bool allowed)
{
    if (allowed == slow_allowed) {
        return;
    }
    slow_allowed = allowed;
    if (deferred || (engine_ready && audio_engine_next_pending(&engine))) {
        follow_queue();
    }
}

static void start_deferred(void)
{
    if (!deferred || audio_engine_status(&engine).state != AUDIO_ENGINE_STOPPED ||
        audio_engine_status(&engine).failed) {
        return;
    }
    deferred = false;
    hal_audio_play(queued_path, 0, queued_token);
}

void hal_audio_pause(void)
{
    ensure_engine();
    if (audio_engine_status(&engine).state != AUDIO_ENGINE_PLAYING) {
        return;
    }
    if (transferring) {
        set_muted(true);
        dmac_suspend();
    }
    audio_engine_pause(&engine);
    paused_since_ms = hal_clock_milliseconds();
}

static void seek_to(uint32_t position_ms, bool paused, bool choose)
{
    ensure_engine();
    resting = false;
    stop_transfer();
    if (choose) {
        choose_clock(audio_engine_current_path(&engine));
    }
    core1_source_reset_equalizer(&core1_source);
    if (!audio_engine_seek(&engine, position_ms) || !start_output(!paused)) {
        audio_engine_stop(&engine);
        return;
    }
    if (paused) {
        audio_engine_pause(&engine);
        set_muted(true);
        dmac_suspend();
        paused_since_ms = hal_clock_milliseconds();
    }
}

void hal_audio_resume(void)
{
    ensure_engine();
    if (audio_engine_status(&engine).state != AUDIO_ENGINE_PAUSED) {
        return;
    }
    if (resting) {
        audio_engine_resume(&engine);
        seek_to(rest_position_ms, false, true);
        return;
    }
    if (transferring) {
        dmac_resume();
        set_muted(false);
    }
    audio_engine_resume(&engine);
}

void hal_audio_seek(uint32_t position_ms)
{
    ensure_engine();
    seek_to(position_ms, audio_engine_status(&engine).state == AUDIO_ENGINE_PAUSED, true);
}

static DacCurve output_curve(void)
{
    if (board_bluetooth_routed()) {
        return bluetooth_louder ? DAC_CURVE_BLUETOOTH_LOUD : DAC_CURVE_BLUETOOTH;
    }
    switch (output_setting) {
    case HAL_AUDIO_OUTPUT_SINGLE_ENDED:
        return DAC_CURVE_SINGLE_ENDED;
    case HAL_AUDIO_OUTPUT_BALANCED:
        return DAC_CURVE_BALANCED;
    case HAL_AUDIO_OUTPUT_AUTO:
        break;
    }
    return hal_audio_jack() == HAL_AUDIO_JACK_BALANCED ? DAC_CURVE_BALANCED
                                                       : DAC_CURVE_SINGLE_ENDED;
}

static uint8_t hardware_volume_value(void)
{
    if (stream_service != NULL) {
        return stream_hardware_value;
    }
    return dac_volume_value(volume_level, output_curve());
}

static void apply_volume(void)
{
    if (volume_set) {
        dac_path_set_volume(hardware_volume_value());
    }
}

void hal_audio_set_volume(uint8_t level)
{
    volume_level = level;
    volume_set = true;
    apply_volume();
}

void hal_audio_set_gain(HalAudioGain gain)
{
    dac_path_set_gain(gain == HAL_AUDIO_GAIN_HIGH);
}

void hal_audio_set_bluetooth_level(HalAudioBluetoothLevel level)
{
    bluetooth_louder = level == HAL_AUDIO_BLUETOOTH_LOUDER;
    apply_volume();
}

void hal_audio_set_filter(uint8_t filter)
{
    dac_path_set_filter(filter);
}

void hal_audio_set_balance(uint8_t balance)
{
    dac_path_set_balance(balance);
}

void hal_audio_set_output(HalAudioOutput output)
{
    output_setting = output;
    apply_volume();
}

void hal_audio_set_equalizer(const EqualizerCurve *curve)
{
    equalizer_curve = *curve;
    board_core1_set_equalizer(curve);
    if (!engine_ready) {
        return;
    }
    audio_engine_set_equalizer(&engine, &equalizer_curve);
    AudioEngineState state = audio_engine_status(&engine).state;
    if (transferring && state != AUDIO_ENGINE_STOPPED && equalizer_on() &&
        hal_system_clock_mhz() < AUDIO_CLOCK_MHZ) {
        seek_to(audio_engine_status(&engine).position_ms, state == AUDIO_ENGINE_PAUSED, true);
    }
    if (deferred || audio_engine_next_pending(&engine)) {
        follow_queue();
    }
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

static HalAudioCodec codec_of(AudioCodec codec)
{
    switch (codec) {
    case AUDIO_CODEC_WAV:
        return HAL_AUDIO_CODEC_WAV;
    case AUDIO_CODEC_MP3:
        return HAL_AUDIO_CODEC_MP3;
    case AUDIO_CODEC_ALAC:
        return HAL_AUDIO_CODEC_ALAC;
    case AUDIO_CODEC_AAC:
        return HAL_AUDIO_CODEC_AAC;
    case AUDIO_CODEC_OPUS:
        return HAL_AUDIO_CODEC_OPUS;
    case AUDIO_CODEC_FLAC:
        break;
    }
    return HAL_AUDIO_CODEC_FLAC;
}

HalAudioRefusal hal_audio_refusal(void)
{
    ensure_engine();
    AudioRefusal refusal = audio_engine_refusal(&engine);
    switch (refusal.problem) {
    case AUDIO_REFUSAL_RATE:
        return (HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_RATE,
                                 .sample_rate = refusal.sample_rate};
    case AUDIO_REFUSAL_UNREADABLE:
        return (HalAudioRefusal){.problem = unknown_format ? HAL_AUDIO_PROBLEM_FORMAT
                                                           : HAL_AUDIO_PROBLEM_UNREADABLE};
    case AUDIO_REFUSAL_NONE:
        break;
    }
    return (HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_NONE};
}

static HalAudioFormat format_of(AudioTrackInfo track)
{
    FlacInfo stream = track.stream;
    if (stream.sample_rate == 0) {
        return (HalAudioFormat){.codec = HAL_AUDIO_CODEC_UNKNOWN};
    }
    return (HalAudioFormat){
        .codec = codec_of(track.codec),
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
    start_deferred();
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
    if (!start_output(true)) {
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

static void rest_when_paused(void)
{
    if (!board_saving(HAL_SAVING_PAUSE_OFF) ||
        audio_engine_status(&engine).state != AUDIO_ENGINE_PAUSED ||
        hal_clock_milliseconds() - paused_since_ms < PAUSE_REST_MS) {
        return;
    }
    rest_position_ms = audio_engine_status(&engine).position_ms;
    audio_engine_close_source(&engine);
    stop_path();
    board_core1_stop();
    using_core1 = false;
    resting = true;
}

static void poll_jack(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    if (now_ms - last_jack_poll_ms < JACK_POLL_MS) {
        return;
    }
    last_jack_poll_ms = now_ms;
    if (board_jack_poll() && output_setting == HAL_AUDIO_OUTPUT_AUTO) {
        apply_volume();
    }
}

void hal_audio_service(void)
{
    poll_jack();
    if ((transferring || tone_active) && dac_path_take_underrun()) {
        stats.output_underruns++;
        total_underruns++;
    }
    if (stream_service != NULL) {
        stream_service();
        return;
    }
    if (!engine_ready || !transferring || tone_active) {
        return;
    }
    update_consumed();
    pump(consumed);
    start_deferred();
    change_rate();
    shut_down_when_idle();
    rest_when_paused();
}

HalAudioStats hal_audio_stats(void)
{
    HalAudioStats result = stats;
    if (!engine_ready || !transferring || tone_active) {
        return result;
    }
    result.underruns = audio_engine_stats(&engine).underruns;
    result.core1_load = using_core1 ? core1_region().control->load_percent : 0;
    result.elapsed_us = hal_clock_microseconds() - output_started_us;
    uint32_t rate = audio_engine_rate(&engine);
    if (rate > 0) {
        result.buffered_ms = (uint32_t)((uint64_t)audio_engine_buffered(&engine, consumed) *
                                        MILLISECONDS_PER_SECOND / rate);
    }
    return result;
}

HalToneResult hal_audio_dma_tone_start(uint32_t sample_rate)
{
    ensure_engine();
    stop_tone();
    stop_transfer();
    audio_engine_stop(&engine);
    if (!dmac_present()) {
        return HAL_TONE_NO_DMAC;
    }
    stop_path();
    HalToneResult result = dac_path_start(sample_rate);
    if (result != HAL_TONE_PLAYED) {
        return result;
    }
    path_up = true;
    path_rate = sample_rate;
    uint32_t *words = ring();
    int16_t samples[TONE_PERIOD_SAMPLES];
    tone_fill(samples, TONE_PERIOD_SAMPLES, TONE_AMPLITUDE);
    for (uint32_t index = 0; index < RING_WORDS; index++) {
        uint32_t frame = index / WORDS_PER_FRAME;
        words[index] = (uint32_t)(uint16_t)samples[frame % TONE_PERIOD_SAMPLES] << 16;
    }
    dac_path_output_on();
    tone_active = true;
    if (!start_transfer(RING_WORDS, true)) {
        tone_active = false;
        stop_path();
        return HAL_TONE_NO_DMAC;
    }
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
    seek_to(position_before_clock_change, paused_before_clock_change, false);
}

void board_audio_shut_down(void)
{
    resting = false;
    deferred = false;
    if (engine_ready) {
        audio_engine_stop(&engine);
    }
    stop_tone();
    stop_path();
    board_core1_stop();
    using_core1 = false;
}

uint32_t board_audio_underruns(void)
{
    return total_underruns;
}

uint8_t board_audio_volume_level(void)
{
    return volume_level;
}

uint8_t board_audio_stream_attenuation(void)
{
    uint8_t value = dac_volume_value(volume_level, output_curve());
    if (value == DAC_VOLUME_MUTED) {
        return DAC_VOLUME_MUTED;
    }
    return value > stream_hardware_value ? (uint8_t)(value - stream_hardware_value) : 0;
}

static void clear_ring(void)
{
    for (uint32_t index = 0; index < RING_WORDS; index++) {
        music_ring[index] = 0;
    }
}

bool board_audio_stream_open(uint32_t rate, void (*service)(void))
{
    ensure_engine();
    ensure_audio_clock();
    board_audio_shut_down();
    clear_ring();
    stream_hardware_value = dac_volume_value(HAL_AUDIO_VOLUME_MAX, output_curve());
    stream_service = service;
    dac_path_set_fades(false);
    apply_volume();
    return ensure_path(rate);
}

uint32_t *board_audio_stream_ring(uint32_t *frames)
{
    *frames = RING_FRAMES;
    return music_ring;
}

bool board_audio_stream_play(void)
{
    return start_transfer(RING_WORDS, true);
}

void board_audio_stream_pause(void)
{
    clear_ring();
    stop_transfer();
}

uint32_t board_audio_stream_consumed(void)
{
    update_consumed();
    return transferring ? consumed : 0;
}

void board_audio_stream_close(void)
{
    board_audio_shut_down();
    stream_service = NULL;
    dac_path_set_fades(true);
    apply_volume();
}
