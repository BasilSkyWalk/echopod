#include "library/spoken_path.h"

#include <stdbool.h>

static char lower(char letter)
{
    return letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter;
}

static bool name_matches(const char *name, size_t length, const char *expected)
{
    size_t index = 0;
    for (; index < length && expected[index] != '\0'; index++) {
        if (lower(name[index]) != expected[index]) {
            return false;
        }
    }
    return index == length && expected[index] == '\0';
}

SpokenKind spoken_kind_of_name(const char *name, size_t length)
{
    if (name_matches(name, length, "audiobooks") || name_matches(name, length, "audiobook")) {
        return SPOKEN_AUDIOBOOK;
    }
    if (name_matches(name, length, "podcasts") || name_matches(name, length, "podcast")) {
        return SPOKEN_PODCAST;
    }
    return SPOKEN_NONE;
}

SpokenKind spoken_kind_of(const char *path)
{
    while (*path == '/') {
        path++;
    }
    size_t length = 0;
    while (path[length] != '\0' && path[length] != '/') {
        length++;
    }
    return path[length] == '/' ? spoken_kind_of_name(path, length) : SPOKEN_NONE;
}
