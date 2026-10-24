#pragma once

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

void sim_input_handle_event(const SDL_Event *event);
void sim_input_inject(Key key, bool pressed, uint32_t time_ms);
