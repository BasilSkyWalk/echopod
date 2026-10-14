#pragma once

#include "core/frame_stats.h"

void app_start(void);
void app_step(void);
const FrameStats *app_frame_stats(void);
