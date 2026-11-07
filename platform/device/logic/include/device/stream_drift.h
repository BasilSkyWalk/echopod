#pragma once

#include <stdint.h>

typedef struct StreamDriftLimits {
    int32_t target;
    int32_t window;
    int32_t settle;
    int32_t spacing;
} StreamDriftLimits;

typedef struct StreamDrift {
    StreamDriftLimits limits;
    int32_t correction;
    int32_t wait;
} StreamDrift;

void stream_drift_init(StreamDrift *drift, StreamDriftLimits limits);
int32_t stream_drift_adjust(StreamDrift *drift, int32_t fill);
