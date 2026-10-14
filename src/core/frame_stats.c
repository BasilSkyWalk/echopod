#include "core/frame_stats.h"

enum {
    MICROSECONDS_PER_SECOND = 1000000
};

static uint32_t window_length(const FrameStats *stats)
{
    return stats->recorded < FRAME_STATS_WINDOW ? stats->recorded : FRAME_STATS_WINDOW;
}

void frame_stats_reset(FrameStats *stats)
{
    *stats = (FrameStats){0};
}

void frame_stats_record(FrameStats *stats, uint32_t duration_us)
{
    stats->durations_us[stats->next_slot] = duration_us;
    stats->next_slot = (stats->next_slot + 1) % FRAME_STATS_WINDOW;
    stats->recorded++;
}

uint32_t frame_stats_latest_us(const FrameStats *stats)
{
    if (stats->recorded == 0) {
        return 0;
    }
    uint32_t latest_slot = (stats->next_slot + FRAME_STATS_WINDOW - 1) % FRAME_STATS_WINDOW;
    return stats->durations_us[latest_slot];
}

uint32_t frame_stats_average_us(const FrameStats *stats)
{
    uint32_t length = window_length(stats);
    if (length == 0) {
        return 0;
    }
    uint64_t total_us = 0;
    for (uint32_t slot = 0; slot < length; slot++) {
        total_us += stats->durations_us[slot];
    }
    return (uint32_t)(total_us / length);
}

uint32_t frame_stats_worst_us(const FrameStats *stats)
{
    uint32_t worst_us = 0;
    for (uint32_t slot = 0; slot < window_length(stats); slot++) {
        if (stats->durations_us[slot] > worst_us) {
            worst_us = stats->durations_us[slot];
        }
    }
    return worst_us;
}

uint32_t frame_stats_achievable_fps(const FrameStats *stats)
{
    uint32_t average_us = frame_stats_average_us(stats);
    return average_us == 0 ? 0 : MICROSECONDS_PER_SECOND / average_us;
}
