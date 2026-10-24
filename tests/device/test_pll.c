#include "check.h"
#include "device/pll.h"

static void the_96_mhz_setting_is_stocks_floor_row(void)
{
    PllSetting setting;
    CHECK(pll_setting_for(96, &setting));
    CHECK_EQUAL(0xFFFF1078u, setting.con0);
    CHECK_EQUAL(0x11FF1146u, setting.con1);
    CHECK_EQUAL(0xFFFF0000u, setting.clock_select);
    CHECK_EQUAL(0x03000100u, setting.pclk_select);
    CHECK_EQUAL(0x30400000u, setting.core_speed_bits);
    CHECK_EQUAL(0xC0000000u, setting.bus_speed_bits);
    CHECK_EQUAL(96000000u, setting.core_hz);
    CHECK_EQUAL(48000000u, setting.pclk_hz);
}

static void the_200_mhz_setting_runs_the_pll_at_200(void)
{
    PllSetting setting;
    CHECK(pll_setting_for(200, &setting));
    CHECK_EQUAL(0xFFFF1064u, setting.con0);
    CHECK_EQUAL(0x11FF1086u, setting.con1);
    CHECK_EQUAL(0xFFFF0000u, setting.clock_select);
    CHECK_EQUAL(0x03000200u, setting.pclk_select);
    CHECK_EQUAL(0x30403040u, setting.core_speed_bits);
    CHECK_EQUAL(0xC0000000u, setting.bus_speed_bits);
    CHECK_EQUAL(200000000u, setting.core_hz);
    CHECK_EQUAL(50000000u, setting.pclk_hz);
}

static void other_speeds_are_refused(void)
{
    PllSetting setting;
    CHECK(!pll_setting_for(24, &setting));
    CHECK(!pll_setting_for(150, &setting));
    CHECK(!pll_setting_for(400, &setting));
}

static void the_pll_output_follows_the_register_fields(void)
{
    CHECK_EQUAL(96000000u, pll_output_hz(0x1078u, 0x1146u));
    CHECK_EQUAL(200000000u, pll_output_hz(0x1064u, 0x1086u));
    CHECK_EQUAL(400000000u, pll_output_hz(0x1064u, 0x1046u));
    CHECK_EQUAL(0u, pll_output_hz(0x0000u, 0x0000u));
}

static void the_core_clock_comes_from_the_pll_or_the_crystal(void)
{
    CHECK_EQUAL(96000000u, pll_core_hz(0x0001u, 0x0000u, 0x1078u, 0x1146u));
    CHECK_EQUAL(48000000u, pll_core_hz(0x0001u, 0x0001u, 0x1078u, 0x1146u));
    CHECK_EQUAL(24000000u, pll_core_hz(0x0000u, 0x0000u, 0x1078u, 0x1146u));
    CHECK_EQUAL(12000000u, pll_core_hz(0x0100u, 0x0000u, 0x1078u, 0x1146u));
}

static void measured_boot_registers_give_48_mhz(void)
{
    CHECK_EQUAL(48000000u, pll_core_hz(0x0001u, 0x0330u, 0x4020u, 0x9501u));
}

static void timekeeping_uses_the_whole_megahertz_of_the_core(void)
{
    CHECK_EQUAL(48u, pll_ticks_per_microsecond(48000000u));
    CHECK_EQUAL(96u, pll_ticks_per_microsecond(96000000u));
    CHECK_EQUAL(24u, pll_ticks_per_microsecond(0u));
    CHECK_EQUAL(24u, pll_ticks_per_microsecond(47500000u));
    CHECK_EQUAL(24u, pll_ticks_per_microsecond(900000000u));
}

static void pclk_is_the_core_shifted_by_the_select_field(void)
{
    CHECK_EQUAL(24000000u, pll_pclk_hz(48000000u, 0x0100u));
    CHECK_EQUAL(50000000u, pll_pclk_hz(200000000u, 0x0200u));
    CHECK_EQUAL(24000000u, pll_pclk_hz(24000000u, 0x0000u));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_96_mhz_setting_is_stocks_floor_row),
        TEST_CASE(the_200_mhz_setting_runs_the_pll_at_200),
        TEST_CASE(other_speeds_are_refused),
        TEST_CASE(the_pll_output_follows_the_register_fields),
        TEST_CASE(the_core_clock_comes_from_the_pll_or_the_crystal),
        TEST_CASE(measured_boot_registers_give_48_mhz),
        TEST_CASE(timekeeping_uses_the_whole_megahertz_of_the_core),
        TEST_CASE(pclk_is_the_core_shifted_by_the_select_field),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
