#include "theme_fixture.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/hash.h"
#include "fake_hal.h"

enum {
    FIXTURE_BYTES = 3072,
    FIXTURE_SLOTS = 4,
    PATH_BYTES = 512
};

static uint8_t slots[FIXTURE_SLOTS][FIXTURE_BYTES];
static int next_slot;

static uint32_t read_fixture(const char *fixture, uint8_t *bytes)
{
    char path[PATH_BYTES];
    snprintf(path, sizeof path, "%s/themes/%s", ECHO_FIXTURE_DIR, fixture);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    uint32_t size = (uint32_t)fread(bytes, 1, FIXTURE_BYTES, file);
    fclose(file);
    return size;
}

uint8_t *theme_fixture_on_card(const char *fixture, const char *card_name, uint32_t *size)
{
    char path[PATH_BYTES];
    uint8_t *bytes = slots[next_slot++ % FIXTURE_SLOTS];
    *size = read_fixture(fixture, bytes);
    snprintf(path, sizeof path, "/Themes/%s", card_name);
    fake_hal_add_file(path, bytes, *size);
    fake_hal_add_directory_entry("/Themes", card_name, false, false);
    return bytes;
}

enum {
    LETTERED_HEADER = 52,
    LETTERED_ENTRY = 16,
    LETTERED_GLYPHS = 2,
    LETTERED_RECORD = 10,
    LETTERED_BITMAP = 10,
    LETTERED_SECTION = LETTERED_GLYPHS * LETTERED_RECORD + LETTERED_BITMAP,
    LETTERED_PAPER = 32 + 1,
    LETTERED_BYTES = LETTERED_HEADER + 2 * LETTERED_ENTRY + LETTERED_SECTION + LETTERED_PAPER,
    FIRST_FONT_KIND = 2,
    BODY_KIND = FIRST_FONT_KIND + 1
};

static uint8_t lettered[LETTERED_BYTES];

static void put_le(uint8_t *bytes, uint32_t value, int count)
{
    for (int index = 0; index < count; index++) {
        bytes[index] = (uint8_t)(value >> (8 * index));
    }
}

static void put_glyph(uint8_t *record, const uint8_t *fields, uint32_t codepoint, uint32_t offset)
{
    put_le(record, codepoint, 2);
    put_le(record + 2, offset, 2);
    memcpy(record + 4, fields, 5);
    record[9] = 0;
}

static uint32_t put_section(uint8_t *section, LetteredSpoil spoil)
{
    static const uint8_t SOLID[] = {4, 4, 0, 12, 6};
    static const uint8_t SMALL[] = {2, 2, 0, 6, 3};
    bool reversed = spoil == LETTERED_OUT_OF_ORDER;
    put_glyph(section, SOLID, reversed ? 'Z' : 'M', 0);
    put_glyph(section + LETTERED_RECORD, SMALL, reversed ? 'M' : 'Z', 8);
    memset(section + LETTERED_GLYPHS * LETTERED_RECORD, 0xFF, LETTERED_BITMAP);
    uint32_t check = hash_update(HASH_START, section, LETTERED_SECTION);
    section[LETTERED_SECTION - 1] ^= spoil == LETTERED_DAMAGED ? 0x0F : 0;
    return check;
}

static void put_entry(uint8_t *entry, const uint8_t *head, uint32_t offset, uint32_t check)
{
    memcpy(entry, head, 8);
    put_le(entry + 8, offset, 4);
    put_le(entry + 12, check, 4);
}

static uint32_t put_assets(uint8_t *bytes, LetteredSpoil spoil)
{
    bool papered = spoil == LETTERED_PAPERED;
    uint32_t core = LETTERED_HEADER + (papered ? 2u : 1u) * LETTERED_ENTRY;
    const uint8_t font_head[8] = {BODY_KIND, 16, LETTERED_GLYPHS, 0, LETTERED_BITMAP, 0, 4, 0};
    uint32_t check = put_section(bytes + core, spoil);
    put_entry(bytes + LETTERED_HEADER, font_head, core, check);
    if (!papered) {
        return core + LETTERED_SECTION;
    }
    uint8_t *paper = bytes + core + LETTERED_SECTION;
    memset(paper, 0, LETTERED_PAPER);
    put_le(paper, 0xFFFF, 2);
    paper[LETTERED_PAPER - 1] = 0x01;
    const uint8_t paper_head[8] = {0, 0, 2, 0, 1, 0, 0, 0};
    put_entry(bytes + LETTERED_HEADER + LETTERED_ENTRY, paper_head, core + LETTERED_SECTION,
              hash_update(HASH_START, paper, LETTERED_PAPER));
    return core + LETTERED_SECTION + LETTERED_PAPER;
}

uint8_t *theme_fixture_lettered(const char *card_name, LetteredSpoil spoil, uint32_t *size)
{
    char path[PATH_BYTES];
    memset(lettered, 0, sizeof lettered);
    memcpy(lettered, "ETHM", 4);
    put_le(lettered + 4, 1, 2);
    put_le(lettered + 6, LETTERED_HEADER, 2);
    lettered[20] = 11;
    memcpy(lettered + 24, "Lettered", 8);
    lettered[48] = spoil == LETTERED_PAPERED ? 2 : 1;
    *size = put_assets(lettered, spoil);
    uint32_t core = LETTERED_HEADER + lettered[48] * (uint32_t)LETTERED_ENTRY;
    put_le(lettered + 8, core, 4);
    put_le(lettered + 12, hash_update(HASH_START, lettered + 16, core - 16), 4);
    snprintf(path, sizeof path, "/Themes/%s", card_name);
    fake_hal_add_file(path, lettered, *size);
    fake_hal_add_directory_entry("/Themes", card_name, false, false);
    return lettered;
}

enum {
    PLACED_HEADER = 52,
    PLACED_BOX = 8,
    PLACED_BYTES = PLACED_HEADER + 2 * THEME_LAYOUT_COUNT * PLACED_BOX
};

static uint8_t placed[PLACED_BYTES];

static uint8_t *put_boxes(uint8_t *at, const ThemeLayout *layout)
{
    for (int id = 0; id < THEME_LAYOUT_COUNT; id++) {
        ThemeLayoutBox box = layout->boxes[id];
        put_le(at, (uint16_t)box.x, 2);
        put_le(at + 2, (uint16_t)box.y, 2);
        put_le(at + 4, box.width, 2);
        at[6] = box.height;
        at[7] = box.align;
        at += PLACED_BOX;
    }
    return at;
}

uint8_t *theme_fixture_placed(const char *card_name, const ThemeLayout *wide,
                              const ThemeLayout *tall, uint32_t *size)
{
    char path[PATH_BYTES];
    memset(placed, 0, sizeof placed);
    memcpy(placed, "ETHM", 4);
    put_le(placed + 4, 1, 2);
    put_le(placed + 6, PLACED_HEADER, 2);
    placed[20] = 11;
    memcpy(placed + 24, "Placed", 6);
    placed[49] = wide != NULL ? THEME_LAYOUT_COUNT : 0;
    placed[50] = tall != NULL ? THEME_LAYOUT_COUNT : 0;
    uint8_t *end = placed + PLACED_HEADER;
    end = wide != NULL ? put_boxes(end, wide) : end;
    end = tall != NULL ? put_boxes(end, tall) : end;
    *size = (uint32_t)(end - placed);
    put_le(placed + 8, *size, 4);
    put_le(placed + 12, hash_update(HASH_START, placed + 16, *size - 16), 4);
    snprintf(path, sizeof path, "/Themes/%s", card_name);
    fake_hal_add_file(path, placed, *size);
    fake_hal_add_directory_entry("/Themes", card_name, false, false);
    return placed;
}
