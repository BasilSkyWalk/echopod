#include "device/stall_watch.h"

void stall_watch_init(StallWatch *watch, uint32_t now_ms)
{
    *watch = (StallWatch){.beat_ms = now_ms, .paused = false, .tripped = false};
}

void stall_watch_beat(StallWatch *watch, uint32_t now_ms)
{
    watch->beat_ms = now_ms;
    watch->tripped = false;
}

void stall_watch_pause(StallWatch *watch, bool paused, uint32_t now_ms)
{
    watch->paused = paused;
    stall_watch_beat(watch, now_ms);
}

bool stall_watch_check(StallWatch *watch, uint32_t now_ms)
{
    if (watch->paused || watch->tripped || now_ms - watch->beat_ms < STALL_WATCH_LIMIT_MS) {
        return false;
    }
    watch->tripped = true;
    return true;
}
