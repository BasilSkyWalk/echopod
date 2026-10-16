#pragma once

#include <stdbool.h>

#include "codec/flac.h"
#include "device/audio_engine.h"

typedef struct FlacFileOpener {
    bool (*open)(void *context, const char *path, FlacInput *input);
    void (*close)(void *context);
    void *context;
} FlacFileOpener;

typedef struct FlacSource {
    FlacFileOpener files;
    FlacDecoder *decoder;
    bool open;
} FlacSource;

void flac_source_init(FlacSource *source, FlacFileOpener files, FlacDecoder *decoder);
bool flac_source_open_at(FlacSource *source, const char *path, uint32_t start_ms, FlacInfo *info);
void flac_source_close(FlacSource *source);
AudioSource flac_source_interface(FlacSource *source);
