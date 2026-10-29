#pragma once

#include <stdint.h>

enum {
    SIM_GLYPHS_CACHE_BYTES = 0xD000 - 1024
};

uint32_t sim_glyphs_attach(void);
uint32_t sim_glyphs_face_count(void);
