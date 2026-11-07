#include "equalizer_files.h"

#include <stddef.h>

#include "hal/storage.h"

enum {
    EXTENSION_LENGTH = 4
};

static char lower(char character)
{
    return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static bool text_extension(const char *name)
{
    const char *dot = NULL;
    uint32_t length = 0;
    for (uint32_t index = 0; name[index] != '\0'; index++) {
        if (name[index] == '.') {
            dot = name + index;
        }
        length++;
    }
    return dot != NULL && length > 4 && dot[4] == '\0' && lower(dot[1]) == 't' &&
           lower(dot[2]) == 'x' && lower(dot[3]) == 't';
}

uint32_t equalizer_files_stem_length(const char *name)
{
    uint32_t length = 0;
    while (name[length] != '\0') {
        length++;
    }
    return text_extension(name) ? length - EXTENSION_LENGTH : length;
}

void equalizer_files_name(const EqualizerFileList *list, int index, char *name)
{
    const char *stem = list->names[index];
    uint32_t length = 0;
    while (stem[length] != '\0') {
        name[length] = stem[length];
        length++;
    }
    for (uint32_t extension = 0; extension <= EXTENSION_LENGTH; extension++) {
        name[length + extension] = extension == 0 ? '.' : stem[length + extension];
    }
}

void equalizer_files_scan(EqualizerFileList *list)
{
    list->count = 0;
#ifdef ECHO_EQ_FILES_HIDDEN
    return;
#endif
    HalDir dir;
    if (!hal_dir_open(&dir, EQUALIZER_FOLDER_PATH)) {
        return;
    }
    HalDirEntry entry;
    while (hal_dir_next(&dir, &entry)) {
        if (entry.directory || entry.hidden || entry.name[0] == '.' ||
            !text_extension(entry.name)) {
            continue;
        }
        uint32_t length = 0;
        while (entry.name[length] != '\0') {
            length++;
        }
        if (length + 1 > EQUALIZER_PRESET_NAME_CAPACITY) {
            continue;
        }
        char *name = list->names[list->count];
        for (uint32_t index = 0; index <= length; index++) {
            name[index] = entry.name[index];
        }
        name[length - EXTENSION_LENGTH] = '\0';
        list->count++;
        if (list->count == EQUALIZER_FILES_MAX) {
            break;
        }
    }
    hal_dir_close(&dir);
}
