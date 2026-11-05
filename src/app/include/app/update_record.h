#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/system.h"

enum {
    UPDATE_RECORD_BYTES = 16
};

extern const char UPDATE_RECORD_PATH[];

void update_record_encode(HalUpdateRecord record, uint8_t *bytes);
bool update_record_decode(const uint8_t *bytes, HalUpdateRecord *record);
bool update_record_read(HalUpdateRecord *record);
bool update_record_save(HalUpdateRecord record);
