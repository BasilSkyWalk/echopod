#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum DacPin {
    DAC_PIN_MUTE,
    DAC_PIN_ENABLE_A,
    DAC_PIN_ENABLE_B,
    DAC_PIN_RESET,
    DAC_PIN_AMP
} DacPin;

typedef enum DacResult {
    DAC_READY,
    DAC_NO_REPLY,
    DAC_NOT_READY
} DacResult;

typedef struct DacBus {
    bool (*write)(void *context, uint8_t address, uint32_t map, uint8_t value);
    bool (*read)(void *context, uint8_t address, uint32_t map, uint8_t *value);
    void (*set_pin)(void *context, DacPin pin, bool level);
    void (*delay_us)(void *context, uint32_t microseconds);
    void *context;
} DacBus;

enum {
    DAC_LEFT_ADDRESS = 0x60,
    DAC_RIGHT_ADDRESS = 0x66,
    DAC_VOLUME_LEVELS = 50,
    DAC_VOLUME_MUTED = 0xFF
};

void dac_power_pins(const DacBus *bus);
bool dac_probe(const DacBus *bus);
DacResult dac_configure(const DacBus *bus, uint8_t rate_byte);
void dac_output_on(const DacBus *bus, uint8_t volume);
void dac_output_off(const DacBus *bus);
void dac_set_muted(const DacBus *bus, bool muted, uint8_t volume);
void dac_set_volume(const DacBus *bus, uint8_t volume);
void dac_set_gain(const DacBus *bus, bool high);
uint8_t dac_volume_value(uint8_t level);
