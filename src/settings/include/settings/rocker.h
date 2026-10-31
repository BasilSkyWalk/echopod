#pragma once

#include <stdbool.h>

#include "core/key.h"
#include "settings/settings.h"

bool rocker_swapped_in(Orientation orientation);
Key rocker_key(Key key, bool swapped);
