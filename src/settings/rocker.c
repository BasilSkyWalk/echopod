#include "settings/rocker.h"

static const Orientation ROCKER_SWAPPED_ORIENTATION = ORIENTATION_PORTRAIT;

bool rocker_swapped_in(Orientation orientation)
{
    return orientation == ROCKER_SWAPPED_ORIENTATION;
}

Key rocker_key(Key key, bool swapped)
{
    if (!swapped) {
        return key;
    }
    if (key == KEY_VOLUME_UP) {
        return KEY_VOLUME_DOWN;
    }
    if (key == KEY_VOLUME_DOWN) {
        return KEY_VOLUME_UP;
    }
    return key;
}
