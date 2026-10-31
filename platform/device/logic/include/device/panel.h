#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    PANEL_FIRST_VISIBLE_ROW = 35,
    PANEL_BUS_DATA_SHIFT = 16,
    PANEL_BUS_WR = 1u << 24,
    PANEL_BUS_DC = 1u << 25
};

typedef enum PanelVariant {
    PANEL_VARIANT_A,
    PANEL_VARIANT_B
} PanelVariant;

typedef struct PanelBus {
    void (*command)(void *context, uint8_t command);
    void (*data)(void *context, uint8_t value);
    void (*pixels)(void *context, const uint16_t *pixels, uint32_t count);
    void (*delay_ms)(void *context, uint32_t milliseconds);
    void *context;
} PanelBus;

typedef enum PanelRotation {
    PANEL_LANDSCAPE,
    PANEL_PORTRAIT,
    PANEL_PORTRAIT_FLIPPED,
    PANEL_ROTATION_COUNT
} PanelRotation;

typedef struct PanelWindow {
    int x;
    int y;
    int width;
    int height;
    PanelRotation rotation;
} PanelWindow;

void panel_send_init(const PanelBus *bus, PanelVariant variant);
void panel_set_rotation(const PanelBus *bus, PanelRotation rotation);
void panel_begin_write(const PanelBus *bus, PanelWindow window);
uint32_t panel_bus_word(uint32_t shadow, uint8_t value, bool data);
