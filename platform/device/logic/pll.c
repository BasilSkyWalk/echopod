#include "device/pll.h"

#include <stddef.h>

#define CON0_MASK 0xFFFF0000u
#define CON1_MASK 0x11FF0000u
#define CLOCK_SELECT_UNDIVIDED 0xFFFF0000u
#define PCLK_SELECT_MASK 0x03000000u
#define CORE_SPEED_SLOW 0x30400000u
#define CORE_SPEED_FAST 0x30403040u
#define BUS_SPEED_SLOW 0xC0000000u
#define BUS_SPEED_FAST 0xC000C000u

enum {
    CRYSTAL_HZ = 24000000,
    MEGAHERTZ = 1000000,
    CON1_INTEGER_MODE = 1u << 12,
    POSTDIV1_SHIFT = 12,
    POSTDIV2_SHIFT = 6,
    PCLK_SHIFT_SHIFT = 8,
    FAST_CORE_HZ = 180000000,
    FAST_BUS_HZ = 300000000,
    FBDIV_MASK = 0xFFF,
    POSTDIV_MASK = 0x7,
    REFDIV_MASK = 0x3F,
    MODE_ON_PLL = 0x1,
    CRYSTAL_DIVIDER_SHIFT = 8,
    CRYSTAL_DIVIDER_MASK = 0x3F,
    CORE_DIVIDER_MASK = 0x7
};

typedef struct PllRow {
    uint32_t core_mhz;
    uint32_t refdiv;
    uint32_t fbdiv;
    uint32_t postdiv1;
    uint32_t postdiv2;
    uint32_t pclk_shift;
} PllRow;

static const PllRow ROWS[] = {
    {.core_mhz = 96, .refdiv = 6, .fbdiv = 120, .postdiv1 = 1, .postdiv2 = 5, .pclk_shift = 1},
    {.core_mhz = 200, .refdiv = 6, .fbdiv = 100, .postdiv1 = 1, .postdiv2 = 2, .pclk_shift = 2},
};

static PllSetting setting_of(const PllRow *row)
{
    uint32_t core_hz = CRYSTAL_HZ / row->refdiv * row->fbdiv / row->postdiv1 / row->postdiv2;
    return (PllSetting){
        .con0 = CON0_MASK | row->postdiv1 << POSTDIV1_SHIFT | row->fbdiv,
        .con1 = CON1_MASK | CON1_INTEGER_MODE | row->postdiv2 << POSTDIV2_SHIFT | row->refdiv,
        .clock_select = CLOCK_SELECT_UNDIVIDED,
        .pclk_select = PCLK_SELECT_MASK | row->pclk_shift << PCLK_SHIFT_SHIFT,
        .core_speed_bits = core_hz >= FAST_CORE_HZ ? CORE_SPEED_FAST : CORE_SPEED_SLOW,
        .bus_speed_bits = core_hz >= FAST_BUS_HZ ? BUS_SPEED_FAST : BUS_SPEED_SLOW,
        .core_hz = core_hz,
        .pclk_hz = core_hz >> row->pclk_shift,
    };
}

bool pll_setting_for(uint32_t core_mhz, PllSetting *setting)
{
    for (size_t index = 0; index < sizeof ROWS / sizeof ROWS[0]; index++) {
        if (ROWS[index].core_mhz == core_mhz) {
            *setting = setting_of(&ROWS[index]);
            return setting->core_hz == core_mhz * MEGAHERTZ;
        }
    }
    return false;
}

uint32_t pll_output_hz(uint32_t con0, uint32_t con1)
{
    uint32_t fbdiv = con0 & FBDIV_MASK;
    uint32_t postdiv1 = con0 >> POSTDIV1_SHIFT & POSTDIV_MASK;
    uint32_t refdiv = con1 & REFDIV_MASK;
    uint32_t postdiv2 = con1 >> POSTDIV2_SHIFT & POSTDIV_MASK;
    if (fbdiv == 0 || postdiv1 == 0 || refdiv == 0 || postdiv2 == 0) {
        return 0;
    }
    uint64_t hz = (uint64_t)CRYSTAL_HZ * fbdiv / refdiv / postdiv1 / postdiv2;
    return hz <= UINT32_MAX ? (uint32_t)hz : 0;
}

uint32_t pll_core_hz(uint32_t mode, uint32_t clock_select, uint32_t con0, uint32_t con1)
{
    if ((mode & MODE_ON_PLL) == 0) {
        return CRYSTAL_HZ / ((mode >> CRYSTAL_DIVIDER_SHIFT & CRYSTAL_DIVIDER_MASK) + 1);
    }
    return pll_output_hz(con0, con1) / ((clock_select & CORE_DIVIDER_MASK) + 1);
}
