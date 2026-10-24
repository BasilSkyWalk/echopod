#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device/dma_loop.h"

bool dmac_present(void);
bool dmac_start_loop(const DmaItem *item);
void dmac_stop(void);
bool dmac_running(void);
uint32_t dmac_source(void);
void dmac_suspend(void);
void dmac_resume(void);
