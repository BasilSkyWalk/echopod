#pragma once

#include <stdint.h>

enum {
    FRAME_STATS_WINDOW = 32
};

typedef struct FrameStats {
    uint32_t durations_us[FRAME_STATS_WINDOW];
    uint32_t recorded;
    uint32_t next_slot;
} FrameStats;

void frame_stats_reset(FrameStats *stats);
void frame_stats_record(FrameStats *stats, uint32_t duration_us);
uint32_t frame_stats_latest_us(const FrameStats *stats);
uint32_t frame_stats_average_us(const FrameStats *stats);
uint32_t frame_stats_worst_us(const FrameStats *stats);
uint32_t frame_stats_achievable_fps(const FrameStats *stats);
