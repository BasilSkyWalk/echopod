#include "ui/theme_package.h"

#include "core/hash.h"

enum {
    VERSION_FIELD = 4,
    HEADER_BYTES_FIELD = 6,
    TOTAL_BYTES_FIELD = 8,
    CHECKSUM_FIELD = 12,
    CHECKED_FROM = 16,
    COLOR_BYTES = 2
};

static const uint8_t MAGIC[] = {'E', 'T', 'H', 'M'};

static const uint8_t SPACING_RANGES[THEME_SPACING_COUNT][2] = {
#define THEME_SPACING(id, name, least, most) {least, most},
#include "ui/theme_spacing.def"
#undef THEME_SPACING
};

static uint32_t u16_at(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)bytes[offset] | (uint32_t)bytes[offset + 1] << 8;
}

static uint32_t u32_at(const uint8_t *bytes, uint32_t offset)
{
    return u16_at(bytes, offset) | u16_at(bytes, offset + 2) << 16;
}

static bool header_valid(const uint8_t *bytes, uint32_t size)
{
    if (size < THEME_PACKAGE_HEADER_BYTES || size > THEME_PACKAGE_MAX_BYTES) {
        return false;
    }
    for (uint32_t index = 0; index < sizeof MAGIC; index++) {
        if (bytes[index] != MAGIC[index]) {
            return false;
        }
    }
    return u16_at(bytes, VERSION_FIELD) == THEME_PACKAGE_VERSION &&
           u16_at(bytes, HEADER_BYTES_FIELD) >= THEME_PACKAGE_HEADER_BYTES &&
           u32_at(bytes, TOTAL_BYTES_FIELD) == size &&
           u32_at(bytes, CHECKSUM_FIELD) ==
               hash_update(HASH_START, bytes + CHECKED_FROM, size - CHECKED_FROM);
}

static bool box_usable(const BoxStyle *box)
{
    return box->fill_top < THEME_COLOR_COUNT && box->fill_bottom < THEME_COLOR_COUNT &&
           box->border < THEME_COLOR_COUNT && box->shine < THEME_COLOR_COUNT &&
           box->ink < THEME_COLOR_COUNT && box->border_edges <= BOX_EDGE_ALL;
}

static void take_colors(const uint8_t *bytes, uint32_t count, ThemeColors *colors)
{
    Color *table = (Color *)colors;
    uint32_t known = count < THEME_COLOR_COUNT ? count : THEME_COLOR_COUNT;
    for (uint32_t index = 0; index < known; index++) {
        table[index] = (Color)u16_at(bytes, index * COLOR_BYTES);
    }
}

static void take_box(const uint8_t *bytes, uint32_t box_bytes, BoxStyle *box)
{
    BoxStyle taken = *box;
    uint8_t *fields = (uint8_t *)&taken;
    uint32_t known = box_bytes < sizeof taken ? box_bytes : sizeof taken;
    for (uint32_t index = 0; index < known; index++) {
        fields[index] = bytes[index];
    }
    if (box_usable(&taken)) {
        *box = taken;
    }
}

static void take_boxes(const uint8_t *bytes, uint32_t count, uint32_t box_bytes, ThemeBoxes *boxes)
{
    BoxStyle *table = (BoxStyle *)boxes;
    uint32_t known = count < THEME_BOX_COUNT ? count : THEME_BOX_COUNT;
    for (uint32_t index = 0; index < known; index++) {
        take_box(bytes + index * box_bytes, box_bytes, &table[index]);
    }
}

static void take_spacing(const uint8_t *bytes, uint32_t count, uint8_t *spacing)
{
    for (uint32_t index = 0; index < THEME_SPACING_COUNT; index++) {
        uint32_t stored = index < count ? bytes[index] : THEME_PACKAGE_SPACING_UNSET;
        bool usable = stored > SPACING_RANGES[index][0] && stored <= SPACING_RANGES[index][1] + 1u;
        spacing[index] = usable ? (uint8_t)stored : THEME_PACKAGE_SPACING_UNSET;
    }
}

static void take_name(const uint8_t *bytes, char *name)
{
    for (uint32_t index = 0; index + 1 < THEME_PACKAGE_NAME_BYTES; index++) {
        name[index] = (char)bytes[index];
    }
    name[THEME_PACKAGE_NAME_BYTES - 1] = '\0';
}

bool theme_package_decode(const uint8_t *bytes, uint32_t size, ThemePackage *package)
{
    if (!header_valid(bytes, size)) {
        return false;
    }
    uint32_t header = u16_at(bytes, HEADER_BYTES_FIELD);
    uint32_t colors = u16_at(bytes, THEME_PACKAGE_COLORS_FIELD);
    uint32_t boxes = u16_at(bytes, THEME_PACKAGE_BOXES_FIELD);
    uint32_t box_bytes = bytes[THEME_PACKAGE_BOX_BYTES_FIELD];
    uint32_t spacing = bytes[THEME_PACKAGE_SPACING_FIELD];
    uint32_t boxes_at = header + colors * COLOR_BYTES;
    if (header > size || boxes_at > size || boxes * box_bytes > size - boxes_at ||
        spacing > size - boxes_at - boxes * box_bytes) {
        return false;
    }
    take_colors(bytes + header, colors, &package->colors);
    take_boxes(bytes + boxes_at, boxes, box_bytes, &package->boxes);
    take_spacing(bytes + boxes_at + boxes * box_bytes, spacing, package->spacing);
    take_name(bytes + THEME_PACKAGE_NAME_FIELD, package->name);
    uint8_t selection = bytes[THEME_PACKAGE_SELECTION_FIELD];
    package->selection = selection < THEME_BOX_COUNT ? selection : THEME_BOX_SELECTION;
    return true;
}
