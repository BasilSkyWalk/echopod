#include "fake_hal.h"

#include "hal/system.h"

#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "hal/power.h"
#include "hal/storage.h"

#include <string.h>

enum {
    TRANSITION_CAPACITY = 16,
    FILE_CAPACITY = 8,
    WRITTEN_CAPACITY = 1100,
    PATH_CAPACITY = 260
};

typedef struct FakeAudio {
    HalAudioStatus status;
    char playing_path[PATH_CAPACITY];
    uint32_t play_start_ms;
    int play_count;
    char queued_path[PATH_CAPACITY];
    bool has_queued;
    uint32_t queued_token;
    int seek_count;
    uint32_t last_seek_ms;
    uint8_t volume;
    HalAudioGain gain;
    HalAudioOutput output;
} FakeAudio;

typedef struct WrittenFile {
    char path[PATH_CAPACITY];
    uint8_t bytes[WRITTEN_CAPACITY];
} WrittenFile;

typedef struct FakeFile {
    const char *path;
    const uint8_t *bytes;
    uint32_t size;
} FakeFile;

typedef struct FakeHal {
    const char *version;
    uint16_t panel[HAL_DISPLAY_HEIGHT][HAL_DISPLAY_WIDTH];
    long pixels_written;
    int present_count;
    bool display_on;
    uint8_t brightness;
    HalBattery battery;
    int power_off_count;
    uint32_t now_ms;
    uint32_t now_us;
    uint32_t microseconds_per_read;
    HalKeyTransition transitions[TRANSITION_CAPACITY];
    int transition_count;
    int next_transition;
    FakeFile files[FILE_CAPACITY];
    int file_count;
    FakeAudio audio;
    WrittenFile written[FILE_CAPACITY];
    int write_count;
    uint32_t clock_mhz;
    bool refuse_clock_switch;
} FakeHal;

static FakeHal fake;

void fake_hal_reset(void)
{
    fake = (FakeHal){0};
}

void fake_hal_set_version(const char *version)
{
    fake.version = version;
}

const char *hal_system_version(void)
{
    return fake.version != NULL ? fake.version : "test";
}

void fake_hal_refuse_clock_switch(void)
{
    fake.refuse_clock_switch = true;
}

uint32_t hal_system_clock_mhz(void)
{
    return fake.clock_mhz != 0 ? fake.clock_mhz : 24;
}

bool hal_system_set_clock_mhz(uint32_t mhz)
{
    if (fake.refuse_clock_switch) {
        return false;
    }
    fake.clock_mhz = mhz;
    return true;
}

void fake_hal_add_file(const char *path, const uint8_t *bytes, uint32_t size)
{
    fake.files[fake.file_count++] = (FakeFile){.path = path, .bytes = bytes, .size = size};
}

void fake_hal_set_milliseconds(uint32_t now_ms)
{
    fake.now_ms = now_ms;
}

void fake_hal_set_microseconds_per_read(uint32_t step_us)
{
    fake.microseconds_per_read = step_us;
}

void fake_hal_queue_transition(Key key, bool pressed, uint32_t time_ms)
{
    if (fake.transition_count == TRANSITION_CAPACITY) {
        return;
    }
    fake.transitions[fake.transition_count++] =
        (HalKeyTransition){.key = key, .pressed = pressed, .time_ms = time_ms};
}

uint16_t fake_hal_pixel(int x, int y)
{
    return fake.panel[y][x];
}

long fake_hal_pixels_written(void)
{
    return fake.pixels_written;
}

int fake_hal_present_count(void)
{
    return fake.present_count;
}

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels)
{
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            fake.panel[y + row][x + column] = pixels[row * width + column];
        }
    }
    fake.pixels_written += (long)width * height;
}

void hal_display_present(void)
{
    fake.present_count++;
}

void hal_display_set_power(bool on)
{
    fake.display_on = on;
}

void hal_display_set_brightness(uint8_t level)
{
    fake.brightness = level;
}

bool fake_hal_display_on(void)
{
    return fake.display_on;
}

uint8_t fake_hal_display_brightness(void)
{
    return fake.brightness;
}

void fake_hal_set_battery(HalBattery battery)
{
    fake.battery = battery;
}

HalBattery hal_power_battery(void)
{
    return fake.battery;
}

void hal_power_off(void)
{
    fake.power_off_count++;
}

int fake_hal_power_off_count(void)
{
    return fake.power_off_count;
}

bool hal_input_next_transition(HalKeyTransition *transition)
{
    if (fake.next_transition == fake.transition_count) {
        fake.next_transition = 0;
        fake.transition_count = 0;
        return false;
    }
    *transition = fake.transitions[fake.next_transition++];
    return true;
}

uint32_t hal_clock_milliseconds(void)
{
    return fake.now_ms;
}

uint32_t fake_hal_microseconds(void)
{
    return fake.now_us;
}

uint32_t hal_clock_microseconds(void)
{
    fake.now_us += fake.microseconds_per_read;
    return fake.now_us;
}

bool hal_file_open(HalFile *file, const char *path)
{
    for (int index = 0; index < fake.file_count; index++) {
        if (strcmp(fake.files[index].path, path) == 0) {
            *file = (HalFile){.handle = index, .size = fake.files[index].size};
            return true;
        }
    }
    *file = (HalFile){.handle = -1, .size = 0};
    return false;
}

bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length)
{
    if (file.handle < 0 || file.handle >= fake.file_count) {
        return false;
    }
    const FakeFile *source = &fake.files[file.handle];
    if (offset > source->size || length > source->size - offset) {
        return false;
    }
    memcpy(buffer, source->bytes + offset, length);
    return true;
}

void hal_file_close(HalFile *file)
{
    file->handle = -1;
}

static void copy_path(char *target, const char *source)
{
    strncpy(target, source, PATH_CAPACITY - 1);
    target[PATH_CAPACITY - 1] = '\0';
}

const char *fake_hal_audio_playing_path(void)
{
    return fake.audio.playing_path;
}

uint32_t fake_hal_audio_play_start(void)
{
    return fake.audio.play_start_ms;
}

int fake_hal_audio_play_count(void)
{
    return fake.audio.play_count;
}

const char *fake_hal_audio_queued_path(void)
{
    return fake.audio.has_queued ? fake.audio.queued_path : NULL;
}

void fake_hal_audio_finish_track(void)
{
    if (!fake.audio.has_queued) {
        fake.audio.status.state = HAL_AUDIO_STOPPED;
        return;
    }
    fake.audio.status.token = fake.audio.queued_token;
    fake.audio.status.position_ms = 0;
    copy_path(fake.audio.playing_path, fake.audio.queued_path);
    fake.audio.has_queued = false;
}

void fake_hal_audio_fail_track(void)
{
    fake.audio.status.state = HAL_AUDIO_STOPPED;
    fake.audio.status.failed = true;
}

void fake_hal_audio_set_position(uint32_t position_ms)
{
    fake.audio.status.position_ms = position_ms;
}

int fake_hal_audio_seek_count(void)
{
    return fake.audio.seek_count;
}

uint32_t fake_hal_audio_last_seek(void)
{
    return fake.audio.last_seek_ms;
}

uint8_t fake_hal_audio_volume(void)
{
    return fake.audio.volume;
}

int fake_hal_audio_state(void)
{
    return (int)fake.audio.status.state;
}

int fake_hal_write_count(void)
{
    return fake.write_count;
}

bool hal_audio_play(const char *path, uint32_t start_ms, uint32_t token)
{
    copy_path(fake.audio.playing_path, path);
    fake.audio.play_start_ms = start_ms;
    fake.audio.play_count++;
    fake.audio.status = (HalAudioStatus){
        .state = HAL_AUDIO_PLAYING, .token = token, .position_ms = start_ms, .failed = false};
    return true;
}

void hal_audio_queue_next(const char *path, uint32_t token)
{
    fake.audio.has_queued = path != NULL;
    if (path != NULL) {
        copy_path(fake.audio.queued_path, path);
        fake.audio.queued_token = token;
    }
}

void hal_audio_pause(void)
{
    fake.audio.status.state = HAL_AUDIO_PAUSED;
}

void hal_audio_resume(void)
{
    fake.audio.status.state = HAL_AUDIO_PLAYING;
}

void hal_audio_seek(uint32_t position_ms)
{
    fake.audio.seek_count++;
    fake.audio.last_seek_ms = position_ms;
    fake.audio.status.position_ms = position_ms;
}

void hal_audio_set_volume(uint8_t level)
{
    fake.audio.volume = level;
}

void hal_audio_set_gain(HalAudioGain gain)
{
    fake.audio.gain = gain;
}

void hal_audio_set_output(HalAudioOutput output)
{
    fake.audio.output = output;
}

int fake_hal_audio_gain(void)
{
    return (int)fake.audio.gain;
}

int fake_hal_audio_output(void)
{
    return (int)fake.audio.output;
}

HalAudioStatus hal_audio_status(void)
{
    return fake.audio.status;
}

static WrittenFile *written_slot(const char *path)
{
    for (int index = 0; index < FILE_CAPACITY; index++) {
        if (strcmp(fake.written[index].path, path) == 0 || fake.written[index].path[0] == '\0') {
            return &fake.written[index];
        }
    }
    return NULL;
}

static void register_written(const char *path, uint32_t length)
{
    WrittenFile *slot = written_slot(path);
    for (int index = 0; index < fake.file_count; index++) {
        if (strcmp(fake.files[index].path, path) == 0) {
            fake.files[index].size = length;
            return;
        }
    }
    fake_hal_add_file(slot->path, slot->bytes, length);
}

bool hal_file_write(const char *path, const void *bytes, uint32_t length)
{
    WrittenFile *slot = written_slot(path);
    if (slot == NULL || length > WRITTEN_CAPACITY) {
        return false;
    }
    copy_path(slot->path, path);
    memcpy(slot->bytes, bytes, length);
    register_written(path, length);
    fake.write_count++;
    return true;
}

const uint8_t *fake_hal_file_bytes(const char *path, uint32_t *size)
{
    for (int index = 0; index < fake.file_count; index++) {
        if (strcmp(fake.files[index].path, path) == 0) {
            *size = fake.files[index].size;
            return fake.files[index].bytes;
        }
    }
    *size = 0;
    return NULL;
}
