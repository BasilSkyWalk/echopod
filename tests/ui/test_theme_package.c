#include <stdio.h>
#include <string.h>

#include "check.h"
#include "core/hash.h"
#include "ui/theme_package.h"
#include "ui_test_theme.h"

enum {
    BUFFER_BYTES = 1024,
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
        TEST_CASE(a_file_bigger_than_a_theme_may_be_is_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
