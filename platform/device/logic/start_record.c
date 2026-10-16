#include "device/start_record.h"

#define START_RECORD_MAGIC 0x53545254u

static bool record_valid(const StartRecord *record)
{
    return record->magic == START_RECORD_MAGIC && record->check == ~record->starts;
}

StartInfo start_record_boot(StartRecord *record)
{
    StartInfo info = {.warm = false};
    if (record_valid(record)) {
        info = (StartInfo){.warm = true,
                           .previous_uptime_ms = record->uptime_ms,
                           .previous_power_off = record->power_off != 0,
                           .fault_status = record->fault_status,
                           .hard_fault_status = record->hard_fault_status,
                           .fault_address = record->fault_address};
        record->starts++;
    } else {
        record->starts = 1;
    }
    record->magic = START_RECORD_MAGIC;
    record->check = ~record->starts;
    record->uptime_ms = 0;
    record->power_off = 0;
    record->fault_status = 0;
    record->hard_fault_status = 0;
    record->fault_address = 0;
    info.starts = record->starts;
    return info;
}

void start_record_tick(StartRecord *record, uint32_t uptime_ms)
{
    record->uptime_ms = uptime_ms;
}

void start_record_power_off(StartRecord *record)
{
    record->power_off = 1;
}

void start_record_fault(StartRecord *record, uint32_t fault_status, uint32_t hard_fault_status,
                        uint32_t fault_address)
{
    record->fault_status = fault_status;
    record->hard_fault_status = hard_fault_status;
    record->fault_address = fault_address;
}
