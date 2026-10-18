#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/key_tracker.h"

typedef struct PowerPolicy {
    bool display_on;
    bool sleep_timer_active;
    uint32_t last_activity_ms;
    uint32_t timeout_ms;
    uint32_t sleep_deadline_ms;
    uint32_t auto_off_ms;
    uint32_t awake_ms;
} PowerPolicy;

typedef struct PowerKeyOutcome {
    bool consumed;
    bool shutdown;
    bool background;
} PowerKeyOutcome;

void power_policy_init(PowerPolicy *policy, uint32_t timeout_ms, uint32_t now_ms);
void power_policy_set_timeout(PowerPolicy *policy, uint32_t timeout_ms);
PowerKeyOutcome power_policy_key(PowerPolicy *policy, KeyEvent event, uint32_t now_ms);
void power_policy_start_sleep_timer(PowerPolicy *policy, uint32_t duration_ms, uint32_t now_ms);
void power_policy_cancel_sleep_timer(PowerPolicy *policy);
bool power_policy_update(PowerPolicy *policy, uint32_t now_ms);
void power_policy_wake(PowerPolicy *policy, uint32_t now_ms);
void power_policy_set_auto_off(PowerPolicy *policy, uint32_t auto_off_ms, uint32_t now_ms);
void power_policy_stay_awake(PowerPolicy *policy, uint32_t now_ms);
