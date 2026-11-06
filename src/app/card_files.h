#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct CardFilesReport {
    bool library_created;
    bool card_full;
} CardFilesReport;

CardFilesReport card_files_prepare(void (*creating)(void));
uint32_t card_files_covers_size(uint32_t albums);
void card_files_fit_covers(uint32_t albums);
