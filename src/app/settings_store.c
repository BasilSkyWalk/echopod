#include "settings_store.h"

#include "hal/audio.h"
#include "hal/display.h"
#include "hal/storage.h"
#include "hal/system.h"

const char SETTINGS_STORE_PATH[] = "/.echo/settings.bin";

static const HalAudioGain HAL_GAINS[GAIN_LEVEL_COUNT] = {HAL_AUDIO_GAIN_LOW, HAL_AUDIO_GAIN_HIGH};
static const HalAudioOutput HAL_OUTPUTS[OUTPUT_JACK_COUNT] = {
    HAL_AUDIO_OUTPUT_AUTO, HAL_AUDIO_OUTPUT_SINGLE_ENDED, HAL_AUDIO_OUTPUT_BALANCED};
static const HalUsbMode HAL_USB_MODES[USB_MODE_COUNT] = {HAL_USB_MODE_STORAGE, HAL_USB_MODE_DAC};

static uint32_t read_settings(uint8_t *bytes)
{
    HalFile file;
    if (!hal_file_open(&file, SETTINGS_STORE_PATH)) {
        return 0;
    }
    uint32_t length = file.size < SETTINGS_FILE_SIZE ? file.size : SETTINGS_FILE_SIZE;
    if (!hal_file_read(file, 0, bytes, length)) {
        length = 0;
    }
    hal_file_close(&file);
    return length;
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
    uint8_t bytes[SETTINGS_FILE_SIZE] = {0};
    store->has_saved = false;
    if (settings_decode(bytes, read_settings(bytes), &store->settings)) {
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

HalAudioSpeed settings_store_spoken_speed(const Settings *settings)
{
    return (HalAudioSpeed){.quarters = settings_spoken_quarters(settings->options),
                           .keep_pitch = settings_spoken_keep_pitch(settings->options)};
}

void settings_store_restore_playback(const SettingsStore *store, Player *player, uint32_t seed)
{
    const Settings *settings = &store->settings;
    player_change_volume(player, (int)settings->volume - (int)player_status(player).volume);
    player_set_shuffle(player, settings->shuffle, seed);
    player_set_repeat(player, settings->repeat);
    player_set_spoken_speed(player, settings_store_spoken_speed(settings));
}

void settings_store_apply_hardware(const Settings *settings)
{
    hal_display_set_brightness(settings->brightness);
    hal_audio_set_gain(HAL_GAINS[settings->gain]);
    hal_audio_set_filter(settings->filter);
    hal_audio_set_balance(settings->balance);
    hal_audio_set_output(HAL_OUTPUTS[settings->output]);
    hal_audio_set_bluetooth_level(settings->options[OPTION_BLUETOOTH_LEVEL] ==
                                          BLUETOOTH_LEVEL_LOUDER
                                      ? HAL_AUDIO_BLUETOOTH_LOUDER
                                      : HAL_AUDIO_BLUETOOTH_STANDARD);
    hal_system_set_usb_mode(HAL_USB_MODES[settings->usb_mode]);
    hal_system_set_dac_volume(settings->dac_volume);
    hal_system_set_usb_prompt(settings->options[OPTION_USB_PROMPT] == USB_PROMPT_ON);
}
