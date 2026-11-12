#pragma once

#include <stdbool.h>
#include <stdint.h>

#define OVERLAY_WINDOW_BASE 0x0307A000u
#define OVERLAY_WINDOW_BYTES 0x15000u
#define OVERLAY_HEADER_BYTES 16u
#define RESCAN_OVERLAY_IMAGE_OFFSET 0x200000u
#define RESCAN_OVERLAY_MAGIC 0x4C564F45u
#define USB_OVERLAY_IMAGE_OFFSET 0x43C000u
#define USB_OVERLAY_MAGIC 0x42535545u
#define NP_FACES_BASE 0x0308B000u
#define NP_FACES_BYTES 0x4000u
#define NP_FACES_ENTRY (NP_FACES_BASE + OVERLAY_HEADER_BYTES)
#define NP_FACES_IMAGE_OFFSET 0x1FC000u
#define NP_FACES_IMAGE_MAGIC 0x46504E45u

typedef struct BoardOverlay {
    uint32_t image_offset;
    uint32_t magic;
    uint8_t *base;
    uint32_t capacity;
} BoardOverlay;

bool board_overlay_load(const BoardOverlay *overlay);
const void *board_usb_overlay_load(void);
void board_np_faces_forget(void);
