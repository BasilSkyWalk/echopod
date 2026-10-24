#include "core/permutation.h"

#include "core/random.h"

#define MIX_FIRST UINT32_C(0x7FEB352D)
#define MIX_SECOND UINT32_C(0x846CA68B)

enum {
    MIX_SHIFT_FIRST = 16,
    MIX_SHIFT_SECOND = 15,
    MIX_SHIFT_THIRD = 16,
    MOST_HALF_BITS = 15
};

static uint32_t mix(uint32_t value)
{
    value ^= value >> MIX_SHIFT_FIRST;
    value *= MIX_FIRST;
    value ^= value >> MIX_SHIFT_SECOND;
    value *= MIX_SECOND;
    value ^= value >> MIX_SHIFT_THIRD;
    return value;
}

static uint32_t half_bits_for(uint32_t size)
{
    uint32_t half_bits = 1;
    while (half_bits < MOST_HALF_BITS && (UINT32_C(1) << (half_bits * 2)) < size) {
        half_bits++;
    }
    return half_bits;
}

Permutation permutation_make(uint32_t size, uint32_t seed)
{
    Permutation permutation = {.size = size, .half_bits = half_bits_for(size)};
    Random random = random_seeded(seed);
    for (uint32_t round = 0; round < PERMUTATION_ROUNDS; round++) {
        permutation.keys[round] = random_next(&random);
    }
    return permutation;
}

static uint32_t scramble(const Permutation *permutation, uint32_t value)
{
    uint32_t mask = (UINT32_C(1) << permutation->half_bits) - 1;
    uint32_t left = value >> permutation->half_bits;
    uint32_t right = value & mask;
    for (uint32_t round = 0; round < PERMUTATION_ROUNDS; round++) {
        uint32_t next = left ^ (mix(right ^ permutation->keys[round]) & mask);
        left = right;
        right = next;
    }
    return left << permutation->half_bits | right;
}

uint32_t permutation_at(const Permutation *permutation, uint32_t index)
{
    if (index >= permutation->size) {
        return index;
    }
    uint32_t value = scramble(permutation, index);
    while (value >= permutation->size) {
        value = scramble(permutation, value);
    }
    return value;
}
