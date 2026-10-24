#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emmc.h"
#include "hal/system.h"

enum {
    SECTOR_WORDS = 128,
    TAG_OFFSET = 0x30,
    TAG_LENGTH = 8
};

static char tag[TAG_LENGTH + 1];
static bool tried;

static bool is_printable(uint8_t value)
{
    return value >= 0x20 && value < 0x7F;
}

static void read_tag(void)
{
    static uint32_t header[SECTOR_WORDS];
    EmmcInfo info;
    if (!emmc_start(&info) || !emmc_read(NULL, info.layout.slot_1, 1, header)) {
        return;
    }
    const uint8_t *bytes = (const uint8_t *)header + TAG_OFFSET;
    for (int index = 0; index < TAG_LENGTH && is_printable(bytes[index]); index++) {
        tag[index] = (char)bytes[index];
    }
}

const char *hal_system_version(void)
{
    if (!tried) {
        tried = true;
        read_tag();
    }
    return tag[0] != '\0' ? tag : "unknown";
}
