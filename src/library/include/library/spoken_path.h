#pragma once

#include <stddef.h>

typedef enum SpokenKind {
    SPOKEN_NONE,
    SPOKEN_AUDIOBOOK,
    SPOKEN_PODCAST
} SpokenKind;

SpokenKind spoken_kind_of(const char *path);
SpokenKind spoken_kind_of_name(const char *name, size_t length);
