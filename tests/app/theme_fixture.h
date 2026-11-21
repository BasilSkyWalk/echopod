#pragma once

#include <stdint.h>

#include "ui/theme.h"

uint8_t *theme_fixture_on_card(const char *fixture, const char *card_name, uint32_t *size);

typedef enum LetteredSpoil {
    LETTERED_SOUND,
    LETTERED_DAMAGED,
    LETTERED_OUT_OF_ORDER,
    LETTERED_PAPERED
} LetteredSpoil;

uint8_t *theme_fixture_lettered(const char *card_name, LetteredSpoil spoil, uint32_t *size);
uint8_t *theme_fixture_placed(const char *card_name, const ThemeLayout *wide,
                              const ThemeLayout *tall, uint32_t *size);
