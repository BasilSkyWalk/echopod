#include "dsp/speed.h"

#include "dsp/equalizer.h"

uint32_t speed_played_quarters(SpeedSetting setting, uint32_t rate)
{
    bool speeds = setting.quarters >= SPEED_MIN_QUARTERS && setting.quarters <= SPEED_MAX_QUARTERS;
    bool stretches = rate >= SPEED_STRETCH_MIN_RATE && rate <= SPEED_STRETCH_MAX_RATE;
    if (!speeds || !stretches) {
        return SPEED_UNITY_QUARTERS;
    }
    uint32_t fastest = SPEED_MAX_DECODED_RATE * SPEED_UNITY_QUARTERS / rate;
    return setting.quarters < fastest ? setting.quarters : fastest;
}

SpeedMethod speed_method_for(SpeedSetting setting, uint32_t rate)
{
    return speed_played_quarters(setting, rate) == SPEED_UNITY_QUARTERS ? SPEED_BYPASS
                                                                        : SPEED_STRETCH;
}

uint32_t speed_equalizer_rate(uint32_t rate, uint32_t quarters)
{
    uint32_t decoded = rate * quarters / SPEED_UNITY_QUARTERS;
    return decoded > EQUALIZER_MAX_RATE ? decoded : rate;
}

void speed_configure(SpeedStage *stage, SpeedSetting setting, uint32_t rate)
{
    uint32_t quarters = speed_played_quarters(setting, rate);
    stage->method = quarters == SPEED_UNITY_QUARTERS ? SPEED_BYPASS : SPEED_STRETCH;
    stage->quarters = (uint8_t)quarters;
    speed_fifo_reset(&stage->fifo, 0);
    if (stage->method == SPEED_STRETCH) {
        stretch_configure(&stage->stretch, &stage->fifo, rate, quarters);
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
