#include "dsp/speed.h"

SpeedMethod speed_method_for(SpeedSetting setting, uint32_t rate)
{
    bool speeds = setting.quarters >= SPEED_MIN_QUARTERS &&
                  setting.quarters <= SPEED_MAX_QUARTERS &&
                  setting.quarters != SPEED_UNITY_QUARTERS;
    bool stretches = rate >= SPEED_STRETCH_MIN_RATE && rate <= SPEED_STRETCH_MAX_RATE;
    return speeds && stretches ? SPEED_STRETCH : SPEED_BYPASS;
}

void speed_configure(SpeedStage *stage, SpeedSetting setting, uint32_t rate)
{
    stage->method = speed_method_for(setting, rate);
    stage->quarters = stage->method == SPEED_BYPASS ? SPEED_UNITY_QUARTERS : setting.quarters;
    speed_fifo_reset(&stage->fifo, 0);
    if (stage->method == SPEED_STRETCH) {
        stretch_configure(&stage->stretch, &stage->fifo, rate, setting.quarters);
    }
}

uint32_t speed_space(const SpeedStage *stage)
{
    return speed_fifo_space(&stage->fifo);
}

uint32_t speed_push(SpeedStage *stage, const int32_t *frames, uint32_t count)
{
    return speed_fifo_append(&stage->fifo, frames, count);
}

uint32_t speed_pull(SpeedStage *stage, int32_t *frames, uint32_t capacity)
{
    return stage->method == SPEED_STRETCH
               ? stretch_pull(&stage->stretch, &stage->fifo, frames, capacity)
               : 0;
}

void speed_finish(SpeedStage *stage)
{
    stage->fifo.finished = true;
}

bool speed_drained(const SpeedStage *stage)
{
    return stage->method != SPEED_STRETCH || stretch_drained(&stage->stretch, &stage->fifo);
}

uint32_t speed_quarters(const SpeedStage *stage)
{
    return stage->quarters;
}

SpeedMethod speed_method(const SpeedStage *stage)
{
    return stage->method;
}
