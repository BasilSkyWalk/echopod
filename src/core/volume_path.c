#include "core/volume_path.h"

bool volume_is_internal_name(const char *name, size_t length)
{
    if (length != VOLUME_INTERNAL_NAME_LENGTH) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (name[index] != VOLUME_INTERNAL_NAME[index]) {
            return false;
        }
    }
    return true;
}

const char *volume_internal_path(const char *path)
{
    if (path[0] != '/' || !volume_is_internal_name(path + 1, VOLUME_INTERNAL_NAME_LENGTH)) {
        return NULL;
    }
    const char *rest = path + 1 + VOLUME_INTERNAL_NAME_LENGTH;
    if (*rest == '\0') {
        return "/";
    }
    return *rest == '/' ? rest : NULL;
}
