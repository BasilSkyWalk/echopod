#include "equalizer_store.h"

#include "hal/audio.h"
#include "hal/storage.h"

const char EQUALIZER_STORE_PATH[] = "/.echo/equalizer.bin";

static bool read_choice(uint8_t *bytes)
{
    HalFile file;
    if (!hal_file_open(&file, EQUALIZER_STORE_PATH)) {
        return false;
    }
    bool read =
        file.size >= EQUALIZER_FILE_SIZE && hal_file_read(file, 0, bytes, EQUALIZER_FILE_SIZE);
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

void equalizer_store_load(EqualizerStore *store)
{
    uint8_t bytes[EQUALIZER_FILE_SIZE];
    if (!read_choice(bytes) || !equalizer_file_decode(bytes, sizeof bytes, &store->choice)) {
        store->choice = equalizer_file_defaults();
        equalizer_file_encode(&store->choice, bytes);
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

void equalizer_store_apply(const EqualizerStore *store)
{
    EqualizerCurve curve = equalizer_file_curve(&store->choice);
    hal_audio_set_equalizer(&curve);
}
