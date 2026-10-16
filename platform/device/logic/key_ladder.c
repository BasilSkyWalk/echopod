#include "device/key_ladder.h"

#include <stddef.h>

typedef struct LadderBand {
    uint16_t low;
    uint16_t high;
    Key key;
} LadderBand;

static const LadderBand LADDER_BANDS[] = {
    {0, 69, KEY_VOLUME_DOWN}, {70, 183, KEY_VOLUME_UP}, {200, 319, KEY_PLAY},
    {320, 409, KEY_MENU},     {450, 549, KEY_BACK},
};

bool key_ladder_decode(uint16_t reading, Key *key)
{
    for (size_t index = 0; index < sizeof LADDER_BANDS / sizeof LADDER_BANDS[0]; index++) {
        const LadderBand *band = &LADDER_BANDS[index];
        if (reading >= band->low && reading <= band->high) {
            *key = band->key;
            return true;
        }
    }
    return false;
}

uint8_t key_ladder_mask(uint16_t reading)
{
    Key key = KEY_POWER;
    return key_ladder_decode(reading, &key) ? (uint8_t)(1u << key) : 0;
}
