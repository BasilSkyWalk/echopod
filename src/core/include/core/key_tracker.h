#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key.h"

enum {
    KEY_TRACKER_QUEUE_CAPACITY = 8
};

typedef enum KeyGesture {
    KEY_GESTURE_PRESS,
    KEY_GESTURE_LONG_PRESS,
    KEY_GESTURE_REPEAT
} KeyGesture;

typedef enum KeyBehavior {
    KEY_BEHAVIOR_TAP_OR_HOLD,
    KEY_BEHAVIOR_REPEAT,
    KEY_BEHAVIOR_TAP_HOLD_REPEAT
} KeyBehavior;

typedef struct KeyEvent {
    Key key;
    KeyGesture gesture;
} KeyEvent;

typedef struct KeyTiming {
    uint32_t long_press_ms;
    uint32_t repeat_delay_ms;
    uint32_t repeat_interval_ms;
} KeyTiming;

typedef struct KeyConfig {
    KeyTiming timing;
    KeyBehavior behaviors[KEY_COUNT];
    uint32_t long_press_ms[KEY_COUNT];
} KeyConfig;

typedef struct KeyState {
    bool held;
    bool long_press_sent;
    uint32_t pressed_at_ms;
    uint32_t next_repeat_at_ms;
} KeyState;

typedef struct KeyTracker {
    KeyConfig config;
    KeyState states[KEY_COUNT];
    KeyEvent queue[KEY_TRACKER_QUEUE_CAPACITY];
    uint32_t queue_start;
    uint32_t queue_length;
} KeyTracker;

void key_tracker_init(KeyTracker *tracker, const KeyConfig *config);
void key_tracker_set_behavior(KeyTracker *tracker, Key key, KeyBehavior behavior);
void key_tracker_key_down(KeyTracker *tracker, Key key, uint32_t now_ms);
void key_tracker_key_up(KeyTracker *tracker, Key key, uint32_t now_ms);
void key_tracker_update(KeyTracker *tracker, uint32_t now_ms);
bool key_tracker_next_event(KeyTracker *tracker, KeyEvent *event);
