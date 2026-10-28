#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"
#include "library/library_builder.h"
#include "library/library_scan.h"

enum {
    COVER_BUILD_RECORD_BYTES = (112 * 112 + 56 * 56) * 2,
    COVER_BUILD_MAX_COVERS = 0xFFFF
};

typedef struct CoverProgress {
    void (*report)(void *context, uint32_t done, uint32_t total);
    void *context;
} CoverProgress;

typedef struct CoverJob {
    ScanFileSystem files;
    ByteSource library;
    uint32_t library_size;
    ByteSink sink;
    ByteSource previous;
    uint32_t capacity;
    void *memory;
    uint32_t size;
    CoverProgress progress;
} CoverJob;

typedef struct CoverStats {
    uint32_t albums;
    uint32_t covers;
    uint32_t reused;
    uint32_t missing;
} CoverStats;

ScanResult cover_build(const CoverJob *job, CoverStats *stats);
