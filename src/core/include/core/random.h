#pragma once

#include <stdint.h>

typedef struct Random {
    uint32_t state;
} Random;

Random random_seeded(uint32_t seed);
uint32_t random_next(Random *random);
uint32_t random_below(Random *random, uint32_t bound);
