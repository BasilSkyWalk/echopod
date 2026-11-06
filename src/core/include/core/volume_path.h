#pragma once

#include <stdbool.h>
#include <stddef.h>

#define VOLUME_INTERNAL_NAME ":internal"

enum {
    VOLUME_INTERNAL_NAME_LENGTH = sizeof VOLUME_INTERNAL_NAME - 1
};

bool volume_is_internal_name(const char *name, size_t length);
const char *volume_internal_path(const char *path);
