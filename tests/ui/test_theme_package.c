#include <stdio.h>
#include <string.h>

#include "check.h"
#include "core/hash.h"
#include "ui/theme_package.h"
#include "ui_test_theme.h"

enum {
    BUFFER_BYTES = THEME_PACKAGE_MAX_BYTES + 16,
    BOX_BYTES = 10,
    TOTAL_FIELD = 8,
    CHECKSUM_FIELD = 12,
    CHECKED_FROM = 16
};

typedef struct Built {
    uint8_t bytes[BUFFER_BYTES];
    uint32_t size;
} Built;

static void put_u16(uint8_t *bytes, uint32_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *bytes, uint32_t offset, uint32_t value)
{
    put_u16(bytes, offset, value & 0xFFFF);
    put_u16(bytes, offset + 2, value >> 16);
}

static void seal(Built *built)
{
    put_u32(built->bytes, TOTAL_FIELD, built->size);
    put_u32(built->bytes, CHECKSUM_FIELD,
            hash_update(HASH_START, built->bytes + CHECKED_FROM, built->size - CHECKED_FROM));
}

static Built built_with(uint32_t colors, uint32_t boxes, uint32_t box_bytes)
{
    Built built = {.size = THEME_PACKAGE_HEADER_BYTES + 2 * colors + boxes * box_bytes};
    memcpy(built.bytes, "ETHM", 4);
    put_u16(built.bytes, 4, THEME_PACKAGE_VERSION);
    put_u16(built.bytes, 6, THEME_PACKAGE_HEADER_BYTES);
    put_u16(built.bytes, THEME_PACKAGE_COLORS_FIELD, colors);
    put_u16(built.bytes, THEME_PACKAGE_BOXES_FIELD, boxes);
    built.bytes[THEME_PACKAGE_BOX_BYTES_FIELD] = (uint8_t)box_bytes;
    memcpy(built.bytes + THEME_PACKAGE_NAME_FIELD, "Test", 5);
    seal(&built);
    return built;
}

static ThemePackage defaults(void)
{
    return (ThemePackage){.colors = TEST_COLORS, .boxes = THEME_STANDARD_BOXES};
}

static Built fixture(const char *name)
{
    Built built = {0};
    char path[512];
    snprintf(path, sizeof path, "%s/themes/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    if (file != NULL) {
        built.size = (uint32_t)fread(built.bytes, 1, sizeof built.bytes, file);
        fclose(file);
    }
    return built;
}

static void the_packed_dark_theme_holds_the_standard_boxes(void)
{
    Built dark = fixture("dark.echotheme");
    ThemePackage package = {0};
    CHECK(theme_package_decode(dark.bytes, dark.size, &package));
    CHECK(memcmp(&package.boxes, &THEME_STANDARD_BOXES, sizeof package.boxes) == 0);
    CHECK_EQUAL(THEME_BOX_SELECTION, package.selection);
    CHECK(strcmp("Dark", package.name) == 0);
}

static void a_packed_theme_brings_its_name_colours_and_boxes(void)
{
    Built ember = fixture("ember.echotheme");
    ThemePackage package = defaults();
    CHECK(theme_package_decode(ember.bytes, ember.size, &package));
    CHECK(strcmp("Ember", package.name) == 0);
    CHECK_EQUAL(COLOR_RGB(0x18, 0x12, 0x10), package.colors.background);
    CHECK_EQUAL(COLOR_RGB(0xf8, 0xa0, 0x30), package.colors.selection_top);
    CHECK_EQUAL(THEME_COLOR_SELECTION_TOP, package.boxes.selection.border);
    CHECK_EQUAL(3, package.boxes.selection.border_width);
    CHECK_EQUAL(BOX_EDGE_LEFT, package.boxes.selection.border_edges);
    CHECK_EQUAL(0, package.boxes.selection.shine_height);
}

static void colours_a_file_does_not_have_keep_their_defaults(void)
{
    Built built = built_with(2, 0, BOX_BYTES);
    put_u16(built.bytes, THEME_PACKAGE_HEADER_BYTES, 0x1234);
    put_u16(built.bytes, THEME_PACKAGE_HEADER_BYTES + 2, 0x5678);
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(0x1234, package.colors.background);
    CHECK_EQUAL(0x5678, package.colors.text);
    CHECK_EQUAL(TEST_COLORS.text_secondary, package.colors.text_secondary);
    CHECK_EQUAL(TEST_COLORS.chevron, package.colors.chevron);
}

static void colours_and_boxes_past_the_ones_known_are_ignored(void)
{
    Built built = built_with(THEME_COLOR_COUNT + 3, THEME_BOX_COUNT + 2, BOX_BYTES);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(0, package.colors.chevron);
    CHECK_EQUAL(0, package.boxes.progress_active.fill_top);
    CHECK(memcmp(package.name, "Test", 5) == 0);
}

static void a_box_that_names_an_unknown_colour_keeps_its_default(void)
{
    Built built = built_with(0, 2, BOX_BYTES);
    uint8_t *boxes = built.bytes + THEME_PACKAGE_HEADER_BYTES;
    boxes[0] = THEME_COLOR_COUNT;
    boxes[BOX_BYTES] = THEME_COLOR_DANGER;
    boxes[BOX_BYTES + 1] = THEME_COLOR_DANGER;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK(memcmp(&package.boxes.selection, &THEME_STANDARD_BOXES.selection, sizeof(BoxStyle)) == 0);
    CHECK_EQUAL(THEME_COLOR_DANGER, package.boxes.selection_flat.fill_top);
}

static void a_box_with_unknown_edges_keeps_its_default(void)
{
    Built built = built_with(0, 1, BOX_BYTES);
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 6] = BOX_EDGE_ALL + 1;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK(memcmp(&package.boxes.selection, &THEME_STANDARD_BOXES.selection, sizeof(BoxStyle)) == 0);
}

static void a_shorter_box_keeps_the_default_of_the_fields_it_lacks(void)
{
    Built built = built_with(0, 1, 2);
    built.bytes[THEME_PACKAGE_HEADER_BYTES] = THEME_COLOR_DANGER;
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 1] = THEME_COLOR_SUCCESS;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(THEME_COLOR_DANGER, package.boxes.selection.fill_top);
    CHECK_EQUAL(THEME_COLOR_SUCCESS, package.boxes.selection.fill_bottom);
    CHECK_EQUAL(THEME_STANDARD_BOXES.selection.shine_height, package.boxes.selection.shine_height);
}

static void an_unknown_selection_box_falls_back_to_the_first(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    built.bytes[THEME_PACKAGE_SELECTION_FIELD] = THEME_BOX_SELECTION_OUTLINE;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(THEME_BOX_SELECTION_OUTLINE, package.selection);
    built.bytes[THEME_PACKAGE_SELECTION_FIELD] = THEME_BOX_COUNT;
    seal(&built);
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(THEME_BOX_SELECTION, package.selection);
}

static bool decodes(const Built *built)
{
    ThemePackage package = defaults();
    return theme_package_decode(built->bytes, built->size, &package);
}

static void a_damaged_or_foreign_file_is_refused(void)
{
    Built good = built_with(4, 2, BOX_BYTES);
    CHECK(decodes(&good));
    Built magic = good;
    magic.bytes[0] = 'X';
    CHECK(!decodes(&magic));
    Built version = good;
    put_u16(version.bytes, 4, THEME_PACKAGE_VERSION + 1);
    seal(&version);
    CHECK(!decodes(&version));
    Built flipped = good;
    flipped.bytes[THEME_PACKAGE_HEADER_BYTES] ^= 1;
    CHECK(!decodes(&flipped));
    Built cut = good;
    cut.size -= 1;
    CHECK(!decodes(&cut));
    Built tiny = good;
    tiny.size = THEME_PACKAGE_HEADER_BYTES - 1;
    CHECK(!decodes(&tiny));
}

static void counts_that_pass_the_end_of_the_file_are_refused(void)
{
    Built colors = built_with(4, 0, BOX_BYTES);
    put_u16(colors.bytes, THEME_PACKAGE_COLORS_FIELD, 5);
    seal(&colors);
    CHECK(!decodes(&colors));
    Built boxes = built_with(0, 2, BOX_BYTES);
    put_u16(boxes.bytes, THEME_PACKAGE_BOXES_FIELD, 3);
    seal(&boxes);
    CHECK(!decodes(&boxes));
    Built huge = built_with(0, 0, BOX_BYTES);
    put_u16(huge.bytes, THEME_PACKAGE_COLORS_FIELD, 0xFFFF);
    put_u16(huge.bytes, THEME_PACKAGE_BOXES_FIELD, 0xFFFF);
    huge.bytes[THEME_PACKAGE_BOX_BYTES_FIELD] = 0xFF;
    seal(&huge);
    CHECK(!decodes(&huge));
}

static void spacing_inside_its_range_is_taken_and_the_rest_stays_unset(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    built.bytes[THEME_PACKAGE_SPACING_FIELD] = 3;
    built.size += 3;
    built.bytes[THEME_PACKAGE_HEADER_BYTES] = 28 + 1;
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 1] = 2 + 1;
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 2] = 0;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(29, package.spacing[THEME_SPACING_ROW_HEIGHT]);
    CHECK_EQUAL(THEME_PACKAGE_SPACING_UNSET, package.spacing[THEME_SPACING_PADDING]);
    CHECK_EQUAL(THEME_PACKAGE_SPACING_UNSET, package.spacing[THEME_SPACING_SCROLLBAR_WIDTH]);
    CHECK_EQUAL(THEME_PACKAGE_SPACING_UNSET, package.spacing[THEME_SPACING_DETAIL_GAP]);
}

static void the_ends_of_a_spacing_range_are_inside_it(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    built.bytes[THEME_PACKAGE_SPACING_FIELD] = 4;
    built.size += 4;
    built.bytes[THEME_PACKAGE_HEADER_BYTES] = 18 + 1;
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 1] = 20 + 1;
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 2] = 8 + 2;
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 3] = 0 + 1;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(19, package.spacing[THEME_SPACING_ROW_HEIGHT]);
    CHECK_EQUAL(21, package.spacing[THEME_SPACING_PADDING]);
    CHECK_EQUAL(THEME_PACKAGE_SPACING_UNSET, package.spacing[THEME_SPACING_SCROLLBAR_WIDTH]);
    CHECK_EQUAL(1, package.spacing[THEME_SPACING_SCROLLBAR_INSET]);
}

static void spacing_that_passes_the_end_of_the_file_is_refused(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    built.bytes[THEME_PACKAGE_SPACING_FIELD] = 2;
    built.size += 1;
    seal(&built);
    CHECK(!decodes(&built));
}

static void a_box_brings_its_corner_radius(void)
{
    Built built = built_with(0, 1, sizeof(BoxStyle));
    built.bytes[THEME_PACKAGE_HEADER_BYTES + 10] = 6;
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(6, package.boxes.selection.radius);
    CHECK_EQUAL(0, package.boxes.header.radius);
}

enum {
    ICON_TABLE = THEME_PACKAGE_HEADER_BYTES,
    THREE_ICONS = 3,
    ICON_DATA = ICON_TABLE + THREE_ICONS * THEME_PACKAGE_ICON_ENTRY_BYTES,
    ICON_DATA_BYTES = 8
};

static void put_icon(Built *built, uint32_t id, const uint8_t entry[3])
{
    uint8_t *field = built->bytes + ICON_TABLE + id * THEME_PACKAGE_ICON_ENTRY_BYTES;
    field[0] = entry[0];
    field[1] = entry[1];
    put_u16(field, 2, entry[2]);
}

static Built built_with_icons(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    built.bytes[THEME_PACKAGE_ICONS_FIELD] = THREE_ICONS;
    built.size = ICON_DATA + ICON_DATA_BYTES;
    put_icon(&built, THEME_ICON_STATUS_PLAY, (const uint8_t[]){3, 2, 0});
    put_icon(&built, THEME_ICON_STATUS_BLUETOOTH, (const uint8_t[]){7, 1, 4});
    seal(&built);
    return built;
}

static void a_file_brings_icons_with_their_sizes_and_where_their_levels_lie(void)
{
    Built built = built_with_icons();
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(3, package.icons[THEME_ICON_STATUS_PLAY].width);
    CHECK_EQUAL(2, package.icons[THEME_ICON_STATUS_PLAY].height);
    CHECK_EQUAL(ICON_DATA, package.icons[THEME_ICON_STATUS_PLAY].offset);
    CHECK_EQUAL(0, package.icons[THEME_ICON_STATUS_PAUSE].width);
    CHECK_EQUAL(7, package.icons[THEME_ICON_STATUS_BLUETOOTH].width);
    CHECK_EQUAL(ICON_DATA + 4, package.icons[THEME_ICON_STATUS_BLUETOOTH].offset);
    CHECK_EQUAL(0, package.icons[THEME_ICON_STATUS_BOLT].width);
    CHECK_EQUAL(0, package.icons[THEME_ICON_COUNT - 1].width);
}

static void a_file_without_icons_clears_the_ones_a_package_held(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    ThemePackage package = defaults();
    package.icons[THEME_ICON_CHEVRON] = (ThemePackageIcon){60, 4, 4};
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(0, package.icons[THEME_ICON_CHEVRON].width);
}

static void an_icon_too_big_for_its_place_or_past_the_end_is_left_out(void)
{
    Built built = built_with_icons();
    put_icon(&built, THEME_ICON_STATUS_PLAY, (const uint8_t[]){11, 2, 0});
    put_icon(&built, THEME_ICON_STATUS_PAUSE, (const uint8_t[]){2, 10, 0});
    put_icon(&built, THEME_ICON_STATUS_BLUETOOTH, (const uint8_t[]){7, 1, 5});
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(0, package.icons[THEME_ICON_STATUS_PLAY].width);
    CHECK_EQUAL(0, package.icons[THEME_ICON_STATUS_PAUSE].width);
    CHECK_EQUAL(0, package.icons[THEME_ICON_STATUS_BLUETOOTH].width);
    put_icon(&built, THEME_ICON_STATUS_PAUSE, (const uint8_t[]){2, 8, 0});
    seal(&built);
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(8, package.icons[THEME_ICON_STATUS_PAUSE].height);
}

static void an_icon_table_that_passes_the_end_of_the_file_is_refused(void)
{
    Built built = built_with_icons();
    built.size = ICON_DATA - 1;
    seal(&built);
    CHECK(!decodes(&built));
}

static void icons_past_the_ones_known_are_ignored(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    built.bytes[THEME_PACKAGE_ICONS_FIELD] = THEME_ICON_COUNT + 2;
    built.size += (THEME_ICON_COUNT + 2) * THEME_PACKAGE_ICON_ENTRY_BYTES + 2;
    put_icon(&built, THEME_ICON_COUNT - 1, (const uint8_t[]){4, 1, 0});
    put_icon(&built, THEME_ICON_COUNT, (const uint8_t[]){200, 200, 0});
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(4, package.icons[THEME_ICON_COUNT - 1].width);
}

static void the_packed_sample_brings_a_battery_and_its_level_at_one_size(void)
{
    Built ember = fixture("ember.echotheme");
    Built dark = fixture("dark.echotheme");
    ThemePackage package = defaults();
    CHECK(theme_package_decode(ember.bytes, ember.size, &package));
    CHECK_EQUAL(22, package.icons[THEME_ICON_BATTERY].width);
    CHECK_EQUAL(10, package.icons[THEME_ICON_BATTERY].height);
    CHECK_EQUAL(22, package.icons[THEME_ICON_BATTERY_LEVEL].width);
    CHECK_EQUAL(30, package.icons[THEME_ICON_BATTERY_LEVEL_LARGE].width);
    uint32_t checked = (uint32_t)ember.bytes[TOTAL_FIELD] | (uint32_t)ember.bytes[TOTAL_FIELD + 1]
                                                                << 8;
    CHECK(package.icons[THEME_ICON_BATTERY_LEVEL_LARGE].offset + 15u * 14u == checked);
    CHECK(theme_package_decode(dark.bytes, dark.size, &package));
    CHECK_EQUAL(0, package.icons[THEME_ICON_BATTERY].width);
}

enum {
    WALL_TABLE = THEME_PACKAGE_ASSET_HEADER_BYTES,
    WALL_CORE = WALL_TABLE + 2 * THEME_PACKAGE_ASSET_ENTRY_BYTES,
    WALL_TRAILING = 40
};

typedef struct WallEntry {
    uint8_t kind;
    uint16_t width;
    uint16_t height;
    uint32_t offset;
} WallEntry;

static void seal_core(Built *built, uint32_t core)
{
    put_u32(built->bytes, TOTAL_FIELD, core);
    put_u32(built->bytes, CHECKSUM_FIELD,
            hash_update(HASH_START, built->bytes + CHECKED_FROM, core - CHECKED_FROM));
}

static void put_wall(Built *built, int index, WallEntry entry)
{
    uint8_t *field = built->bytes + WALL_TABLE + index * THEME_PACKAGE_ASSET_ENTRY_BYTES;
    field[0] = entry.kind;
    put_u16(field, 2, entry.width);
    put_u16(field, 4, entry.height);
    put_u32(field, 8, entry.offset);
    put_u32(field, 12, 0xC0FFEE00u + (uint32_t)index);
    seal_core(built, WALL_CORE);
}

static Built built_with_wallpapers(void)
{
    Built built = built_with(0, 0, BOX_BYTES);
    put_u16(built.bytes, 6, THEME_PACKAGE_ASSET_HEADER_BYTES);
    built.bytes[THEME_PACKAGE_ASSETS_FIELD] = 2;
    built.size = WALL_CORE + WALL_TRAILING;
    put_wall(&built, 0, (WallEntry){THEME_WALLPAPER_LANDSCAPE, 320, 170, WALL_CORE});
    put_wall(&built, 1, (WallEntry){THEME_WALLPAPER_PORTRAIT, 16, 320, WALL_CORE + 8});
    return built;
}

static void a_file_says_where_its_wallpapers_lie_past_the_checked_part(void)
{
    Built built = built_with_wallpapers();
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    ThemePackageWallpaper wide = package.wallpapers[THEME_WALLPAPER_LANDSCAPE];
    ThemePackageWallpaper tall = package.wallpapers[THEME_WALLPAPER_PORTRAIT];
    CHECK_EQUAL(320, wide.width);
    CHECK_EQUAL(170, wide.height);
    CHECK_EQUAL(WALL_CORE, wide.offset);
    CHECK_EQUAL(0xC0FFEE00u, wide.checksum);
    CHECK_EQUAL(16, tall.width);
    CHECK_EQUAL(320, tall.height);
    CHECK_EQUAL(WALL_CORE + 8, tall.offset);
    CHECK_EQUAL(0xC0FFEE01u, tall.checksum);
}

static void bytes_past_the_checked_part_do_not_spoil_the_check(void)
{
    Built built = built_with_wallpapers();
    built.bytes[WALL_CORE + 3] ^= 0xFF;
    CHECK(decodes(&built));
    built.bytes[WALL_CORE - 1] ^= 0xFF;
    CHECK(!decodes(&built));
}

static void a_wallpaper_that_is_misshapen_or_inside_the_checked_part_is_left_out(void)
{
    const WallEntry bad[] = {{THEME_WALLPAPER_LANDSCAPE, 322, 170, WALL_CORE},
                             {THEME_WALLPAPER_LANDSCAPE, 320, 171, WALL_CORE},
                             {THEME_WALLPAPER_LANDSCAPE, 15, 16, WALL_CORE},
                             {THEME_WALLPAPER_LANDSCAPE, 0, 16, WALL_CORE},
                             {THEME_WALLPAPER_LANDSCAPE, 16, 0, WALL_CORE},
                             {THEME_WALLPAPER_LANDSCAPE, 16, 16, WALL_CORE - 1},
                             {THEME_WALLPAPER_PORTRAIT, 172, 16, WALL_CORE},
                             {THEME_WALLPAPER_COUNT, 16, 16, WALL_CORE}};
    for (uint32_t index = 0; index < sizeof bad / sizeof bad[0]; index++) {
        Built built = built_with_wallpapers();
        put_wall(&built, 0, bad[index]);
        ThemePackage package = defaults();
        CHECK(theme_package_decode(built.bytes, built.size, &package));
        CHECK_EQUAL(0, package.wallpapers[THEME_WALLPAPER_LANDSCAPE].width);
        CHECK_EQUAL(16, package.wallpapers[THEME_WALLPAPER_PORTRAIT].width);
    }
}

static void a_wallpaper_table_that_passes_the_checked_part_is_refused(void)
{
    Built built = built_with_wallpapers();
    seal_core(&built, WALL_CORE - 1);
    CHECK(!decodes(&built));
}

static void a_file_with_the_short_header_has_no_wallpapers(void)
{
    Built built = built_with(2, 0, BOX_BYTES);
    ThemePackage package = defaults();
    package.wallpapers[THEME_WALLPAPER_PORTRAIT].width = 16;
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(0, package.wallpapers[THEME_WALLPAPER_PORTRAIT].width);
}

static void the_packed_sample_brings_a_small_wallpaper_to_repeat(void)
{
    Built ember = fixture("ember.echotheme");
    ThemePackage package = defaults();
    CHECK(theme_package_decode(ember.bytes, ember.size, &package));
    ThemePackageWallpaper tile = package.wallpapers[THEME_WALLPAPER_LANDSCAPE];
    CHECK_EQUAL(16, tile.width);
    CHECK_EQUAL(16, tile.height);
    CHECK_EQUAL(ember.size, tile.offset + THEME_PACKAGE_PALETTE_BYTES + 8u * 16u);
    CHECK_EQUAL(0, package.wallpapers[THEME_WALLPAPER_PORTRAIT].width);
    CHECK_EQUAL(22, package.icons[THEME_ICON_BATTERY].width);
}

typedef struct FontEntry {
    uint8_t kind;
    uint8_t ascent;
    uint8_t descent;
    uint16_t glyphs;
    uint32_t offset;
} FontEntry;

static Built built_with_font(FontEntry font)
{
    Built built = built_with_wallpapers();
    uint8_t *field = built.bytes + WALL_TABLE + THEME_PACKAGE_ASSET_ENTRY_BYTES;
    field[0] = font.kind;
    field[1] = font.ascent;
    put_u16(field, 2, font.glyphs);
    put_u16(field, 4, 300);
    field[6] = font.descent;
    field[7] = 0;
    put_u32(field, 8, font.offset);
    put_u32(field, 12, 0xF0F0F0F0u);
    seal_core(&built, WALL_CORE);
    return built;
}

static void a_file_says_where_its_fonts_lie_and_how_they_measure(void)
{
    Built built = built_with_font(
        (FontEntry){THEME_ASSET_FIRST_FONT + THEME_FONT_BODY, 15, 5, 95, WALL_CORE + 4});
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    ThemePackageFont body = package.fonts[THEME_FONT_BODY];
    CHECK_EQUAL(WALL_CORE + 4, body.offset);
    CHECK_EQUAL(0xF0F0F0F0u, body.checksum);
    CHECK_EQUAL(95, body.glyph_count);
    CHECK_EQUAL(300, body.bitmap_bytes);
    CHECK_EQUAL(15, body.ascent);
    CHECK_EQUAL(5, body.descent);
    CHECK_EQUAL(0, package.fonts[THEME_FONT_TITLE].glyph_count);
    CHECK_EQUAL(320, package.wallpapers[THEME_WALLPAPER_LANDSCAPE].width);
    CHECK_EQUAL(0, package.wallpapers[THEME_WALLPAPER_PORTRAIT].width);
}

static void a_font_too_tall_for_its_text_or_misshapen_is_left_out(void)
{
    const uint8_t body = THEME_ASSET_FIRST_FONT + THEME_FONT_BODY;
    const uint8_t small = THEME_ASSET_FIRST_FONT + THEME_FONT_SMALL;
    const FontEntry bad[] = {{body, 16, 5, 95, WALL_CORE},
                             {small, 12, 4, 95, WALL_CORE},
                             {body, 0, 4, 95, WALL_CORE},
                             {body, 16, 4, 0, WALL_CORE},
                             {body, 16, 4, THEME_FONT_MAX_GLYPHS + 1, WALL_CORE},
                             {body, 16, 4, 95, WALL_CORE - 1},
                             {THEME_ASSET_KINDS, 16, 4, 95, WALL_CORE}};
    for (uint32_t index = 0; index < sizeof bad / sizeof bad[0]; index++) {
        Built built = built_with_font(bad[index]);
        ThemePackage package = defaults();
        CHECK(theme_package_decode(built.bytes, built.size, &package));
        for (int font = 0; font < THEME_FONT_COUNT; font++) {
            CHECK_EQUAL(0, package.fonts[font].glyph_count);
        }
    }
    Built small_enough = built_with_font((FontEntry){small, 12, 3, 95, WALL_CORE});
    ThemePackage package = defaults();
    CHECK(theme_package_decode(small_enough.bytes, small_enough.size, &package));
    CHECK_EQUAL(95, package.fonts[THEME_FONT_SMALL].glyph_count);
}

static void a_file_without_fonts_clears_the_ones_a_package_held(void)
{
    Built built = built_with_wallpapers();
    ThemePackage package = defaults();
    package.fonts[THEME_FONT_SMALL].glyph_count = 9;
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    CHECK_EQUAL(0, package.fonts[THEME_FONT_SMALL].glyph_count);
}

enum {
    LAYOUT_AT = THEME_PACKAGE_ASSET_HEADER_BYTES,
    LAYOUT_BOX = THEME_PACKAGE_LAYOUT_BOX_BYTES
};

static void put_place(Built *built, uint32_t index, ThemeLayoutBox box)
{
    uint8_t *field = built->bytes + LAYOUT_AT + index * LAYOUT_BOX;
    put_u16(field, 0, (uint16_t)box.x);
    put_u16(field, 2, (uint16_t)box.y);
    put_u16(field, 4, box.width);
    field[6] = box.height;
    field[7] = box.align;
}

static Built built_with_places(uint8_t wide, uint8_t tall)
{
    Built built = built_with(0, 0, BOX_BYTES);
    put_u16(built.bytes, 6, THEME_PACKAGE_ASSET_HEADER_BYTES);
    built.bytes[THEME_PACKAGE_LAYOUTS_FIELD] = wide;
    built.bytes[THEME_PACKAGE_LAYOUTS_FIELD + 1] = tall;
    built.size = LAYOUT_AT + (wide + tall) * (uint32_t)LAYOUT_BOX;
    for (uint32_t index = 0; index < (uint32_t)(wide + tall); index++) {
        put_place(&built, index, (ThemeLayoutBox){.align = THEME_ALIGN_UNSET});
    }
    return built;
}

static void a_file_places_now_playing_for_each_way_the_screen_is_held(void)
{
    Built built = built_with_places(THEME_LAYOUT_COUNT, 2);
    put_place(&built, THEME_LAYOUT_COVER, (ThemeLayoutBox){-4, 30, 64, 0, THEME_ALIGN_LEFT});
    put_place(&built, THEME_LAYOUT_TITLE, (ThemeLayoutBox){10, 96, 300, 22, THEME_ALIGN_RIGHT});
    put_place(&built, THEME_LAYOUT_COUNT + 1, (ThemeLayoutBox){0, 0, 0, 22, THEME_ALIGN_LEFT});
    seal(&built);
    ThemePackage package = defaults();
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    const ThemeLayout *wide = &package.layouts[THEME_WALLPAPER_LANDSCAPE];
    const ThemeLayout *tall = &package.layouts[THEME_WALLPAPER_PORTRAIT];
    CHECK_EQUAL(-4, wide->boxes[THEME_LAYOUT_COVER].x);
    CHECK_EQUAL(64, wide->boxes[THEME_LAYOUT_COVER].width);
    CHECK_EQUAL(96, wide->boxes[THEME_LAYOUT_TITLE].y);
    CHECK_EQUAL(THEME_ALIGN_RIGHT, wide->boxes[THEME_LAYOUT_TITLE].align);
    CHECK_EQUAL(THEME_ALIGN_UNSET, wide->boxes[THEME_LAYOUT_LYRICS].align);
    CHECK_EQUAL(THEME_ALIGN_UNSET, tall->boxes[THEME_LAYOUT_COVER].align);
    CHECK_EQUAL(THEME_ALIGN_LEFT, tall->boxes[THEME_LAYOUT_TITLE].align);
    CHECK_EQUAL(0, tall->boxes[THEME_LAYOUT_TITLE].width);
    CHECK_EQUAL(THEME_ALIGN_UNSET, tall->boxes[THEME_LAYOUT_ARTIST].align);
}

static void a_place_out_of_reach_or_oddly_aligned_is_left_to_the_player(void)
{
    const ThemeLayoutBox bad[] = {{321, 0, 10, 10, THEME_ALIGN_LEFT},
                                  {0, -321, 10, 10, THEME_ALIGN_LEFT},
                                  {0, 0, 321, 10, THEME_ALIGN_LEFT},
                                  {0, 0, 10, 10, THEME_ALIGN_COUNT}};
    for (uint32_t index = 0; index < sizeof bad / sizeof bad[0]; index++) {
        Built built = built_with_places(THEME_LAYOUT_COUNT, 0);
        put_place(&built, THEME_LAYOUT_TITLE, bad[index]);
        put_place(&built, THEME_LAYOUT_COVER, (ThemeLayoutBox){0, 0, 171, 0, THEME_ALIGN_LEFT});
        seal(&built);
        ThemePackage package = defaults();
        CHECK(theme_package_decode(built.bytes, built.size, &package));
        const ThemeLayout *wide = &package.layouts[THEME_WALLPAPER_LANDSCAPE];
        CHECK_EQUAL(THEME_ALIGN_UNSET, wide->boxes[THEME_LAYOUT_TITLE].align);
        CHECK_EQUAL(THEME_ALIGN_UNSET, wide->boxes[THEME_LAYOUT_COVER].align);
    }
}

static void places_that_pass_the_end_of_the_file_are_refused(void)
{
    Built built = built_with_places(THEME_LAYOUT_COUNT, 0);
    built.bytes[THEME_PACKAGE_LAYOUTS_FIELD + 1] = 1;
    seal(&built);
    CHECK(!decodes(&built));
}

static void a_file_without_places_leaves_every_place_to_the_player(void)
{
    Built built = built_with(2, 0, BOX_BYTES);
    ThemePackage package = defaults();
    package.layouts[THEME_WALLPAPER_PORTRAIT].boxes[THEME_LAYOUT_TIMES].align = THEME_ALIGN_LEFT;
    CHECK(theme_package_decode(built.bytes, built.size, &package));
    for (int side = 0; side < THEME_WALLPAPER_COUNT; side++) {
        for (int id = 0; id < THEME_LAYOUT_COUNT; id++) {
            CHECK_EQUAL(THEME_ALIGN_UNSET, package.layouts[side].boxes[id].align);
        }
    }
}

static void a_file_bigger_than_a_theme_may_be_is_refused(void)
{
    Built big = built_with(THEME_COLOR_COUNT, THEME_BOX_COUNT, BOX_BYTES);
    big.size = THEME_PACKAGE_MAX_BYTES + 1;
    seal(&big);
    CHECK(!decodes(&big));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_packed_dark_theme_holds_the_standard_boxes),
        TEST_CASE(a_packed_theme_brings_its_name_colours_and_boxes),
        TEST_CASE(colours_a_file_does_not_have_keep_their_defaults),
        TEST_CASE(colours_and_boxes_past_the_ones_known_are_ignored),
        TEST_CASE(a_box_that_names_an_unknown_colour_keeps_its_default),
        TEST_CASE(a_box_with_unknown_edges_keeps_its_default),
        TEST_CASE(a_shorter_box_keeps_the_default_of_the_fields_it_lacks),
        TEST_CASE(an_unknown_selection_box_falls_back_to_the_first),
        TEST_CASE(a_damaged_or_foreign_file_is_refused),
        TEST_CASE(counts_that_pass_the_end_of_the_file_are_refused),
        TEST_CASE(spacing_inside_its_range_is_taken_and_the_rest_stays_unset),
        TEST_CASE(the_ends_of_a_spacing_range_are_inside_it),
        TEST_CASE(spacing_that_passes_the_end_of_the_file_is_refused),
        TEST_CASE(a_box_brings_its_corner_radius),
        TEST_CASE(a_file_brings_icons_with_their_sizes_and_where_their_levels_lie),
        TEST_CASE(a_file_without_icons_clears_the_ones_a_package_held),
        TEST_CASE(an_icon_too_big_for_its_place_or_past_the_end_is_left_out),
        TEST_CASE(an_icon_table_that_passes_the_end_of_the_file_is_refused),
        TEST_CASE(icons_past_the_ones_known_are_ignored),
        TEST_CASE(the_packed_sample_brings_a_battery_and_its_level_at_one_size),
        TEST_CASE(a_file_says_where_its_wallpapers_lie_past_the_checked_part),
        TEST_CASE(bytes_past_the_checked_part_do_not_spoil_the_check),
        TEST_CASE(a_wallpaper_that_is_misshapen_or_inside_the_checked_part_is_left_out),
        TEST_CASE(a_wallpaper_table_that_passes_the_checked_part_is_refused),
        TEST_CASE(a_file_with_the_short_header_has_no_wallpapers),
        TEST_CASE(the_packed_sample_brings_a_small_wallpaper_to_repeat),
        TEST_CASE(a_file_says_where_its_fonts_lie_and_how_they_measure),
        TEST_CASE(a_font_too_tall_for_its_text_or_misshapen_is_left_out),
        TEST_CASE(a_file_without_fonts_clears_the_ones_a_package_held),
        TEST_CASE(a_file_places_now_playing_for_each_way_the_screen_is_held),
        TEST_CASE(a_place_out_of_reach_or_oddly_aligned_is_left_to_the_player),
        TEST_CASE(places_that_pass_the_end_of_the_file_are_refused),
        TEST_CASE(a_file_without_places_leaves_every_place_to_the_player),
        TEST_CASE(a_file_bigger_than_a_theme_may_be_is_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
