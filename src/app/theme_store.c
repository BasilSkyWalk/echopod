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
    ThemeIcons icons;
    ThemeAssets assets;
    uint8_t file[THEME_PACKAGE_MAX_BYTES];
} ThemeMemory;

_Static_assert(sizeof(ThemeMemory) <= HAL_THEME_MEMORY_BYTES,
               "the theme memory the platform hands over holds the store");

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

static bool decode(ThemeMemory *block, uint32_t size)
{
    ThemePackage decoded = {.colors = DEFAULT_THEME_DARK_COLORS, .boxes = THEME_STANDARD_BOXES};
    if (!theme_package_decode(block->file, size, &decoded)) {
        return false;
    }
    for (int id = 0; id < THEME_ICON_COUNT; id++) {
        ThemePackageIcon icon = decoded.icons[id];
        const uint8_t *levels = icon.width > 0 ? block->file + icon.offset : NULL;
        block->icons.masks[id] = (IconMask){levels, icon.width, icon.height};
    }
    block->package = decoded;
    return true;
}

static bool open_theme(const char *name, HalFile *file)
{
    char path[PATH_CAPACITY];
    StringBuilder builder = string_builder_make(path, sizeof path);
    string_builder_append(&builder, THEME_STORE_FOLDER);
    string_builder_append_char(&builder, '/');
    string_builder_append(&builder, name);
    string_builder_append(&builder, SUFFIX);
    return hal_file_open(file, path);
}

static bool read_package(const char *name, ThemeMemory *block)
{
    HalFile file;
    if (!open_theme(name, &file)) {
        return false;
    }
    uint32_t held = file.size < sizeof block->file ? file.size : sizeof block->file;
    bool read = hal_file_read(file, 0, block->file, held);
    hal_file_close(&file);
    if (!read || !decode(block, held)) {
        return false;
    }
    copy_name(block->package.name, name, THEME_PACKAGE_NAME_BYTES - 1);
    return true;
}

static void apply(ThemeMemory *block, const char *name)
{
    block->active = name[0] != '\0' && read_package(name, block);
    theme_assets_reset(&block->assets, &block->package);
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

const ThemeIcons *theme_store_icons(void)
{
    const ThemeMemory *block = memory();
    return block != NULL && block->active ? &block->icons : NULL;
}

const Font *theme_store_font(ThemeFontId id)
{
    ThemeMemory *block = memory();
    return block != NULL && block->active ? theme_assets_font(&block->assets, &block->package, id)
                                          : NULL;
}

const Wallpaper *theme_store_wallpaper(bool portrait)
{
    ThemeMemory *block = memory();
    if (block == NULL || !block->active) {
        return NULL;
    }
    return theme_assets_wallpaper(&block->assets, &block->package, portrait);
}

void theme_store_watch_assets(void)
{
    ThemeMemory *block = memory();
    if (block != NULL && block->active) {
        theme_assets_watch(&block->assets);
    }
}

bool theme_store_tend_assets(void)
{
    theme_store_watch_assets();
    ThemeMemory *block = memory();
    HalFile file;
    if (block == NULL || !block->active || !theme_assets_wanted(&block->assets) ||
        !open_theme(block->package.name, &file)) {
        return false;
    }
    bool changed = theme_assets_tend(&block->assets, &block->package, file);
    hal_file_close(&file);
    return changed;
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

bool theme_store_wear(const char *name)
{
    ThemeMemory *block = memory();
    if (block == NULL) {
        return false;
    }
    apply(block, name);
    return block->active;
}

const char **theme_store_options(void)
{
    ThemeMemory *block = memory();
    return block != NULL ? block->options : NULL;
}
