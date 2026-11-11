#include "theme_store.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "default_theme.h"
#include "hal/storage.h"
#include "hal/system.h"

enum {
    MAGIC_BYTES = 4,
    RECORD_BYTES = MAGIC_BYTES + THEME_PACKAGE_NAME_BYTES,
    PATH_CAPACITY = 64,
    SUFFIX_BYTES = 10,
    CASE_BIT = 0x20
};

const char THEME_STORE_PATH[] = "/.echo/theme.bin";
const char THEME_STORE_FOLDER[] = "/Themes";

static const char SUFFIX[SUFFIX_BYTES + 1] = ".echotheme";
static const uint8_t MAGIC[MAGIC_BYTES] = {'E', 'T', 'H', 'S'};

typedef struct ThemeMemory {
    ThemePackage package;
    bool active;
    uint8_t revision;
    uint8_t count;
    char names[THEME_STORE_FILES][THEME_PACKAGE_NAME_BYTES];
    const char *options[THEME_STORE_OPTIONS];
} ThemeMemory;

static ThemeMemory *memory(void)
{
    uint32_t size = 0;
    ThemeMemory *block = hal_system_theme_memory(&size);
    return size >= sizeof(ThemeMemory) ? block : NULL;
}

static bool same_name(const char *first, const char *second)
{
    int index = 0;
    while (first[index] != '\0' && first[index] == second[index]) {
        index++;
    }
    return first[index] == second[index];
}

static void copy_name(char *target, const char *source, size_t length)
{
    bool ended = false;
    for (size_t index = 0; index < THEME_PACKAGE_NAME_BYTES; index++) {
        ended = ended || index >= length || source[index] == '\0';
        target[index] = ended ? '\0' : source[index];
    }
}

static size_t stem_length(const char *file)
{
    size_t length = 0;
    while (file[length] != '\0') {
        length++;
    }
    if (length <= SUFFIX_BYTES || length - SUFFIX_BYTES >= THEME_PACKAGE_NAME_BYTES) {
        return 0;
    }
    for (size_t index = 0; index < SUFFIX_BYTES; index++) {
        char letter = file[length - SUFFIX_BYTES + index];
        if ((letter | (index > 0 ? CASE_BIT : 0)) != SUFFIX[index]) {
            return 0;
        }
    }
    return length - SUFFIX_BYTES;
}

static bool read_package(const char *name, ThemePackage *package)
{
    char path[PATH_CAPACITY];
    uint8_t bytes[THEME_PACKAGE_MAX_BYTES];
    StringBuilder builder = string_builder_make(path, sizeof path);
    string_builder_append(&builder, THEME_STORE_FOLDER);
    string_builder_append_char(&builder, '/');
    string_builder_append(&builder, name);
    string_builder_append(&builder, SUFFIX);
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return false;
    }
    bool read = file.size <= sizeof bytes && hal_file_read(file, 0, bytes, file.size);
    hal_file_close(&file);
    ThemePackage decoded = {.colors = DEFAULT_THEME_DARK_COLORS, .boxes = THEME_STANDARD_BOXES};
    if (!read || !theme_package_decode(bytes, file.size, &decoded)) {
        return false;
    }
    copy_name(decoded.name, name, THEME_PACKAGE_NAME_BYTES - 1);
    *package = decoded;
    return true;
}

static void apply(ThemeMemory *block, const char *name)
{
    block->active = name[0] != '\0' && read_package(name, &block->package);
    block->revision++;
}

static void remember(const ThemeMemory *block)
{
    uint8_t record[RECORD_BYTES] = {0};
    for (int index = 0; index < MAGIC_BYTES; index++) {
        record[index] = MAGIC[index];
    }
    if (block->active) {
        copy_name((char *)record + MAGIC_BYTES, block->package.name, THEME_PACKAGE_NAME_BYTES - 1);
    }
    (void)hal_file_write(THEME_STORE_PATH, record, sizeof record);
}

static bool remembered(char *name)
{
    uint8_t record[RECORD_BYTES];
    HalFile file;
    if (!hal_file_open(&file, THEME_STORE_PATH)) {
        return false;
    }
    bool read = file.size >= sizeof record && hal_file_read(file, 0, record, sizeof record);
    hal_file_close(&file);
    for (int index = 0; read && index < MAGIC_BYTES; index++) {
        read = record[index] == MAGIC[index];
    }
    if (read) {
        copy_name(name, (const char *)record + MAGIC_BYTES, THEME_PACKAGE_NAME_BYTES - 1);
    }
    return read;
}

void theme_store_start(void)
{
    ThemeMemory *block = memory();
    char name[THEME_PACKAGE_NAME_BYTES];
    if (block == NULL) {
        return;
    }
    block->count = 0;
    apply(block, remembered(name) ? name : "");
}

const ThemePackage *theme_store_package(void)
{
    const ThemeMemory *block = memory();
    return block != NULL && block->active ? &block->package : NULL;
}

uint8_t theme_store_revision(void)
{
    const ThemeMemory *block = memory();
    return block != NULL ? block->revision : 0;
}

static void note_file(ThemeMemory *block, const HalDirEntry *entry)
{
    size_t length = entry->directory || entry->hidden ? 0 : stem_length(entry->name);
    if (length > 0 && block->count < THEME_STORE_FILES) {
        copy_name(block->names[block->count], entry->name, length);
        block->count++;
    }
}

int theme_store_scan(void)
{
    ThemeMemory *block = memory();
    HalDir dir;
    HalDirEntry entry;
    if (block == NULL) {
        return 0;
    }
    block->count = 0;
    if (!hal_dir_open(&dir, THEME_STORE_FOLDER)) {
        return 0;
    }
    while (block->count < THEME_STORE_FILES && hal_dir_next(&dir, &entry)) {
        note_file(block, &entry);
    }
    hal_dir_close(&dir);
    return block->count;
}

const char *theme_store_name(int index)
{
    const ThemeMemory *block = memory();
    return block != NULL && index >= 0 && index < block->count ? block->names[index] : "";
}

int theme_store_current(void)
{
    const ThemeMemory *block = memory();
    for (int index = 0; block != NULL && block->active && index < block->count; index++) {
        if (same_name(block->names[index], block->package.name)) {
            return index;
        }
    }
    return THEME_STORE_NONE;
}

bool theme_store_choose(int index)
{
    ThemeMemory *block = memory();
    if (block == NULL) {
        return false;
    }
    apply(block, theme_store_name(index));
    remember(block);
    return block->active;
}

bool theme_store_wear(const uint8_t *bytes, uint32_t size)
{
    ThemeMemory *block = memory();
    ThemePackage decoded = {.colors = DEFAULT_THEME_DARK_COLORS, .boxes = THEME_STANDARD_BOXES};
    if (block == NULL) {
        return false;
    }
    block->active = theme_package_decode(bytes, size, &decoded);
    if (block->active) {
        block->package = decoded;
    }
    block->revision++;
    return block->active;
}

const char **theme_store_options(void)
{
    ThemeMemory *block = memory();
    return block != NULL ? block->options : NULL;
}
