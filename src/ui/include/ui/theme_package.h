#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/theme.h"

enum {
    THEME_PACKAGE_VERSION = 1,
    THEME_PACKAGE_HEADER_BYTES = 48,
    THEME_PACKAGE_NAME_BYTES = 24,
    THEME_PACKAGE_COLORS_FIELD = 16,
    THEME_PACKAGE_BOXES_FIELD = 18,
    THEME_PACKAGE_BOX_BYTES_FIELD = 20,
    THEME_PACKAGE_SELECTION_FIELD = 21,
    THEME_PACKAGE_SPACING_FIELD = 22,
    THEME_PACKAGE_ICONS_FIELD = 23,
    THEME_PACKAGE_NAME_FIELD = 24,
    THEME_PACKAGE_ICON_ENTRY_BYTES = 4,
    THEME_PACKAGE_ASSETS_FIELD = 48,
    THEME_PACKAGE_LAYOUTS_FIELD = 49,
    THEME_PACKAGE_ACCENTS_FIELD = 51,
    THEME_PACKAGE_FLAGS_FIELD = 52,
    THEME_PACKAGE_FLAGS_HEADER_BYTES = 56,
    THEME_PACKAGE_ACCENT_BYTES = 20,
    THEME_PACKAGE_MOST_ACCENTS = 8,
    THEME_ACCENT_NAME_BYTES = 13,
    THEME_ACCENT_NAME_FIELD = 7,
    THEME_ACCENT_CUSTOM = 0xFF,
    THEME_FLAG_DARK_COVER_FLOW = 1,
    THEME_FLAG_NO_HOME_ART = 2,
    THEME_SCENE_MOST_BYTES = 6144,
    THEME_SCENE_CHROME = 1,
    THEME_SCENE_CHROME_BYTES = 2 * THEME_COLOR_COUNT,
    THEME_PACKAGE_LAYOUT_BOX_BYTES = 8,
    THEME_LAYOUT_REACH = 320,
    THEME_LAYOUT_MOST_COVER = 170,
    THEME_PACKAGE_ASSET_HEADER_BYTES = 52,
    THEME_PACKAGE_ASSET_ENTRY_BYTES = 16,
    THEME_PACKAGE_PALETTE_COLORS = 16,
    THEME_PACKAGE_PALETTE_BYTES = 2 * THEME_PACKAGE_PALETTE_COLORS,
    THEME_PACKAGE_MAX_BYTES = 3072,
    THEME_WALLPAPER_LONG_SIDE = 320,
    THEME_WALLPAPER_SHORT_SIDE = 170,
    THEME_FONT_MAX_GLYPHS = 512,
    THEME_GLYPH_RECORD_BYTES = 10
};

typedef enum ThemeWallpaperKind {
    THEME_WALLPAPER_LANDSCAPE,
    THEME_WALLPAPER_PORTRAIT,
    THEME_WALLPAPER_COUNT
} ThemeWallpaperKind;

enum {
    THEME_ASSET_FIRST_FONT = THEME_WALLPAPER_COUNT,
    THEME_ASSET_SCENE = THEME_ASSET_FIRST_FONT + THEME_FONT_COUNT,
    THEME_ASSET_KINDS = THEME_ASSET_SCENE + 1,
    THEME_ASSET_HOME_SCENE = 8
};

typedef enum ThemeSceneId {
    THEME_SCENE_NOW_PLAYING,
    THEME_SCENE_HOME,
    THEME_SCENE_COUNT
} ThemeSceneId;

typedef struct ThemePackageFont {
    uint32_t offset;
    uint32_t checksum;
    uint16_t glyph_count;
    uint16_t bitmap_bytes;
    uint8_t ascent;
    uint8_t descent;
} ThemePackageFont;

typedef struct ThemePackageWallpaper {
    uint32_t offset;
    uint32_t checksum;
    uint16_t width;
    uint16_t height;
} ThemePackageWallpaper;

typedef struct ThemePackageIcon {
    uint16_t offset;
    uint8_t width;
    uint8_t height;
} ThemePackageIcon;

typedef struct ThemePackageScene {
    uint32_t offset;
    uint32_t checksum;
    uint16_t code_bytes;
    uint16_t string_bytes;
    uint8_t flags;
} ThemePackageScene;

typedef struct ThemeAccent {
    Color background;
    Color top;
    Color shine;
    uint8_t name;
} ThemeAccent;

typedef struct ThemePackage {
    ThemeColors colors;
    ThemeBoxes boxes;
    uint8_t selection;
    uint8_t spacing[THEME_SPACING_COUNT];
    char name[THEME_PACKAGE_NAME_BYTES];
    ThemePackageIcon icons[THEME_ICON_COUNT];
    ThemePackageWallpaper wallpapers[THEME_WALLPAPER_COUNT];
    ThemePackageFont fonts[THEME_FONT_COUNT];
    ThemeLayout layouts[THEME_WALLPAPER_COUNT];
    ThemePackageScene scene;
    ThemePackageScene home_scene;
    ThemeAccent accents[THEME_PACKAGE_MOST_ACCENTS];
    uint16_t accents_at;
    uint8_t accent_count;
    uint8_t flags;
} ThemePackage;

enum {
    THEME_PACKAGE_SPACING_UNSET = 0
};

bool theme_package_decode(const uint8_t *bytes, uint32_t size, ThemePackage *package);
