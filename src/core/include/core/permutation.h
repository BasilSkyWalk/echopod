#pragma once

#include <stdint.h>

enum {
    PERMUTATION_ROUNDS = 6
};

typedef struct Permutation {
    uint32_t size;
    uint32_t half_bits;
    uint32_t keys[PERMUTATION_ROUNDS];
} Permutation;

Permutation permutation_make(uint32_t size, uint32_t seed);
uint32_t permutation_at(const Permutation *permutation, uint32_t index);
