#include "equalizer_store.h"

#include <stddef.h>

#include "hal/audio.h"
#include "hal/storage.h"

enum {
    PATH_CAPACITY = 72,
    MISSING_NONE = 0
};

const char EQUALIZER_STORE_PATH[] = "/.echo/equalizer.bin";
const char EQUALIZER_FOLDER_PATH[] = "/EQ";

static bool name_valid(const char *name)
{
    if (name[0] == '\0') {
        return false;
    }
    uint32_t length = 0;
    while (name[length] != '\0') {
        if (name[length] == '/' || length + 1 >= EQUALIZER_PRESET_NAME_CAPACITY) {
            return false;
        }
        length++;
    }
    return true;
}

static bool make_path(char *path, const char *name)
{
    if (!name_valid(name)) {
        return false;
    }
    uint32_t length = 0;
    while (EQUALIZER_FOLDER_PATH[length] != '\0') {
        path[length] = EQUALIZER_FOLDER_PATH[length];
        length++;
    }
    path[length++] = '/';
    uint32_t index = 0;
    while (name[index] != '\0' && length + 1 < PATH_CAPACITY) {
        path[length++] = name[index++];
    }
    path[length] = '\0';
    return name[index] == '\0';
}

static void copy_name(char *destination, const char *source)
{
    uint32_t index = 0;
    while (source[index] != '\0' && index + 1 < EQUALIZER_PRESET_NAME_CAPACITY) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static bool read_choice(uint8_t *bytes, uint32_t *length)
{
    HalFile file;
    if (!hal_file_open(&file, EQUALIZER_STORE_PATH)) {
        return false;
    }
    *length = file.size < EQUALIZER_FILE_SIZE ? file.size : EQUALIZER_FILE_SIZE;
    bool read = *length >= 20 && hal_file_read(file, 0, bytes, *length);
    hal_file_close(&file);
    return read;
}

static void remember(EqualizerStore *store, const uint8_t *bytes)
{
    for (int index = 0; index < EQUALIZER_FILE_SIZE; index++) {
        store->saved[index] = bytes[index];
    }
    store->has_saved = true;
}

static bool unchanged(const EqualizerStore *store, const uint8_t *bytes)
{
    for (int index = 0; index < EQUALIZER_FILE_SIZE; index++) {
        if (store->saved[index] != bytes[index]) {
            return false;
        }
    }
    return store->has_saved;
}

static bool read_preset(void *context, uint32_t offset, char *buffer, uint32_t length)
{
    const HalFile *file = context;
    return hal_file_read(*file, offset, buffer, length);
}

bool equalizer_store_file_curve(const char *name, EqualizerCurve *curve, EqualizerTextError *error)
{
    EqualizerTextError local = {MISSING_NONE, EQUALIZER_TEXT_UNREADABLE, 0};
    if (error == NULL) {
        error = &local;
    }
#ifdef ECHO_EQ_FILES_HIDDEN
    (void)name;
    (void)curve;
    error->problem = EQUALIZER_TEXT_UNREADABLE;
    return false;
#endif
    char path[PATH_CAPACITY];
    if (!make_path(path, name)) {
        error->problem = EQUALIZER_TEXT_UNREADABLE;
        return false;
    }
    HalFile file;
    if (!hal_file_open(&file, path)) {
        error->problem = EQUALIZER_TEXT_UNREADABLE;
        return false;
    }
    if (file.size > EQUALIZER_TEXT_MAX_BYTES) {
        hal_file_close(&file);
        error->problem = EQUALIZER_TEXT_TOO_LARGE;
        return false;
    }
    if (file.size == 0) {
        hal_file_close(&file);
        error->problem = EQUALIZER_TEXT_EMPTY;
        return false;
    }
    EqualizerTextSource source = {read_preset, &file};
    bool parsed = equalizer_text_parse_from(source, file.size, curve, error);
    hal_file_close(&file);
    return parsed;
}

void equalizer_store_load(EqualizerStore *store)
{
    uint8_t bytes[EQUALIZER_FILE_SIZE] = {0};
    uint32_t length = 0;
    if (!read_choice(bytes, &length) || !equalizer_file_decode(bytes, length, &store->choice)) {
        store->choice = equalizer_file_defaults();
        equalizer_file_encode(&store->choice, bytes);
        length = EQUALIZER_FILE_SIZE;
    }
    for (uint32_t index = length; index < EQUALIZER_FILE_SIZE; index++) {
        bytes[index] = 0;
    }
    remember(store, bytes);
}

bool equalizer_store_save(EqualizerStore *store)
{
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    equalizer_file_encode(&store->choice, bytes);
    if (unchanged(store, bytes)) {
        return true;
    }
    if (!hal_file_write(EQUALIZER_STORE_PATH, bytes, sizeof bytes)) {
        return false;
    }
    remember(store, bytes);
    return true;
}

bool equalizer_store_apply(EqualizerStore *store)
{
    if (store->choice.preset != EQUALIZER_PRESET_FILE) {
        EqualizerCurve curve = equalizer_file_curve(&store->choice);
        hal_audio_set_equalizer(&curve);
        return true;
    }
    EqualizerCurve curve;
    EqualizerTextError error;
    if (!equalizer_store_file_curve(store->choice.preset_name, &curve, &error)) {
        store->choice.preset = EQUALIZER_PRESET_OFF;
        store->choice.preset_name[0] = '\0';
        EqualizerCurve flat = {0};
        hal_audio_set_equalizer(&flat);
        return false;
    }
    hal_audio_set_equalizer(&curve);
    return true;
}

bool equalizer_store_select_file(EqualizerStore *store, const char *name, EqualizerTextError *error)
{
    EqualizerCurve curve;
    if (!equalizer_store_file_curve(name, &curve, error)) {
        return false;
    }
    store->choice.preset = EQUALIZER_PRESET_FILE;
    copy_name(store->choice.preset_name, name);
    hal_audio_set_equalizer(&curve);
    return true;
}
