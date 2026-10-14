#include "core/key_tracker.h"

#include "core/deadline.h"

static bool repeats(const KeyTracker *tracker, Key key)
{
    return tracker->config.behaviors[key] == KEY_BEHAVIOR_REPEAT;
}

static bool repeats_after_hold(const KeyTracker *tracker, Key key)
{
    return tracker->config.behaviors[key] == KEY_BEHAVIOR_TAP_HOLD_REPEAT;
}

static bool long_press_due(const KeyTracker *tracker, Key key, uint32_t now_ms)
{
    uint32_t held_for_ms = now_ms - tracker->states[key].pressed_at_ms;
    return held_for_ms >= tracker->config.timing.long_press_ms;
}

static void push_event(KeyTracker *tracker, Key key, KeyGesture gesture)
{
    if (tracker->queue_length == KEY_TRACKER_QUEUE_CAPACITY) {
        return;
    }
    uint32_t slot = (tracker->queue_start + tracker->queue_length) % KEY_TRACKER_QUEUE_CAPACITY;
    tracker->queue[slot] = (KeyEvent){.key = key, .gesture = gesture};
    tracker->queue_length++;
}

void key_tracker_init(KeyTracker *tracker, const KeyConfig *config)
{
    *tracker = (KeyTracker){.config = *config};
}

void key_tracker_set_behavior(KeyTracker *tracker, Key key, KeyBehavior behavior)
{
    tracker->config.behaviors[key] = behavior;
}

void key_tracker_key_down(KeyTracker *tracker, Key key, uint32_t now_ms)
{
    KeyState *state = &tracker->states[key];
    if (state->held) {
        return;
    }
    *state = (KeyState){
        .held = true,
        .pressed_at_ms = now_ms,
        .next_repeat_at_ms = now_ms + tracker->config.timing.repeat_delay_ms,
    };
    if (repeats(tracker, key)) {
        push_event(tracker, key, KEY_GESTURE_PRESS);
    }
}

static void finish_tap_or_hold(KeyTracker *tracker, Key key, uint32_t now_ms)
{
    if (tracker->states[key].long_press_sent) {
        return;
    }
    bool held_long = long_press_due(tracker, key, now_ms);
    push_event(tracker, key, held_long ? KEY_GESTURE_LONG_PRESS : KEY_GESTURE_PRESS);
}

void key_tracker_key_up(KeyTracker *tracker, Key key, uint32_t now_ms)
{
    if (!tracker->states[key].held) {
        return;
    }
    if (!repeats(tracker, key)) {
        finish_tap_or_hold(tracker, key, now_ms);
    }
    tracker->states[key].held = false;
}

static void update_repeat(KeyTracker *tracker, Key key, uint32_t now_ms)
{
    KeyState *state = &tracker->states[key];
    if (!deadline_reached(now_ms, state->next_repeat_at_ms)) {
        return;
    }
    push_event(tracker, key, KEY_GESTURE_REPEAT);
    state->next_repeat_at_ms = now_ms + tracker->config.timing.repeat_interval_ms;
}

static void update_long_press(KeyTracker *tracker, Key key, uint32_t now_ms)
{
    KeyState *state = &tracker->states[key];
    if (state->long_press_sent || !long_press_due(tracker, key, now_ms)) {
        return;
    }
    push_event(tracker, key, KEY_GESTURE_LONG_PRESS);
    state->long_press_sent = true;
    state->next_repeat_at_ms = now_ms + tracker->config.timing.repeat_interval_ms;
}

void key_tracker_update(KeyTracker *tracker, uint32_t now_ms)
{
    for (int index = 0; index < KEY_COUNT; index++) {
        Key key = (Key)index;
        if (!tracker->states[key].held) {
            continue;
        }
        bool repeating = repeats(tracker, key) ||
                         (repeats_after_hold(tracker, key) && tracker->states[key].long_press_sent);
        if (repeating) {
            update_repeat(tracker, key, now_ms);
        } else {
            update_long_press(tracker, key, now_ms);
        }
    }
}

bool key_tracker_next_event(KeyTracker *tracker, KeyEvent *event)
{
    if (tracker->queue_length == 0) {
        return false;
    }
    *event = tracker->queue[tracker->queue_start];
    tracker->queue_start = (tracker->queue_start + 1) % KEY_TRACKER_QUEUE_CAPACITY;
    tracker->queue_length--;
    return true;
}
