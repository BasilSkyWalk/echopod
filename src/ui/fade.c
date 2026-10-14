#include "ui/fade.h"

void fade_start(Fade *fade, uint32_t now_ms, FadeTiming timing)
{
    *fade = (Fade){.active = true, .start_ms = now_ms, .timing = timing};
}

static uint8_t ramp(uint32_t elapsed_ms, uint32_t fade_ms)
{
    if (fade_ms == 0 || elapsed_ms >= fade_ms) {
        return FADE_OPAQUE;
    }
    return (uint8_t)(elapsed_ms * FADE_OPAQUE / fade_ms);
}

uint8_t fade_alpha(const Fade *fade, uint32_t now_ms)
{
    uint32_t elapsed = now_ms - fade->start_ms;
    if (fade->timing.rising) {
        return ramp(elapsed, fade->timing.fade_ms);
    }
    if (elapsed < fade->timing.hold_ms) {
        return FADE_OPAQUE;
    }
    return (uint8_t)(FADE_OPAQUE - ramp(elapsed - fade->timing.hold_ms, fade->timing.fade_ms));
}

bool fade_finished(const Fade *fade, uint32_t now_ms)
{
    uint32_t total = (uint32_t)fade->timing.fade_ms + fade->timing.hold_ms;
    return !fade->active || now_ms - fade->start_ms >= total;
}
