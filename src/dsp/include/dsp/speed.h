#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dsp/speed_fifo.h"
#include "dsp/stretch.h"

enum {
    SPEED_UNITY_QUARTERS = 4,
    SPEED_MIN_QUARTERS = 3,
    SPEED_MAX_QUARTERS = 8,
    SPEED_STRETCH_MIN_RATE = 4000,
    SPEED_STRETCH_MAX_RATE = 48000
};

typedef struct SpeedSetting {
    uint8_t quarters;
} SpeedSetting;

typedef enum SpeedMethod {
    SPEED_BYPASS,
    SPEED_STRETCH
} SpeedMethod;

typedef struct SpeedStage {
    SpeedMethod method;
    SpeedFifo fifo;
    Stretch stretch;
} SpeedStage;

SpeedMethod speed_method_for(SpeedSetting setting, uint32_t rate);
void speed_configure(SpeedStage *stage, SpeedSetting setting, uint32_t rate);
uint32_t speed_space(const SpeedStage *stage);
uint32_t speed_push(SpeedStage *stage, const int32_t *frames, uint32_t count);
uint32_t speed_pull(SpeedStage *stage, int32_t *frames, uint32_t capacity);
void speed_finish(SpeedStage *stage);
bool speed_drained(const SpeedStage *stage);
uint32_t speed_quarters(const SpeedStage *stage);
SpeedMethod speed_method(const SpeedStage *stage);
