#include "theme_fixture.h"

#include <stdio.h>
#include <string.h>

#include "fake_hal.h"

enum {
    FIXTURE_BYTES = 512,
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
