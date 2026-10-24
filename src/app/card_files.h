#pragma once

#include <stdbool.h>

typedef struct CardFilesReport {
    bool library_created;
    bool card_full;
} CardFilesReport;

CardFilesReport card_files_prepare(void);
