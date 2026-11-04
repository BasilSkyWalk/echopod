#include "equalizer_files.h"

#include <stddef.h>

#include "hal/audio.h"
#include "hal/storage.h"

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

void equalizer_files_scan(EqualizerFileList *list)
{
    list->count = 0;
    HalDir dir;
    if (!hal_dir_open(&dir, EQUALIZER_FOLDER_PATH)) {
        return;
    }
    HalDirEntry entry;
    uint32_t seen = 0;
    while (hal_dir_next(&dir, &entry)) {
        if (seen++ % 8 == 0) {
            hal_audio_service();
        }
        if (entry.directory || entry.hidden || entry.name[0] == '.' ||
            !text_extension(entry.name) || list->count == EQUALIZER_FILES_MAX) {
            continue;
        }
        uint32_t length = 0;
        while (entry.name[length] != '\0') {
            length++;
        }
        if (length + 1 > EQUALIZER_PRESET_NAME_CAPACITY) {
            continue;
        }
        for (uint32_t index = 0; index <= length; index++) {
            list->names[list->count][index] = entry.name[index];
        }
        list->count++;
    }
    hal_dir_close(&dir);
}
