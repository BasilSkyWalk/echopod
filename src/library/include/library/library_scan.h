#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"
#include "library/library_builder.h"

enum {
    SCAN_NAME_CAPACITY = 768,
    SCAN_PATH_CAPACITY = 1024,
    SCAN_MAX_DEPTH = 32
};

typedef struct ScanEntry {
    bool directory;
    char name[SCAN_NAME_CAPACITY];
} ScanEntry;

typedef struct ScanFileSystem {
    bool (*open_dir)(void *context, const char *path);
    bool (*next_entry)(void *context, ScanEntry *entry);
    void (*close_dir)(void *context);
    bool (*open_file)(void *context, const char *path, ByteSource *source, uint32_t *size);
    void (*close_file)(void *context);
    void *context;
} ScanFileSystem;

typedef struct ScanProgress {
    void (*report)(void *context, uint32_t files, uint32_t tracks);
    void *context;
} ScanProgress;

typedef enum ScanResult {
    SCAN_OK,
    SCAN_NO_MUSIC_ROOT,
    SCAN_OUT_OF_MEMORY,
    SCAN_TOO_MANY,
    SCAN_FOLDER_TOO_BIG,
    SCAN_PATH_TOO_LONG,
    SCAN_WRITE_FAILED
} ScanResult;

typedef struct ScanStats {
    uint32_t files;
    uint32_t tracks;
    uint32_t skipped;
    uint32_t length;
} ScanStats;

typedef struct ScanJob {
    ScanFileSystem files;
    void *memory;
    uint32_t size;
    ByteSink sink;
    ScanProgress progress;
} ScanJob;

ScanResult library_scan(const ScanJob *job, ScanStats *stats);
