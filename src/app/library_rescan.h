#pragma once

#include <stdint.h>

#include "library/library_scan.h"

typedef struct RescanOutcome {
    ScanResult result;
    ScanStats stats;
} RescanOutcome;

typedef void (*RescanProgress)(uint32_t files, uint32_t tracks);

RescanOutcome library_rescan_run(RescanProgress progress);
const char *library_rescan_message(RescanOutcome outcome, char *text, uint32_t capacity);
