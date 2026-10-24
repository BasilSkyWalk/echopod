#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    STALL_WATCH_LIMIT_MS = 10000
};

typedef struct StallWatch {
    uint32_t beat_ms;
    bool paused;
    bool tripped;
} StallWatch;

void stall_watch_init(StallWatch *watch, uint32_t now_ms);
void stall_watch_beat(StallWatch *watch, uint32_t now_ms);
void stall_watch_pause(StallWatch *watch, bool paused, uint32_t now_ms);
bool stall_watch_check(StallWatch *watch, uint32_t now_ms);
