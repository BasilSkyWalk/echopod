#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/system.h"

#define START_RECORD_NO_EMMC_REPLY 0xFFFFFFFFu

enum {
    START_RECORD_STACK_WORDS = 3
};

typedef struct StartRecord {
    uint32_t magic;
    uint32_t starts;
    uint32_t check;
    uint32_t uptime_ms;
    uint32_t power_off;
    uint32_t fault_status;
    uint32_t hard_fault_status;
    uint32_t fault_address;
    uint32_t step;
    uint32_t step_finished;
    uint32_t step_ms;
    uint32_t millivolts;
    uint32_t stalled;
    uint32_t stall_lr;
    uint32_t stall_sp;
    uint32_t stall_words[START_RECORD_STACK_WORDS];
    uint32_t stall_stack_used;
    uint32_t update_result;
    uint32_t update_detail;
    uint32_t update_emmc;
} StartRecord;

typedef struct StallSnapshot {
    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t words[START_RECORD_STACK_WORDS];
    uint32_t stack_used;
} StallSnapshot;

typedef struct StartInfo {
    bool warm;
    uint32_t starts;
    uint32_t previous_uptime_ms;
    bool previous_power_off;
    uint32_t fault_status;
    uint32_t hard_fault_status;
    uint32_t fault_address;
    HalStartStep last_step;
    bool last_step_finished;
    uint32_t last_step_ms;
    uint32_t last_millivolts;
    bool stalled;
    uint32_t stall_lr;
    uint32_t stall_sp;
    uint32_t stall_words[START_RECORD_STACK_WORDS];
    uint32_t stall_stack_used;
    HalUpdateRecord last_update;
} StartInfo;

StartInfo start_record_boot(StartRecord *record);
void start_record_tick(StartRecord *record, uint32_t uptime_ms);
void start_record_power_off(StartRecord *record);
void start_record_fault(StartRecord *record, uint32_t fault_status, uint32_t hard_fault_status,
                        uint32_t fault_address);
void start_record_stall(StartRecord *record, const StallSnapshot *snapshot);
void start_record_step_begin(StartRecord *record, HalStartStep step);
void start_record_step_end(StartRecord *record);
void start_record_battery(StartRecord *record, uint32_t millivolts);
void start_record_update(StartRecord *record, HalUpdateRecord update);
