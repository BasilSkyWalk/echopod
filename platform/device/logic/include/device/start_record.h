#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct StartRecord {
    uint32_t magic;
    uint32_t starts;
    uint32_t check;
    uint32_t uptime_ms;
    uint32_t power_off;
    uint32_t fault_status;
    uint32_t hard_fault_status;
    uint32_t fault_address;
} StartRecord;

typedef struct StartInfo {
    bool warm;
    uint32_t starts;
    uint32_t previous_uptime_ms;
    bool previous_power_off;
    uint32_t fault_status;
    uint32_t hard_fault_status;
    uint32_t fault_address;
} StartInfo;

StartInfo start_record_boot(StartRecord *record);
void start_record_tick(StartRecord *record, uint32_t uptime_ms);
void start_record_power_off(StartRecord *record);
void start_record_fault(StartRecord *record, uint32_t fault_status, uint32_t hard_fault_status,
                        uint32_t fault_address);
