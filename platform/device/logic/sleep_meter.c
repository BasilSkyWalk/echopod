#include "device/sleep_meter.h"

enum {
    PERCENT = 100
};

void sleep_meter_init(SleepMeter *meter, uint32_t now_us)
{
    *meter = (SleepMeter){.window_start_us = now_us, .slept_us = 0, .percent = 0};
}

void sleep_meter_slept(SleepMeter *meter, uint32_t slept_us)
{
    meter->slept_us += slept_us;
}

void sleep_meter_advance(SleepMeter *meter, uint32_t now_us)
{
    uint32_t elapsed_us = now_us - meter->window_start_us;
    if (elapsed_us < SLEEP_METER_WINDOW_US) {
        return;
    }
    uint64_t share = (uint64_t)meter->slept_us * PERCENT / elapsed_us;
    meter->percent = share > PERCENT ? PERCENT : (uint8_t)share;
    meter->window_start_us = now_us;
    meter->slept_us = 0;
}

uint8_t sleep_meter_percent(const SleepMeter *meter)
{
    return meter->percent;
}
