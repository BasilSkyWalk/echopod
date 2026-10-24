#include "settings_store.h"

#include "hal/audio.h"
#include "hal/display.h"
#include "hal/storage.h"

const char SETTINGS_STORE_PATH[] = "/.echo/settings.bin";

static const HalAudioGain HAL_GAINS[GAIN_LEVEL_COUNT] = {HAL_AUDIO_GAIN_LOW, HAL_AUDIO_GAIN_HIGH};
static const HalAudioOutput HAL_OUTPUTS[OUTPUT_JACK_COUNT] = {
    HAL_AUDIO_OUTPUT_AUTO, HAL_AUDIO_OUTPUT_SINGLE_ENDED, HAL_AUDIO_OUTPUT_BALANCED};

static bool read_settings(uint8_t *bytes)
{
    HalFile file;
    if (!hal_file_open(&file, SETTINGS_STORE_PATH)) {
        return false;
    }
    bool read =
        file.size >= SETTINGS_FILE_SIZE && hal_file_read(file, 0, bytes, SETTINGS_FILE_SIZE);
    hal_file_close(&file);
    return read;
}

static bool same_bytes(const uint8_t *first, const uint8_t *second)
{
    for (int index = 0; index < SETTINGS_FILE_SIZE; index++) {
        if (first[index] != second[index]) {
            return false;
        }
    }
    return true;
}

static void remember(SettingsStore *store, const uint8_t *bytes)
{
    for (int index = 0; index < SETTINGS_FILE_SIZE; index++) {
        store->saved[index] = bytes[index];
    }
    store->has_saved = true;
}

void settings_store_load(SettingsStore *store)
{
    uint8_t bytes[SETTINGS_FILE_SIZE];
    store->has_saved = false;
    if (read_settings(bytes) && settings_decode(bytes, sizeof bytes, &store->settings)) {
        remember(store, bytes);
        return;
    }
    store->settings = settings_defaults();
}

bool settings_store_save(SettingsStore *store)
{
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&store->settings, bytes);
    if (store->has_saved && same_bytes(bytes, store->saved)) {
        return true;
    }
    if (!hal_file_write(SETTINGS_STORE_PATH, bytes, sizeof bytes)) {
        return false;
    }
    remember(store, bytes);
    return true;
}

void settings_store_capture(SettingsStore *store, PlayerStatus playback)
{
    store->settings.volume = playback.volume;
    store->settings.shuffle = playback.shuffle;
    store->settings.repeat = playback.repeat;
}

void settings_store_restore_playback(const SettingsStore *store, Player *player, uint32_t seed)
{
    const Settings *settings = &store->settings;
    player_change_volume(player, (int)settings->volume - (int)player_status(player).volume);
    player_set_shuffle(player, settings->shuffle, seed);
    player_set_repeat(player, settings->repeat);
}

void settings_store_apply_hardware(const Settings *settings)
{
    hal_display_set_brightness(settings->brightness);
    hal_audio_set_gain(HAL_GAINS[settings->gain]);
    hal_audio_set_output(HAL_OUTPUTS[settings->output]);
}
