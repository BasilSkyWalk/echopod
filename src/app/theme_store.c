#include "theme_store.h"

#include <stddef.h>

#include "core/hash.h"
#include "core/string_builder.h"
#include "default_theme.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "np_faces/scene.h"

enum {
    MAGIC_BYTES = 4,
    RECORD_BYTES = MAGIC_BYTES + THEME_PACKAGE_NAME_BYTES,
    PATH_CAPACITY = 64,
    SUFFIX_BYTES = 10,
    CASE_BIT = 0x20,
    NO_BUILTIN = -1,
    SCENE_MAGIC = 0x4E435345,
    WORD_BYTES = 4
};

typedef struct SceneStamp {
    uint32_t magic;
    uint32_t wipes;
    uint32_t revision;
    ThemePackageScene scenes[THEME_SCENE_COUNT];
} SceneStamp;

enum {
    SCENE_HEADER_BYTES = (sizeof(SceneStamp) + WORD_BYTES - 1) / WORD_BYTES * WORD_BYTES,
    SCENE_SPARE_BYTES = SCENE_HEADER_BYTES + SCENE_WORK_BYTES
};

const char THEME_STORE_PATH[] = "/.echo/theme.bin";
const char THEME_STORE_FOLDER[] = "/Themes";

static const char SUFFIX[SUFFIX_BYTES + 1] = ".echotheme";
static const uint8_t MAGIC[MAGIC_BYTES] = {'E', 'T', 'H', 'S'};

typedef struct ThemeMemory {
    ThemePackage package;
    bool active;
    bool card;
    int8_t builtin;
    uint8_t revision;
    uint8_t count;
    char names[THEME_STORE_FILES][THEME_PACKAGE_NAME_BYTES];
    const char *options[THEME_STORE_OPTIONS];
    char accent_name[THEME_PACKAGE_NAME_BYTES];
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

static bool open_card(const char *name, ThemeSource *source)
{
    char path[PATH_CAPACITY];
    StringBuilder builder = string_builder_make(path, sizeof path);
    string_builder_append(&builder, THEME_STORE_FOLDER);
    string_builder_append_char(&builder, '/');
    string_builder_append(&builder, name);
    string_builder_append(&builder, SUFFIX);
    source->builtin = NO_BUILTIN;
    if (!hal_file_open(&source->file, path)) {
        return false;
    }
    source->size = source->file.size;
    return true;
}

static bool open_builtin(int theme, ThemeSource *source)
{
    source->builtin = theme;
    source->size = theme >= 0 ? hal_system_builtin_theme_size((uint32_t)theme) : 0;
    return source->size > 0;
}

static void close_source(ThemeSource *source)
{
    if (source->builtin == NO_BUILTIN) {
        hal_file_close(&source->file);
    }
}

static bool open_worn(const ThemeMemory *block, ThemeSource *source)
{
    return block->card ? open_card(block->package.name, source)
                       : open_builtin(block->builtin, source);
}

static bool read_package(ThemeMemory *block, ThemeSource *source)
{
    uint32_t held = source->size < sizeof block->file ? source->size : sizeof block->file;
    bool read = theme_source_read(source, 0, block->file, held);
    close_source(source);
    return read && decode(block, held);
}

static bool read_card(ThemeMemory *block, const char *name)
{
    ThemeSource source;
    if (name[0] == '\0' || !open_card(name, &source) || !read_package(block, &source)) {
        return false;
    }
    copy_name(block->package.name, name, THEME_PACKAGE_NAME_BYTES - 1);
    return true;
}

static bool read_builtin(ThemeMemory *block)
{
    ThemeSource source;
    return open_builtin(block->builtin, &source) && read_package(block, &source);
}

static void apply(ThemeMemory *block, const char *card)
{
    block->card = read_card(block, card);
    block->active = block->card || read_builtin(block);
    theme_assets_reset(&block->assets, &block->package);
    block->revision++;
}

static void remember(const ThemeMemory *block)
{
    uint8_t record[RECORD_BYTES] = {0};
    for (int index = 0; index < MAGIC_BYTES; index++) {
        record[index] = MAGIC[index];
    }
    if (block->card) {
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

void theme_store_start(int builtin)
{
    ThemeMemory *block = memory();
    char name[THEME_PACKAGE_NAME_BYTES];
    if (block == NULL) {
        return;
    }
    block->count = 0;
    block->builtin = (int8_t)builtin;
    apply(block, remembered(name) ? name : "");
}

void theme_store_follow(int builtin)
{
    ThemeMemory *block = memory();
    if (block != NULL && !block->card && block->builtin != builtin) {
        block->builtin = (int8_t)builtin;
        apply(block, "");
    }
}

const ThemePackage *theme_store_package(void)
{
    const ThemeMemory *block = memory();
    return block != NULL && block->active ? &block->package : NULL;
}

bool theme_store_card(void)
{
    const ThemeMemory *block = memory();
    return block != NULL && block->card;
}

const char *theme_store_accent_name(int index)
{
    ThemeMemory *block = memory();
    if (block == NULL || !block->active || index < 0 || index >= block->package.accent_count) {
        return "";
    }
    const uint8_t *name = block->file + block->package.accents_at +
                          (uint32_t)index * THEME_PACKAGE_ACCENT_BYTES + THEME_ACCENT_NAME_FIELD;
    copy_name(block->accent_name, (const char *)name, THEME_ACCENT_NAME_BYTES - 1);
    return block->accent_name;
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
    ThemeSource source;
    if (block == NULL || !block->active || !theme_assets_wanted(&block->assets) ||
        !open_worn(block, &source)) {
        return false;
    }
    bool changed = theme_assets_tend(&block->assets, &block->package, &source);
    close_source(&source);
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
    for (int index = 0; block != NULL && block->card && index < block->count; index++) {
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
    return block->card;
}

void theme_store_choose_builtin(int builtin)
{
    ThemeMemory *block = memory();
    if (block == NULL) {
        return;
    }
    block->builtin = (int8_t)builtin;
    apply(block, "");
    remember(block);
}

bool theme_store_wear(const char *name)
{
    ThemeMemory *block = memory();
    if (block == NULL) {
        return false;
    }
    apply(block, name);
    return block->card;
}

static ThemePackageScene scene_slot(const ThemePackage *package, int which)
{
    return which == THEME_SCENE_HOME ? package->home_scene : package->scene;
}

bool theme_store_has_scene(int which)
{
    const ThemeMemory *block = memory();
    return block != NULL && block->active && scene_slot(&block->package, which).code_bytes > 0;
}

static uint32_t scene_bytes(ThemePackageScene scene)
{
    uint32_t chrome = (scene.flags & THEME_SCENE_CHROME) != 0 ? THEME_SCENE_CHROME_BYTES : 0;
    return chrome + scene.code_bytes + scene.string_bytes;
}

static uint32_t section_bytes(ThemePackageScene scene)
{
    return (scene_bytes(scene) + WORD_BYTES - 1) / WORD_BYTES * WORD_BYTES;
}

static bool read_scene(ThemeSource *source, ThemePackageScene scene, uint8_t *room)
{
    uint32_t bytes = scene_bytes(scene);
    return bytes == 0 || (theme_source_read(source, scene.offset, room, bytes) &&
                          hash_update(HASH_START, room, bytes) == scene.checksum);
}

static bool load_scenes(ThemeMemory *block, uint8_t *memory, uint32_t size)
{
    SceneStamp *stamp = (SceneStamp *)(void *)memory;
    uint8_t *room = memory + SCENE_HEADER_BYTES;
    ThemePackageScene first = block->package.scene;
    ThemePackageScene second = block->package.home_scene;
    ThemeSource source;
    stamp->magic = 0;
    if (section_bytes(first) + section_bytes(second) > size - SCENE_SPARE_BYTES ||
        !open_worn(block, &source)) {
        return false;
    }
    bool loaded = read_scene(&source, first, room) &&
                  read_scene(&source, second, room + section_bytes(first));
    close_source(&source);
    if (loaded) {
        *stamp =
            (SceneStamp){SCENE_MAGIC, hal_system_scene_wipes(), block->revision, {first, second}};
    }
    return loaded;
}

bool theme_store_scene(int which, SceneCode *code, const ThemeColors **chrome)
{
    ThemeMemory *block = memory();
    uint32_t size = 0;
    uint8_t *place = theme_store_has_scene(which) ? hal_system_scene_memory(&size) : NULL;
    if (place == NULL || size <= SCENE_SPARE_BYTES) {
        return false;
    }
    const SceneStamp *stamp = (const SceneStamp *)(const void *)place;
    bool current = stamp->magic == SCENE_MAGIC && stamp->wipes == hal_system_scene_wipes() &&
                   stamp->revision == block->revision &&
                   stamp->scenes[0].checksum == block->package.scene.checksum &&
                   stamp->scenes[1].checksum == block->package.home_scene.checksum;
    if (!current && !load_scenes(block, place, size)) {
        return false;
    }
    ThemePackageScene scene = stamp->scenes[which == THEME_SCENE_HOME];
    const uint8_t *section = place + SCENE_HEADER_BYTES +
                             (which == THEME_SCENE_HOME ? section_bytes(stamp->scenes[0]) : 0);
    bool tinted = (scene.flags & THEME_SCENE_CHROME) != 0;
    *chrome = tinted ? (const ThemeColors *)(const void *)section : NULL;
    section += tinted ? THEME_SCENE_CHROME_BYTES : 0;
    uint32_t work_at = (size - SCENE_WORK_BYTES) / WORD_BYTES * WORD_BYTES;
    *code = (SceneCode){section,
                        scene.code_bytes,
                        (const char *)section + scene.code_bytes,
                        scene.string_bytes,
                        place + work_at,
                        SCENE_WORK_BYTES};
    return true;
}

const char **theme_store_options(void)
{
    ThemeMemory *block = memory();
    return block != NULL ? block->options : NULL;
}
