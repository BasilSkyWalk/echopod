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

static HalUpdateResult update_result_of(uint32_t value)
{
    return value < HAL_UPDATE_RESULT_COUNT ? (HalUpdateResult)value : HAL_UPDATE_NONE;
}

static HalUpdateRecord update_of(const StartRecord *record)
{
    return (HalUpdateRecord){.result = update_result_of(record->update_result),
                             .detail = (uint8_t)record->update_detail,
                             .emmc_replied = record->update_emmc != START_RECORD_NO_EMMC_REPLY,
                             .emmc_status = record->update_emmc};
}

static StartInfo warm_info(const StartRecord *record)
{
    StartInfo info = {.warm = true,
                      .previous_uptime_ms = record->uptime_ms,
                      .previous_power_off = record->power_off != 0,
                      .fault_status = record->fault_status,
                      .hard_fault_status = record->hard_fault_status,
                      .fault_address = record->fault_address,
                      .last_step = step_of(record->step),
                      .last_step_finished = record->step_finished != 0,
                      .last_step_ms = record->step_ms,
                      .last_millivolts = record->millivolts,
                      .stalled = record->stalled != 0,
                      .stall_lr = record->stall_lr,
                      .stall_sp = record->stall_sp,
                      .stall_stack_used = record->stall_stack_used,
                      .last_update = update_of(record)};
    for (uint32_t index = 0; index < START_RECORD_STACK_WORDS; index++) {
        info.stall_words[index] = record->stall_words[index];
    }
    return info;
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
    record->stalled = 0;
    record->stall_lr = 0;
    record->stall_sp = 0;
    record->stall_stack_used = 0;
    record->update_result = HAL_UPDATE_NONE;
    record->update_detail = 0;
    record->update_emmc = START_RECORD_NO_EMMC_REPLY;
    for (uint32_t index = 0; index < START_RECORD_STACK_WORDS; index++) {
        record->stall_words[index] = 0;
    }
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

void start_record_stall(StartRecord *record, const StallSnapshot *snapshot)
{
    record->stalled = 1;
    record->fault_address = snapshot->pc;
    record->stall_lr = snapshot->lr;
    record->stall_sp = snapshot->sp;
    record->stall_stack_used = snapshot->stack_used;
    for (uint32_t index = 0; index < START_RECORD_STACK_WORDS; index++) {
        record->stall_words[index] = snapshot->words[index];
    }
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

void start_record_update(StartRecord *record, HalUpdateRecord update)
{
    record->update_result = (uint32_t)update.result;
    record->update_detail = update.detail;
    record->update_emmc = update.emmc_replied ? update.emmc_status : START_RECORD_NO_EMMC_REPLY;
}
