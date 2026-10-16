#include "draw_profile.h"

#include "core/frame_stats.h"

typedef struct DrawProfile {
    uint32_t frame_us[DRAW_PART_COUNT];
    FrameStats stats[DRAW_PART_COUNT];
} DrawProfile;

static DrawProfile profile;

void draw_profile_reset(void)
{
    for (int part = 0; part < DRAW_PART_COUNT; part++) {
        profile.frame_us[part] = 0;
        frame_stats_reset(&profile.stats[part]);
    }
}

void draw_profile_add(DrawPart part, uint32_t microseconds)
{
    profile.frame_us[part] += microseconds;
}

void draw_profile_finish_frame(void)
{
    for (int part = 0; part < DRAW_PART_COUNT; part++) {
        frame_stats_record(&profile.stats[part], profile.frame_us[part]);
        profile.frame_us[part] = 0;
    }
}

uint32_t draw_profile_average_us(DrawPart part)
{
    return frame_stats_average_us(&profile.stats[part]);
}
