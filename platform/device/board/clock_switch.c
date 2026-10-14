#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "device/pll.h"
#include "hal/clock.h"
#include "hal/system.h"
#include "registers.h"

#define PLL_CON0 (CRU_BASE + 0x00u)
#define PLL_CON1 (CRU_BASE + 0x04u)
#define PLL_CON2 (CRU_BASE + 0x08u)
#define CRU_MODE (CRU_BASE + 0x10u)
#define CRU_CLOCK_SELECT (CRU_BASE + 0x14u)
#define CRU_PCLK_SELECT (CRU_BASE + 0x18u)
#define GRF_SPEED (GRF_BASE + 0xC4u)

#define CON1_PREPARE 0x80008000u
#define CON1_POWER_DOWN 0x40004000u
#define CON1_POWER_UP 0x40000000u
#define CON1_LOCKED 0x400u
#define CON2_BEFORE_POWER_UP 0x08000000u
#define CON2_KEPT_BITS 0x0F000000u
#define CON2_POWER_DOWN_BITS 0x0D000000u
#define MODE_FROM_CRYSTAL 0x00010000u
#define MODE_FROM_PLL 0x00010001u
#define MODE_CLEAR_DIVIDER 0x3F000000u
#define MODE_ON_PLL 0x1u
#define CLOCK_SELECT_UNDIVIDED 0xFFFF0000u
#define PCLK_UNDIVIDED 0x03000000u
#define CORE_SPEED_SLOW 0x30400000u
#define BUS_SPEED_SLOW 0xC0000000u

enum {
    BOOT_CLOCK_MHZ = 24,
    LOCK_TIMEOUT_MS = 200,
    DIVIDER_SETTLE_US = 10
};

static uint32_t pll_mhz;

static void leave_pll(void)
{
    register_write(CRU_MODE, MODE_FROM_CRYSTAL);
    register_write(CRU_CLOCK_SELECT, CLOCK_SELECT_UNDIVIDED);
    register_write(CRU_PCLK_SELECT, PCLK_UNDIVIDED);
}

static void power_down_pll(void)
{
    register_write(PLL_CON2, register_read(PLL_CON2) | CON2_POWER_DOWN_BITS);
    register_write(PLL_CON1, CON1_POWER_DOWN);
}

static void use_slow_speed_bits(void)
{
    register_write(GRF_SPEED, CORE_SPEED_SLOW);
    register_write(GRF_SPEED, BUS_SPEED_SLOW);
}

static void run_from_crystal(void)
{
    register_write(CRU_MODE, MODE_CLEAR_DIVIDER);
    leave_pll();
    register_write(CRU_MODE, MODE_CLEAR_DIVIDER);
    board_clock_set_rate(BOOT_CLOCK_MHZ);
    power_down_pll();
    use_slow_speed_bits();
    pll_mhz = 0;
}

static void program_pll(const PllSetting *setting)
{
    register_write(PLL_CON1, CON1_PREPARE);
    register_write(PLL_CON1, CON1_POWER_DOWN);
    register_write(PLL_CON2, CON2_BEFORE_POWER_UP);
    register_write(PLL_CON0, setting->con0);
    register_write(PLL_CON1, setting->con1);
    register_write(PLL_CON2, register_read(PLL_CON2) & CON2_KEPT_BITS);
    register_write(PLL_CON1, CON1_POWER_UP);
    register_write(GRF_SPEED, setting->core_speed_bits);
    register_write(GRF_SPEED, setting->bus_speed_bits);
}

static bool wait_for_lock(void)
{
    uint32_t start = hal_clock_milliseconds();
    while ((register_read(PLL_CON1) & CON1_LOCKED) == 0) {
        if (hal_clock_milliseconds() - start >= LOCK_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

static bool run_from_pll(uint32_t mhz)
{
    PllSetting setting;
    if (!pll_setting_for(mhz, &setting)) {
        return false;
    }
    if ((register_read(CRU_MODE) & MODE_ON_PLL) != 0) {
        run_from_crystal();
    }
    program_pll(&setting);
    if (!wait_for_lock()) {
        power_down_pll();
        use_slow_speed_bits();
        return false;
    }
    register_write(CRU_CLOCK_SELECT, setting.clock_select);
    board_delay_us(DIVIDER_SETTLE_US);
    register_write(CRU_PCLK_SELECT, setting.pclk_select);
    board_delay_us(DIVIDER_SETTLE_US);
    register_write(CRU_MODE, MODE_FROM_PLL);
    board_clock_set_rate(mhz);
    pll_mhz = mhz;
    return true;
}

uint32_t board_pclk_hz(void)
{
    PllSetting setting;
    if (pll_mhz == 0 || !pll_setting_for(pll_mhz, &setting)) {
        return BOOT_CLOCK_MHZ * 1000000u;
    }
    return setting.pclk_hz;
}

uint32_t hal_system_clock_mhz(void)
{
    return pll_mhz != 0 ? pll_mhz : BOOT_CLOCK_MHZ;
}

bool hal_system_set_clock_mhz(uint32_t mhz)
{
    if (mhz == hal_system_clock_mhz()) {
        return true;
    }
    if (mhz == BOOT_CLOCK_MHZ) {
        run_from_crystal();
        return true;
    }
    return run_from_pll(mhz);
}
