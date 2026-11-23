#include "core/draw_profile.h"

#include <stddef.h>

typedef struct DrawProfile {
    uint32_t frame_us[DRAW_PART_COUNT];
    uint32_t sum_us[DRAW_PART_COUNT];
    uint32_t average_us[DRAW_PART_COUNT];
    uint32_t frames;
    uint32_t (*clock)(void);
} DrawProfile;

static DrawProfile profile;

void draw_profile_reset(void)
{
    uint32_t (*clock)(void) = profile.clock;
    profile = (DrawProfile){.clock = clock};
}

void draw_profile_use_clock(uint32_t (*clock)(void))
{
    profile.clock = clock;
}

uint32_t draw_profile_now(void)
{
    return profile.clock != NULL ? profile.clock() : 0;
}

void draw_profile_since(DrawPart part, uint32_t started_us)
{
    profile.frame_us[part] += draw_profile_now() - started_us;
}

void draw_profile_add(DrawPart part, uint32_t microseconds)
{
    profile.frame_us[part] += microseconds;
}

void draw_profile_finish_frame(void)
{
    profile.frames++;
    for (int part = 0; part < DRAW_PART_COUNT; part++) {
        profile.sum_us[part] += profile.frame_us[part];
        profile.frame_us[part] = 0;
        if (profile.frames == DRAW_PROFILE_WINDOW) {
            profile.average_us[part] = profile.sum_us[part] / DRAW_PROFILE_WINDOW;
            profile.sum_us[part] = 0;
        }
    }
    profile.frames %= DRAW_PROFILE_WINDOW;
}

uint32_t draw_profile_average_us(DrawPart part)
{
    return profile.frames > 0 ? profile.sum_us[part] / profile.frames : profile.average_us[part];
}
