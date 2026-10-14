#pragma once

#include <stdbool.h>

#include "gfx/canvas.h"

Canvas ui_test_canvas(void);
bool ui_test_area_has_color(const Canvas *canvas, Rect area, Color color);
bool ui_test_area_has_only(const Canvas *canvas, Rect area, Color color);
