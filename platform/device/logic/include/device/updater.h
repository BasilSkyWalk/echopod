#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    UPDATER_IMAGE_SIZE = 0x500004,
    UPDATER_STOCK_IMAGE_SIZE = 0x2000004,
    UPDATER_CHUNK_BYTES = 8192,
    UPDATER_SCRATCH_WORDS = (2 * UPDATER_CHUNK_BYTES + 0x800) / 4
};

typedef enum UpdateRefusal {
    UPDATE_REFUSED_NONE,
    UPDATE_REFUSED_SIZE,
    UPDATE_REFUSED_HEADER,
    UPDATE_REFUSED_HEADER_COPY,
    UPDATE_REFUSED_CRC,
    UPDATE_REFUSED_MODEL,
    UPDATE_REFUSED_ROOM,
    UPDATE_REFUSED_LAYOUT
} UpdateRefusal;

typedef enum UpdateCheck {
    UPDATE_VALID,
    UPDATE_ALREADY_INSTALLED,
    UPDATE_INVALID,
    UPDATE_STOCK
} UpdateCheck;

typedef struct UpdateFile {
    bool (*read)(void *context, uint32_t offset, void *buffer, uint32_t length);
    uint32_t size;
    void *context;
} UpdateFile;

typedef struct UpdateStorage {
    bool (*read)(void *context, uint32_t lba, uint32_t count, void *buffer);
    bool (*write)(void *context, uint32_t lba, uint32_t count, const void *buffer);
    void *context;
    uint32_t slot_1;
    uint32_t slot_2;
    uint32_t slot_sectors;
} UpdateStorage;

typedef struct UpdateProgress {
    void (*report)(void *context, uint32_t done, uint32_t total);
    void *context;
} UpdateProgress;

UpdateCheck updater_check(const UpdateFile *file, const UpdateStorage *storage);
UpdateRefusal updater_refusal(void);
void updater_use_scratch(uint32_t *words);
bool updater_install(const UpdateFile *file, const UpdateStorage *storage, UpdateProgress progress);
