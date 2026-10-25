#pragma once

#include <stdint.h>

typedef struct StreamDriftLimits {
    int32_t target;
    int32_t window;
    int32_t settle;
} StreamDriftLimits;

typedef struct StreamDrift {
    StreamDriftLimits limits;
    int32_t correction;
} StreamDrift;

void stream_drift_init(StreamDrift *drift, StreamDriftLimits limits);
int32_t stream_drift_adjust(StreamDrift *drift, int32_t fill);
