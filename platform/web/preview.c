#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "app/app.h"
#include "app_internal.h"
#include "fake_hal.h"
#include "gfx/font_pack.h"
#include "hal/display.h"
#include "settings/settings.h"
#include "theme_store.h"
#include "ui/theme_package.h"

#define PREVIEW_EXPORT(name) __attribute__((export_name(#name), visibility("default")))

enum {
    FILE_SLOTS = 8,
    GLYPH_CACHE_WORDS = 13 * 1024,
    PIXELS = HAL_DISPLAY_LONG_SIDE * HAL_DISPLAY_LONG_SIDE,
    START_MS = 1000,
    SCRATCH_FILE_BYTES = 16 * 1024 * 1024,
    BATTERY_PERCENT = 80,
    BATTERY_MILLIVOLTS = 3900,
    SAMPLE_RATE = 44100,
    SAMPLE_BITS = 16,
    BITRATE_KBPS = 912,
    RED_SHIFT = 11,
    GREEN_SHIFT = 5,
    RED_MASK = 0x1F,
    GREEN_MASK = 0x3F,
    BLUE_MASK = 0x1F,
    THEME_BYTES = 160 * 1024
};

static const char THEME_NAME[] = "Preview";
static const char THEME_PATH[] = "/Themes/Preview.echotheme";

typedef struct PreviewFile {
    const char *path;
    const uint8_t *bytes;
    uint32_t size;
} PreviewFile;

static PreviewFile files[FILE_SLOTS];
static PreviewFile faces[FONT_PACK_FACES];
static uint32_t glyph_cache[GLYPH_CACHE_WORDS];
static int file_count;
static uint8_t settings_bytes[SETTINGS_FILE_SIZE];
static uint8_t theme_bytes[THEME_BYTES];
static uint32_t theme_size;
static uint32_t pixels[PIXELS];
static uint32_t now_ms = START_MS;
static uint32_t position_ms;
static int plays_seen;
static bool started;
static int chosen_volume = -1;
static int chosen_shuffle;
static int chosen_repeat;

PREVIEW_EXPORT(preview_alloc) void *preview_alloc(uint32_t size);
PREVIEW_EXPORT(preview_add_file)
void preview_add_file(const char *path, const uint8_t *bytes, uint32_t size);
PREVIEW_EXPORT(preview_add_face)
void preview_add_face(int face, const uint8_t *bytes, uint32_t size);
PREVIEW_EXPORT(preview_start) void preview_start(int orientation, int face, int accent);
PREVIEW_EXPORT(preview_step) void preview_step(uint32_t elapsed_ms);
PREVIEW_EXPORT(preview_key) void preview_key(int key, int pressed);
PREVIEW_EXPORT(preview_theme) int preview_theme(const uint8_t *bytes, uint32_t size);
PREVIEW_EXPORT(preview_width) int preview_width(void);
PREVIEW_EXPORT(preview_height) int preview_height(void);
PREVIEW_EXPORT(preview_pixels) const uint32_t *preview_pixels(void);
PREVIEW_EXPORT(preview_layout) int preview_layout(int16_t *out);
PREVIEW_EXPORT(preview_modes) void preview_modes(int volume, int shuffle, int repeat);
PREVIEW_EXPORT(preview_status) int preview_status(int32_t *out);
PREVIEW_EXPORT(preview_seek) void preview_seek(uint32_t position);

void *preview_alloc(uint32_t size)
{
    return malloc(size);
}

void preview_add_file(const char *path, const uint8_t *bytes, uint32_t size)
{
    if (file_count < FILE_SLOTS) {
        files[file_count++] = (PreviewFile){path, bytes, size};
    }
}

static bool read_face(void *context, uint32_t face, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    if (face >= FONT_PACK_FACES || offset > faces[face].size ||
        length > faces[face].size - offset) {
        return false;
    }
    memcpy(buffer, faces[face].bytes + offset, length);
    return true;
}

void preview_add_face(int face, const uint8_t *bytes, uint32_t size)
{
    if (face < 0 || face >= FONT_PACK_FACES) {
        return;
    }
    faces[face] = (PreviewFile){NULL, bytes, size};
    (void)font_pack_attach((FontPackReader){read_face, NULL}, glyph_cache, sizeof glyph_cache);
}

static void wear_theme(void)
{
    if (theme_size > 0) {
        (void)theme_store_wear(THEME_NAME);
    }
}

void preview_start(int orientation, int face, int accent)
{
    Settings settings = settings_defaults();
    settings.orientation = (Orientation)orientation;
    (void)face;
    settings.options[OPTION_ACCENT] = (uint8_t)accent;
    settings.screen_timeout = SCREEN_TIMEOUT_OPTION_COUNT - 1;
    settings.auto_off = 0;
    if (chosen_volume >= 0) {
        settings.volume = (uint8_t)chosen_volume;
    }
    settings.shuffle = (ShuffleMode)chosen_shuffle;
    settings.repeat = (RepeatMode)chosen_repeat;
    settings_encode(&settings, settings_bytes);
    fake_hal_reset();
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = BATTERY_PERCENT, .millivolts = BATTERY_MILLIVOLTS});
    for (int index = 0; index < file_count; index++) {
        fake_hal_add_file(files[index].path, files[index].bytes, files[index].size);
    }
    fake_hal_add_file("/.echo/settings.bin", settings_bytes, sizeof settings_bytes);
    fake_hal_add_file("/.echo/build.tmp", NULL, SCRATCH_FILE_BYTES);
    fake_hal_add_file(THEME_PATH, theme_bytes, sizeof theme_bytes);
    fake_hal_set_milliseconds(now_ms);
    position_ms = 0;
    plays_seen = 0;
    app_start();
    started = true;
    wear_theme();
}

static void describe_stream(void)
{
    fake_hal_audio_set_format((HalAudioFormat){.codec = HAL_AUDIO_CODEC_FLAC,
                                               .sample_rate = SAMPLE_RATE,
                                               .bits_per_sample = SAMPLE_BITS,
                                               .channels = 2,
                                               .bitrate_kbps = BITRATE_KBPS});
}

static void follow_playback(uint32_t elapsed_ms)
{
    if (fake_hal_audio_play_count() != plays_seen) {
        plays_seen = fake_hal_audio_play_count();
        position_ms = fake_hal_audio_play_start();
        describe_stream();
    }
    PlayerStatus status = app_player_status();
    if (status.state != PLAYER_PLAYING) {
        return;
    }
    position_ms += elapsed_ms;
    if (status.duration_ms > 0 && position_ms >= status.duration_ms) {
        position_ms = 0;
        fake_hal_audio_finish_track();
    }
    fake_hal_audio_set_position(position_ms);
}

void preview_step(uint32_t elapsed_ms)
{
    now_ms += elapsed_ms;
    fake_hal_set_milliseconds(now_ms);
    follow_playback(elapsed_ms);
    app_step();
}

void preview_key(int key, int pressed)
{
    fake_hal_queue_transition((Key)key, pressed != 0, now_ms);
}

int preview_theme(const uint8_t *bytes, uint32_t size)
{
    theme_size = size <= sizeof theme_bytes ? size : 0;
    memcpy(theme_bytes, bytes, theme_size);
    if (started) {
        return theme_size > 0 && theme_store_wear(THEME_NAME) ? 1 : 0;
    }
    ThemePackage checked = {0};
    uint32_t held = theme_size < THEME_PACKAGE_MAX_BYTES ? theme_size : THEME_PACKAGE_MAX_BYTES;
    return theme_package_decode(theme_bytes, held, &checked) ? 1 : 0;
}

static bool portrait(void)
{
    return fake_hal_display_rotation() != HAL_DISPLAY_LANDSCAPE;
}

int preview_width(void)
{
    return portrait() ? HAL_DISPLAY_SHORT_SIDE : HAL_DISPLAY_LONG_SIDE;
}

int preview_height(void)
{
    return portrait() ? HAL_DISPLAY_LONG_SIDE : HAL_DISPLAY_SHORT_SIDE;
}

static uint32_t widened(uint32_t value, uint32_t bits)
{
    return value << (8 - bits) | value >> (2 * bits - 8);
}

static uint32_t opaque(uint16_t color)
{
    uint32_t red = widened((uint32_t)color >> RED_SHIFT & RED_MASK, 5);
    uint32_t green = widened((uint32_t)color >> GREEN_SHIFT & GREEN_MASK, 6);
    uint32_t blue = widened((uint32_t)color & BLUE_MASK, 5);
    return 0xFF000000u | blue << 16 | green << 8 | red;
}

const uint32_t *preview_pixels(void)
{
    int width = preview_width();
    int height = preview_height();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            pixels[y * width + x] = opaque(fake_hal_pixel(x, y));
        }
    }
    return pixels;
}

int preview_layout(int16_t *out)
{
    ThemeLayout layout;
    app_now_playing_layout(&layout);
    for (int id = 0; id < THEME_LAYOUT_COUNT; id++) {
        ThemeLayoutBox box = layout.boxes[id];
        int16_t *place = out + 5 * id;
        place[0] = box.x;
        place[1] = box.y;
        place[2] = (int16_t)box.width;
        place[3] = box.height;
        place[4] = box.align;
    }
    return THEME_LAYOUT_COUNT;
}

void preview_modes(int volume, int shuffle, int repeat)
{
    chosen_volume = volume < 0 ? -1 : volume > SETTINGS_VOLUME_MAX ? SETTINGS_VOLUME_MAX : volume;
    chosen_shuffle = shuffle >= 0 && shuffle < SHUFFLE_MODE_COUNT ? shuffle : SHUFFLE_OFF;
    chosen_repeat = repeat >= 0 && repeat < REPEAT_MODE_COUNT ? repeat : REPEAT_OFF;
}

int preview_status(int32_t *out)
{
    PlayerStatus status = app_player_status();
    out[0] = (int32_t)status.state;
    out[1] = (int32_t)status.position_ms;
    out[2] = (int32_t)status.duration_ms;
    out[3] = (int32_t)status.volume;
    out[4] = (int32_t)status.shuffle;
    out[5] = (int32_t)status.repeat;
    return 6;
}

void preview_seek(uint32_t position)
{
    position_ms = position;
    fake_hal_audio_set_position(position_ms);
}
