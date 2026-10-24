#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/input.h"

enum {
    KEY_SCANNER_QUEUE_CAPACITY = 16
};

typedef struct KeyScanner {
    uint8_t last_sample;
    uint8_t stable;
    bool power_armed;
    uint8_t head;
    uint8_t count;
    HalKeyTransition queue[KEY_SCANNER_QUEUE_CAPACITY];
} KeyScanner;

void key_scanner_init(KeyScanner *scanner);
void key_scanner_sample(KeyScanner *scanner, uint8_t pressed_mask, uint32_t time_ms);
bool key_scanner_next(KeyScanner *scanner, HalKeyTransition *transition);
