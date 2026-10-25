#include "device/stream_drift.h"

void stream_drift_init(StreamDrift *drift, StreamDriftLimits limits)
{
    *drift = (StreamDrift){.limits = limits, .correction = 0};
}

static int32_t distance(int32_t value)
{
    return value < 0 ? -value : value;
}

int32_t stream_drift_adjust(StreamDrift *drift, int32_t fill)
{
    int32_t offset = fill - drift->limits.target;
    if (drift->correction != 0 && -drift->correction * offset <= drift->limits.settle) {
        drift->correction = 0;
    } else if (drift->correction == 0 && distance(offset) > drift->limits.window) {
        drift->correction = offset > 0 ? -1 : 1;
    }
    return drift->correction;
}
