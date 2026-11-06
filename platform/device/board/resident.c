#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "overlay.h"

#define RESIDENT_IMAGE_OFFSET 0x238200u
#define RESIDENT_MAGIC 0x53455245u
#define RESIDENT_END 0x0309F000u

extern uint8_t device_resident_start[];
extern uint8_t device_resident_end[];

bool board_resident_load(void)
{
    uint8_t *base = device_resident_start;
    uint32_t capacity = (uint32_t)(RESIDENT_END - (uintptr_t)base);
    BoardOverlay tables = {RESIDENT_IMAGE_OFFSET, RESIDENT_MAGIC, base, capacity};
    if (board_overlay_load(&tables)) {
        return true;
    }
    uint32_t bytes = (uint32_t)(device_resident_end - device_resident_start);
    for (uint32_t index = 0; index < bytes; index++) {
        base[index] = 0;
    }
    return false;
}
