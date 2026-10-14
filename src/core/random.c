#include "core/random.h"

#define ZERO_SEED_REPLACEMENT UINT32_C(0x9E3779B9)

enum {
    SHIFT_LEFT_FIRST = 13,
    SHIFT_RIGHT = 17,
    SHIFT_LEFT_SECOND = 5
};

Random random_seeded(uint32_t seed)
{
    return (Random){.state = seed != 0 ? seed : ZERO_SEED_REPLACEMENT};
}

uint32_t random_next(Random *random)
{
    uint32_t state = random->state;
    state ^= state << SHIFT_LEFT_FIRST;
    state ^= state >> SHIFT_RIGHT;
    state ^= state << SHIFT_LEFT_SECOND;
    random->state = state;
    return state;
}

uint32_t random_below(Random *random, uint32_t bound)
{
    if (bound <= 1) {
        return 0;
    }
    uint32_t limit = UINT32_MAX - UINT32_MAX % bound;
    uint32_t value = random_next(random);
    while (value >= limit) {
        value = random_next(random);
    }
    return value % bound;
}
