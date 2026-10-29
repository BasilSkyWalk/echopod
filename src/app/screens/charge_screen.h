#pragma once

#include <stddef.h>

#include "gfx/canvas.h"
#include "hal/power.h"
#include "ui/theme.h"

void charge_screen_draw(Canvas *canvas, const Theme *theme, HalBattery battery);
void charge_screen_text(HalBattery battery, char *text, size_t capacity);
