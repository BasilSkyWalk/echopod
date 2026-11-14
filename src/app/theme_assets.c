#include "theme_assets.h"

#include <stddef.h>

#include "core/hash.h"
#include "default_theme.h"
#include "hal/system.h"

enum {
    WALLPAPER_PAIR_BYTES = 1024,
    WORD_BYTES = 4
};

typedef enum AssetState {
    ASSET_NONE,
    ASSET_WANTED,
    ASSET_READY,
    ASSET_FAILED
} AssetState;

typedef struct Arena {
    uint8_t *bytes;
    uint32_t size;
} Arena;

_Static_assert(HAL_THEME_ASSET_BYTES >= WALLPAPER_PAIR_BYTES + THEME_WALLPAPER_LONG_SIDE *
                                                                   THEME_WALLPAPER_SHORT_SIDE / 2,
               "the asset memory holds a whole screen of wallpaper");
_Static_assert(sizeof(Glyph) == THEME_GLYPH_RECORD_BYTES && offsetof(Glyph, bitmap_offset) == 2 &&
                   offsetof(Glyph, width) == 4 && offsetof(Glyph, advance) == 8,
               "a glyph record in a theme file is a Glyph as it lies in memory");

static const Font *builtin_font(ThemeFontId id)
{
    const Font *const fonts[THEME_FONT_COUNT] = {
#define THEME_FONT(id, name, height) DEFAULT_THEME.fonts.name,
#include "ui/theme_fonts.def"
#undef THEME_FONT
    };
    return fonts[id];
}

static uint32_t aligned(uint32_t size)
{
    return (size + WORD_BYTES - 1) / WORD_BYTES * WORD_BYTES;
}

static uint32_t font_bytes(ThemePackageFont font)
{
    return aligned(font.glyph_count * (uint32_t)sizeof(Glyph)) + aligned(font.bitmap_bytes);
}

static uint32_t fonts_end(const ThemePackage *package, int before)
{
    uint32_t end = 0;
    for (int id = 0; id < before; id++) {
        end += package->fonts[id].glyph_count > 0 ? font_bytes(package->fonts[id]) : 0;
    }
    return end;
}

static void unready_font(Font *font, ThemeFontId id)
{
    const Font *builtin = builtin_font(id);
    *font = (Font){.ascent = builtin->ascent, .descent = builtin->descent, .fallback = builtin};
}

static void unready_fonts(ThemeAssets *assets, const ThemePackage *package)
{
    bool any = false;
    for (int id = 0; id < THEME_FONT_COUNT; id++) {
        unready_font(&assets->fonts[id], (ThemeFontId)id);
        any = any || package->fonts[id].glyph_count > 0;
    }
    assets->fonts_state = any ? ASSET_WANTED : ASSET_NONE;
}

void theme_assets_reset(ThemeAssets *assets, const ThemePackage *package)
{
    unready_fonts(assets, package);
    assets->wallpaper_state = ASSET_NONE;
    assets->wallpaper.ready = false;
}

const Font *theme_assets_font(ThemeAssets *assets, const ThemePackage *package, ThemeFontId id)
{
    return package->fonts[id].glyph_count > 0 ? &assets->fonts[id] : NULL;
}

static bool small_enough_to_turn(ThemePackageWallpaper wallpaper)
{
    return wallpaper.width > 0 && wallpaper.width <= THEME_WALLPAPER_SHORT_SIDE &&
           wallpaper.height <= THEME_WALLPAPER_SHORT_SIDE;
}

const Wallpaper *theme_assets_wallpaper(ThemeAssets *assets, const ThemePackage *package,
                                        bool portrait)
{
    uint8_t kind = portrait ? THEME_WALLPAPER_PORTRAIT : THEME_WALLPAPER_LANDSCAPE;
    if (package->wallpapers[kind].width == 0) {
        kind = (uint8_t)(THEME_WALLPAPER_PORTRAIT - kind);
        if (!small_enough_to_turn(package->wallpapers[kind])) {
            return NULL;
        }
    }
    if (assets->wallpaper_state == ASSET_NONE || assets->wallpaper_kind != kind) {
        assets->wallpaper_kind = kind;
        assets->wallpaper_state = ASSET_WANTED;
        assets->wallpaper.ready = false;
    }
    return &assets->wallpaper;
}

void theme_assets_watch(ThemeAssets *assets)
{
    if (assets->wipes == hal_system_theme_asset_wipes()) {
        return;
    }
    for (int id = 0; id < THEME_FONT_COUNT; id++) {
        if (assets->fonts[id].glyph_count > 0) {
            unready_font(&assets->fonts[id], (ThemeFontId)id);
            assets->fonts_state = ASSET_WANTED;
        }
    }
    if (assets->wallpaper_state == ASSET_READY) {
        assets->wallpaper_state = ASSET_WANTED;
        assets->wallpaper.ready = false;
    }
}

bool theme_assets_wanted(const ThemeAssets *assets)
{
    return assets->fonts_state == ASSET_WANTED || assets->wallpaper_state == ASSET_WANTED;
}

static bool glyphs_sound(const Glyph *glyphs, uint32_t count, uint32_t bitmap_bytes)
{
    for (uint32_t index = 0; index < count; index++) {
        const Glyph *glyph = &glyphs[index];
        uint32_t bytes = ((uint32_t)glyph->width * glyph->height + 1) / 2;
        bool ordered = index == 0 || glyph->codepoint > glyphs[index - 1].codepoint;
        if (!ordered || glyph->bitmap_offset > bitmap_bytes ||
            bytes > bitmap_bytes - glyph->bitmap_offset) {
            return false;
        }
    }
    return true;
}

static bool read_section(HalFile file, uint32_t offset, uint8_t *target, uint32_t length)
{
    return offset <= file.size && length <= file.size - offset &&
           hal_file_read(file, offset, target, length);
}

static bool read_font(HalFile file, ThemePackageFont entry, uint8_t *place, Font *font)
{
    uint32_t table = entry.glyph_count * (uint32_t)sizeof(Glyph);
    uint8_t *bitmap = place + aligned(table);
    if (!read_section(file, entry.offset, place, table) ||
        !read_section(file, entry.offset + table, bitmap, entry.bitmap_bytes) ||
        hash_update(hash_update(HASH_START, place, table), bitmap, entry.bitmap_bytes) !=
            entry.checksum ||
        !glyphs_sound((const Glyph *)(void *)place, entry.glyph_count, entry.bitmap_bytes)) {
        return false;
    }
    font->glyphs = (const Glyph *)(void *)place;
    font->bitmap = bitmap;
    font->glyph_count = entry.glyph_count;
    font->bitmap_size = entry.bitmap_bytes;
    font->ascent = entry.ascent;
    font->descent = entry.descent;
    return true;
}

static void load_fonts(ThemeAssets *assets, const ThemePackage *package, HalFile file, Arena arena)
{
    for (int id = 0; id < THEME_FONT_COUNT; id++) {
        ThemePackageFont entry = package->fonts[id];
        uint32_t at = fonts_end(package, id);
        Font loaded = assets->fonts[id];
        bool fits = entry.glyph_count > 0 && at + font_bytes(entry) <= arena.size;
        if (fits && read_font(file, entry, arena.bytes + at, &loaded)) {
            assets->fonts[id] = loaded;
        }
    }
    assets->fonts_state = ASSET_READY;
}

static void fill_pairs(uint32_t *pairs, const uint8_t *palette)
{
    for (uint32_t pair = 0; pair < WALLPAPER_PAIR_BYTES / sizeof(uint32_t); pair++) {
        const uint8_t *left = palette + 2 * (pair >> 4);
        const uint8_t *right = palette + 2 * (pair & 0x0F);
        pairs[pair] = (uint32_t)left[0] | (uint32_t)left[1] << 8 | (uint32_t)right[0] << 16 |
                      (uint32_t)right[1] << 24;
    }
}

static bool read_wallpaper(ThemeAssets *assets, ThemePackageWallpaper entry, HalFile file,
                           Arena arena)
{
    uint8_t palette[THEME_PACKAGE_PALETTE_BYTES];
    uint32_t bytes = entry.width / 2u * entry.height;
    uint32_t *pairs = (uint32_t *)(void *)arena.bytes;
    uint8_t *rows = arena.bytes + WALLPAPER_PAIR_BYTES;
    if (WALLPAPER_PAIR_BYTES + bytes > arena.size ||
        !read_section(file, entry.offset, palette, sizeof palette) ||
        !read_section(file, entry.offset + (uint32_t)sizeof palette, rows, bytes) ||
        hash_update(hash_update(HASH_START, palette, sizeof palette), rows, bytes) !=
            entry.checksum) {
        return false;
    }
    fill_pairs(pairs, palette);
    assets->wallpaper = (Wallpaper){pairs, rows, entry.width, entry.height, true};
    return true;
}

static void load_wallpaper(ThemeAssets *assets, const ThemePackage *package, HalFile file,
                           Arena arena)
{
    uint32_t start = fonts_end(package, THEME_FONT_COUNT);
    Arena rest = {arena.bytes + start, start <= arena.size ? arena.size - start : 0};
    ThemePackageWallpaper entry = package->wallpapers[assets->wallpaper_kind];
    bool loaded = read_wallpaper(assets, entry, file, rest);
    assets->wallpaper_state = loaded ? ASSET_READY : ASSET_FAILED;
}

bool theme_assets_tend(ThemeAssets *assets, const ThemePackage *package, HalFile file)
{
    uint32_t size = 0;
    void *memory = hal_system_theme_assets(&size);
    Arena arena = {memory, memory != NULL ? size : 0};
    assets->wipes = hal_system_theme_asset_wipes();
    if (assets->fonts_state == ASSET_WANTED) {
        load_fonts(assets, package, file, arena);
    }
    if (assets->wallpaper_state == ASSET_WANTED) {
        load_wallpaper(assets, package, file, arena);
    }
    return true;
}
