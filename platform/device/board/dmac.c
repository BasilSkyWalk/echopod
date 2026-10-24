#include "dmac.h"

#include "hal/clock.h"
#include "registers.h"

#define DMAC_BASE 0x60000000u
#define CHANNEL_BASE (DMAC_BASE + 0x58u * 5u)
#define CHANNEL_SAR (CHANNEL_BASE + 0x00u)
#define CHANNEL_DAR (CHANNEL_BASE + 0x08u)
#define CHANNEL_LLP (CHANNEL_BASE + 0x10u)
#define CHANNEL_CTL_LOW (CHANNEL_BASE + 0x18u)
#define CHANNEL_CTL_HIGH (CHANNEL_BASE + 0x1Cu)
#define CHANNEL_CFG_LOW (CHANNEL_BASE + 0x40u)
#define CHANNEL_CFG_HIGH (CHANNEL_BASE + 0x44u)
#define DMAC_MASK_TFR (DMAC_BASE + 0x310u)
#define DMAC_MASK_BLOCK (DMAC_BASE + 0x318u)
#define DMAC_MASK_ERR (DMAC_BASE + 0x330u)
#define DMAC_CLEAR_TFR (DMAC_BASE + 0x338u)
#define DMAC_CLEAR_BLOCK (DMAC_BASE + 0x340u)
#define DMAC_CLEAR_ERR (DMAC_BASE + 0x358u)
#define DMAC_CONFIG (DMAC_BASE + 0x398u)
#define DMAC_CHANNEL_ENABLE (DMAC_BASE + 0x3A0u)
#define DMAC_COMP_TYPE (DMAC_BASE + 0x3F8u)

#define DW_AHB_DMAC_TYPE 0x44571110u
#define CHANNEL_BIT 0x20u
#define CHANNEL_WRITE_ENABLE 0x2000u
#define CFG_LOW_I2S_TX 0x00000840u
#define CFG_HIGH_I2S_TX 0x00001007u
#define CFG_SUSPEND 0x100u
#define CFG_FIFO_EMPTY 0x200u

enum {
    SUSPEND_POLLS = 800,
    STOP_TIMEOUT_MS = 10
};

static bool active;
static int suspend_depth;

bool dmac_present(void)
{
    return register_read(DMAC_COMP_TYPE) == DW_AHB_DMAC_TYPE;
}

bool dmac_running(void)
{
    return active && (register_read(DMAC_CHANNEL_ENABLE) & CHANNEL_BIT) != 0;
}

bool dmac_start_loop(const DmaItem *item)
{
    if ((register_read(DMAC_CHANNEL_ENABLE) & CHANNEL_BIT) != 0) {
        return false;
    }
    register_write(DMAC_CLEAR_TFR, CHANNEL_BIT);
    register_write(DMAC_CLEAR_BLOCK, CHANNEL_BIT);
    register_write(DMAC_CLEAR_ERR, CHANNEL_BIT);
    register_write(DMAC_MASK_TFR, CHANNEL_WRITE_ENABLE);
    register_write(DMAC_MASK_BLOCK, CHANNEL_WRITE_ENABLE);
    register_write(DMAC_MASK_ERR, CHANNEL_WRITE_ENABLE);
    register_write(CHANNEL_SAR, item->source);
    register_write(CHANNEL_DAR, item->destination);
    register_write(CHANNEL_LLP, (uint32_t)(uintptr_t)item);
    register_write(CHANNEL_CTL_LOW, item->control_low);
    register_write(CHANNEL_CTL_HIGH, item->control_high);
    register_write(CHANNEL_CFG_LOW, CFG_LOW_I2S_TX);
    register_write(CHANNEL_CFG_HIGH, CFG_HIGH_I2S_TX);
    register_write(DMAC_CONFIG, 1);
    register_write(DMAC_CHANNEL_ENABLE, CHANNEL_WRITE_ENABLE | CHANNEL_BIT);
    active = true;
    suspend_depth = 0;
    return true;
}

static void suspend_channel(void)
{
    register_write(CHANNEL_CFG_LOW, register_read(CHANNEL_CFG_LOW) | CFG_SUSPEND);
    for (int poll = 0; poll < SUSPEND_POLLS; poll++) {
        if ((register_read(CHANNEL_CFG_LOW) & CFG_FIFO_EMPTY) != 0) {
            return;
        }
    }
}

void dmac_stop(void)
{
    if (!active) {
        return;
    }
    if ((register_read(DMAC_CHANNEL_ENABLE) & CHANNEL_BIT) != 0) {
        suspend_channel();
        register_write(DMAC_CHANNEL_ENABLE, CHANNEL_WRITE_ENABLE);
        uint32_t start = hal_clock_milliseconds();
        while ((register_read(DMAC_CHANNEL_ENABLE) & CHANNEL_BIT) != 0 &&
               hal_clock_milliseconds() - start < STOP_TIMEOUT_MS) {
        }
    }
    register_write(CHANNEL_CFG_LOW, register_read(CHANNEL_CFG_LOW) & ~CFG_SUSPEND);
    active = false;
    suspend_depth = 0;
}

uint32_t dmac_source(void)
{
    return active ? register_read(CHANNEL_SAR) : 0;
}

void dmac_suspend(void)
{
    if (!dmac_running()) {
        return;
    }
    if (suspend_depth == 0) {
        suspend_channel();
    }
    suspend_depth++;
}

void dmac_resume(void)
{
    if (suspend_depth == 0) {
        return;
    }
    suspend_depth--;
    if (suspend_depth == 0) {
        register_write(CHANNEL_CFG_LOW, register_read(CHANNEL_CFG_LOW) & ~CFG_SUSPEND);
    }
}
