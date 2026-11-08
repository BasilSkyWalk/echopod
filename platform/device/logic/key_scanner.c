#include "device/key_scanner.h"

static const uint8_t POWER_MASK = 1u << KEY_POWER;

void key_scanner_init(KeyScanner *scanner)
{
    *scanner = (KeyScanner){.power_armed = false};
}

static bool has_room(const KeyScanner *scanner, bool pressed)
{
    uint32_t free_slots = KEY_SCANNER_QUEUE_CAPACITY - (uint32_t)scanner->count;
    return pressed ? free_slots > KEY_COUNT : free_slots > 0;
}

static uint8_t slot_at(const KeyScanner *scanner, uint32_t index)
{
    return (uint8_t)((scanner->head + index) % KEY_SCANNER_QUEUE_CAPACITY);
}

static bool push(KeyScanner *scanner, HalKeyTransition transition)
{
    if (!has_room(scanner, transition.pressed)) {
        return false;
    }
    scanner->queue[slot_at(scanner, scanner->count)] = transition;
    scanner->count++;
    return true;
}

static uint8_t gate_power(KeyScanner *scanner, uint8_t mask)
{
    if (!scanner->power_armed && (mask & POWER_MASK) == 0) {
        scanner->power_armed = true;
    }
    return scanner->power_armed ? mask : (uint8_t)(mask & ~POWER_MASK);
}

static void emit_changes(KeyScanner *scanner, uint8_t mask, uint32_t time_ms)
{
    uint8_t changed = (uint8_t)(scanner->stable ^ mask);
    for (int pass = 0; pass < 2; pass++) {
        bool pressed = pass == 1;
        for (int key = 0; key < KEY_COUNT; key++) {
            uint8_t bit = (uint8_t)(1u << key);
            if ((changed & bit) != 0 && ((mask & bit) != 0) == pressed &&
                push(scanner, (HalKeyTransition){(Key)key, pressed, time_ms})) {
                scanner->stable ^= bit;
            }
        }
    }
}

void key_scanner_sample(KeyScanner *scanner, uint8_t pressed_mask, uint32_t time_ms)
{
    uint8_t mask = gate_power(scanner, pressed_mask);
    if (mask == scanner->last_sample && mask != scanner->stable) {
        emit_changes(scanner, mask, time_ms);
    }
    scanner->last_sample = mask;
}

bool key_scanner_next(KeyScanner *scanner, HalKeyTransition *transition)
{
    if (scanner->count == 0) {
        return false;
    }
    *transition = scanner->queue[scanner->head];
    scanner->head = slot_at(scanner, 1);
    scanner->count--;
    return true;
}

void key_scanner_drop_presses(KeyScanner *scanner)
{
    uint8_t kept = 0;
    for (uint8_t index = 0; index < scanner->count; index++) {
        HalKeyTransition transition = scanner->queue[slot_at(scanner, index)];
        if (!transition.pressed) {
            scanner->queue[slot_at(scanner, kept)] = transition;
            kept++;
        }
    }
    scanner->count = kept;
}
