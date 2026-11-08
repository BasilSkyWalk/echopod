#include "dsp/speed.h"

SpeedMethod speed_method_for(SpeedSetting setting, uint32_t rate)
{
    if (setting.quarters < SPEED_MIN_QUARTERS || setting.quarters > SPEED_MAX_QUARTERS ||
        setting.quarters == SPEED_UNITY_QUARTERS) {
        return SPEED_BYPASS;
    }
    bool stretches =
        setting.keep_pitch && rate >= SPEED_STRETCH_MIN_RATE && rate <= SPEED_STRETCH_MAX_RATE;
    return stretches ? SPEED_STRETCH : SPEED_RESAMPLE;
}

void speed_configure(SpeedStage *stage, SpeedSetting setting, uint32_t rate)
{
    stage->method = speed_method_for(setting, rate);
    if (stage->method == SPEED_STRETCH) {
        stretch_configure(&stage->stretch, &stage->fifo, rate, setting.quarters);
    } else if (stage->method == SPEED_RESAMPLE) {
        resample_configure(&stage->resample, &stage->fifo, setting.quarters);
    }
}

uint32_t speed_space(const SpeedStage *stage)
{
    return stage->method == SPEED_BYPASS ? 0 : speed_fifo_space(&stage->fifo);
}

uint32_t speed_push(SpeedStage *stage, const int32_t *frames, uint32_t count)
{
    return stage->method == SPEED_BYPASS ? 0 : speed_fifo_append(&stage->fifo, frames, count);
}

uint32_t speed_pull(SpeedStage *stage, int32_t *frames, uint32_t capacity)
{
    switch (stage->method) {
    case SPEED_STRETCH:
        return stretch_pull(&stage->stretch, &stage->fifo, frames, capacity);
    case SPEED_RESAMPLE:
        return resample_pull(&stage->resample, &stage->fifo, frames, capacity);
    default:
        return 0;
    }
}

void speed_finish(SpeedStage *stage)
{
    stage->fifo.finished = true;
}

bool speed_drained(const SpeedStage *stage)
{
    switch (stage->method) {
    case SPEED_STRETCH:
        return stretch_drained(&stage->stretch, &stage->fifo);
    case SPEED_RESAMPLE:
        return resample_drained(&stage->resample, &stage->fifo);
    default:
        return true;
    }
}

uint32_t speed_quarters(const SpeedStage *stage)
{
    switch (stage->method) {
    case SPEED_STRETCH:
        return stage->stretch.shape.quarters;
    case SPEED_RESAMPLE:
        return stage->resample.quarters;
    default:
        return SPEED_UNITY_QUARTERS;
    }
}

SpeedMethod speed_method(const SpeedStage *stage)
{
    return stage->method;
}
