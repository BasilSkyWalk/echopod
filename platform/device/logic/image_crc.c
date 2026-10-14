#include "device/image_crc.h"

#include <stdbool.h>

#define POLYNOMIAL 0x04C10DB7u
#define TOP_BIT 0x80000000u

static uint32_t table[256];
static bool table_ready;

static void build_table(void)
{
    for (uint32_t index = 0; index < 256; index++) {
        uint32_t value = index << 24;
        for (int bit = 0; bit < 8; bit++) {
            value = (value & TOP_BIT) != 0 ? (value << 1) ^ POLYNOMIAL : value << 1;
        }
        table[index] = value;
    }
    table_ready = true;
}

uint32_t image_crc_update(uint32_t crc, const uint8_t *bytes, uint32_t length)
{
    if (!table_ready) {
        build_table();
    }
    for (uint32_t index = 0; index < length; index++) {
        crc = (crc << 8) ^ table[((crc >> 24) ^ bytes[index]) & 0xFFu];
    }
    return crc;
}
