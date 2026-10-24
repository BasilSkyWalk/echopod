#pragma once

#include <stdint.h>

#include "library/cover_build.h"
#include "library/library_scan.h"

typedef struct RescanOutcome {
    ScanResult result;
    ScanStats stats;
    ScanResult cover_result;
    CoverStats covers;
} RescanOutcome;

typedef struct RescanProgress {
    void (*tracks)(uint32_t files, uint32_t tracks);
    void (*covers)(uint32_t done, uint32_t total);
} RescanProgress;

extern const char LIBRARY_SCRATCH_PATH[];

RescanOutcome library_rescan_run(RescanProgress progress);
const char *library_rescan_message(RescanOutcome outcome, char *text, uint32_t capacity);
