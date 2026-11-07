#pragma once

#include <stdint.h>

#include "listen_history.h"

extern const char SCROBBLER_LOG_PATH[];
extern const char SCROBBLER_MARK_PATH[];

enum {
    SCROBBLER_MARK_BYTES = 11
};

uint32_t scrobbler_log_write(ListenHistory *history, const char *version);
