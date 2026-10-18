#include "core/power_policy.h"

#include "core/deadline.h"

static PowerKeyOutcome outcome(bool consumed, bool shutdown, bool background)
{
    return (PowerKeyOutcome){.consumed = consumed, .shutdown = shutdown, .background = background};
}

static PowerKeyOutcome power_key(PowerPolicy *policy, KeyGesture gesture)
{
    if (gesture == KEY_GESTURE_LONG_PRESS) {
        return outcome(true, true, false);
    }
    if (gesture == KEY_GESTURE_PRESS) {
        policy->display_on = !policy->display_on;
    }
    return outcome(true, false, false);
}

static bool is_media_key(Key key)
{
    return key == KEY_VOLUME_UP || key == KEY_VOLUME_DOWN || key == KEY_PLAY;
}

void power_policy_init(PowerPolicy *policy, uint32_t timeout_ms, uint32_t now_ms)
{
    *policy = (PowerPolicy){.display_on = true,
                            .timeout_ms = timeout_ms,
                            .last_activity_ms = now_ms,
                            .auto_off_ms = 0,
                            .awake_ms = now_ms};
}

void power_policy_set_timeout(PowerPolicy *policy, uint32_t timeout_ms)
{
    policy->timeout_ms = timeout_ms;
}

PowerKeyOutcome power_policy_key(PowerPolicy *policy, KeyEvent event, uint32_t now_ms)
{
    policy->last_activity_ms = now_ms;
    policy->awake_ms = now_ms;
    if (event.key == KEY_POWER) {
        return power_key(policy, event.gesture);
    }
    if (policy->display_on) {
        return outcome(false, false, false);
    }
    return outcome(true, false, is_media_key(event.key));
}

void power_policy_start_sleep_timer(PowerPolicy *policy, uint32_t duration_ms, uint32_t now_ms)
{
    policy->sleep_timer_active = true;
    policy->sleep_deadline_ms = now_ms + duration_ms;
}

void power_policy_cancel_sleep_timer(PowerPolicy *policy)
{
    policy->sleep_timer_active = false;
}

void power_policy_wake(PowerPolicy *policy, uint32_t now_ms)
{
    policy->last_activity_ms = now_ms;
    policy->awake_ms = now_ms;
    policy->display_on = true;
}

void power_policy_set_auto_off(PowerPolicy *policy, uint32_t auto_off_ms, uint32_t now_ms)
{
    policy->auto_off_ms = auto_off_ms;
    policy->awake_ms = now_ms;
}

void power_policy_stay_awake(PowerPolicy *policy, uint32_t now_ms)
{
    policy->awake_ms = now_ms;
}

static bool auto_off_due(PowerPolicy *policy, uint32_t now_ms)
{
    if (policy->auto_off_ms == 0 || now_ms - policy->awake_ms < policy->auto_off_ms) {
        return false;
    }
    policy->awake_ms = now_ms;
    policy->display_on = false;
    return true;
}

static bool timed_out(const PowerPolicy *policy, uint32_t now_ms)
{
    return policy->timeout_ms > 0 && now_ms - policy->last_activity_ms >= policy->timeout_ms;
}

bool power_policy_update(PowerPolicy *policy, uint32_t now_ms)
{
    if (policy->display_on && timed_out(policy, now_ms)) {
        policy->display_on = false;
    }
    if (auto_off_due(policy, now_ms)) {
        return true;
    }
    if (!policy->sleep_timer_active || !deadline_reached(now_ms, policy->sleep_deadline_ms)) {
        return false;
    }
    policy->sleep_timer_active = false;
    policy->display_on = false;
    return true;
}
