#include "device/start_record.h"

#define START_RECORD_MAGIC 0x53545254u

static bool record_valid(const StartRecord *record)
{
    return record->magic == START_RECORD_MAGIC && record->check == ~record->starts;
}

static HalStartStep step_of(uint32_t value)
{
    return value < HAL_START_STEP_COUNT ? (HalStartStep)value : HAL_START_STEP_NONE;
}

static StartInfo warm_info(const StartRecord *record)
{
    return (StartInfo){.warm = true,
                       .previous_uptime_ms = record->uptime_ms,
                       .previous_power_off = record->power_off != 0,
                       .fault_status = record->fault_status,
                       .hard_fault_status = record->hard_fault_status,
                       .fault_address = record->fault_address,
                       .last_step = step_of(record->step),
                       .last_step_finished = record->step_finished != 0,
                       .last_step_ms = record->step_ms,
                       .last_millivolts = record->millivolts};
}

static void clear_run(StartRecord *record)
{
    record->uptime_ms = 0;
    record->power_off = 0;
    record->fault_status = 0;
    record->hard_fault_status = 0;
    record->fault_address = 0;
    record->step = HAL_START_STEP_NONE;
    record->step_finished = 0;
    record->step_ms = 0;
    record->millivolts = 0;
}

StartInfo start_record_boot(StartRecord *record)
{
    StartInfo info = {.warm = false, .last_step = HAL_START_STEP_NONE};
    if (record_valid(record)) {
        info = warm_info(record);
        record->starts++;
    } else {
        record->starts = 1;
    }
    record->magic = START_RECORD_MAGIC;
    record->check = ~record->starts;
    clear_run(record);
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

void start_record_step_begin(StartRecord *record, HalStartStep step)
{
    record->step_finished = 0;
    record->step_ms = record->uptime_ms;
    record->step = (uint32_t)step;
}

void start_record_step_end(StartRecord *record)
{
    record->step_finished = 1;
}

void start_record_battery(StartRecord *record, uint32_t millivolts)
{
    record->millivolts = millivolts;
}
