#include "diagnostics_values.h"

#include "core/string_builder.h"

const char *diagnostics_tone_result_text(HalToneResult result)
{
    switch (result) {
    case HAL_TONE_PLAYED:
        return "已播放";
    case HAL_TONE_NO_DAC:
        return "无 DAC";
    case HAL_TONE_SETUP_FAILED:
        return "初始化失败";
    case HAL_TONE_NOT_READY:
        return "未就绪";
    case HAL_TONE_STALLED:
        return "I2S 卡住";
    case HAL_TONE_NO_DMAC:
        return "无 DMAC";
    }
    return "";
}

static void append_hex(StringBuilder *builder, uint32_t value, int digits)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (int digit = digits - 1; digit >= 0; digit--) {
        string_builder_append_char(builder, DIGITS[(value >> (4 * digit)) & 0xFu]);
    }
}

void diagnostics_tone_text(HalToneResult result, HalToneDetail detail, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, diagnostics_tone_result_text(result));
    if (result == HAL_TONE_NO_DAC && detail.stage != 0) {
        string_builder_append(&builder, " s");
        string_builder_append_unsigned(&builder, detail.stage);
        string_builder_append_char(&builder, ' ');
        append_hex(&builder, detail.pending, 2);
    }
}

void diagnostics_boot_clocks_text(HalBootClocks clocks, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_hex(&builder, clocks.mode, 4);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, clocks.clock_select, 4);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, clocks.pclk_select, 4);
}

void diagnostics_usb_registers_text(bool captured, uint32_t first, uint32_t second, char *text,
                                    size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!captured) {
        string_builder_append(&builder, "请先插上 USB");
        return;
    }
    append_hex(&builder, first, 8);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, second, 8);
}

static uint32_t drives_in(uint8_t mask)
{
    return (uint32_t)(mask & 1u) + (uint32_t)((mask >> 1) & 1u);
}

void diagnostics_usb_last_text(HalUsbLast last, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (last.end == HAL_USB_END_NONE) {
        string_builder_append(&builder, "尚无会话");
        return;
    }
    if (last.end == HAL_USB_END_NO_HOST) {
        string_builder_append(&builder, "无电脑");
        return;
    }
    if (last.end == HAL_USB_END_EJECTED) {
        string_builder_append(&builder, "弹出 W");
    } else {
        string_builder_append(&builder, "已拔出 ");
        string_builder_append_unsigned(&builder, drives_in(last.left_mounted));
        string_builder_append(&builder, " 启动 W");
    }
    string_builder_append_unsigned(&builder, last.internal_written);
    string_builder_append_char(&builder, '/');
    string_builder_append_unsigned(&builder, last.card_written);
    if (last.writes_after_eject > 0) {
        string_builder_append(&builder, " A");
        string_builder_append_unsigned(&builder, last.writes_after_eject);
    }
}

void diagnostics_dma_tone_text(DmaToneView view, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!view.tried) {
        return;
    }
    if (view.result != HAL_TONE_PLAYED) {
        diagnostics_tone_text(view.result, view.detail, text, capacity);
        return;
    }
    if (view.status.running) {
        string_builder_append(&builder, "播放中 ");
        string_builder_append_unsigned(&builder, view.status.position);
        return;
    }
    string_builder_append(&builder, view.stopped ? "已停止" : "已结束");
}

void diagnostics_boot_pll_text(HalBootClocks clocks, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    append_hex(&builder, clocks.pll_con0, 4);
    string_builder_append_char(&builder, ' ');
    append_hex(&builder, clocks.pll_con1, 4);
    string_builder_append_char(&builder, ' ');
    string_builder_append_unsigned(&builder, clocks.core_hz / 1000000u);
    string_builder_append(&builder, " MHz");
}

static const uint32_t CLOCK_STEPS_MHZ[] = {24, 96, 200};

uint32_t diagnostics_next_clock_mhz(uint32_t current_mhz)
{
    for (size_t index = 0; index < sizeof CLOCK_STEPS_MHZ / sizeof CLOCK_STEPS_MHZ[0]; index++) {
        if (CLOCK_STEPS_MHZ[index] > current_mhz) {
            return CLOCK_STEPS_MHZ[index];
        }
    }
    return CLOCK_STEPS_MHZ[0];
}

void diagnostics_clock_text(uint32_t mhz, bool failed, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (failed) {
        string_builder_append(&builder, "未锁定");
        return;
    }
    string_builder_append_unsigned(&builder, mhz);
    string_builder_append(&builder, " MHz");
}

static void append_centi(StringBuilder *builder, const char *prefix, uint32_t centi)
{
    string_builder_append(builder, prefix);
    string_builder_append_unsigned(builder, centi / 100);
    string_builder_append_char(builder, '.');
    string_builder_append_char(builder, (char)('0' + centi / 10 % 10));
    string_builder_append_char(builder, (char)('0' + centi % 10));
}

void diagnostics_loop_text(bool run, HalLoopCycles cycles, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    append_centi(&builder, "r", cycles.register_centi);
    append_centi(&builder, " m", cycles.memory_centi);
}

static const char *core1_failure_text(HalCore1Status status)
{
    switch (status) {
    case HAL_CORE1_RAN:
        return "";
    case HAL_CORE1_UNAVAILABLE:
        return "不存在";
    case HAL_CORE1_NEEDS_PLL:
        return "需要 96+ MHz";
    case HAL_CORE1_NO_POWER:
        return "无电源";
    case HAL_CORE1_NO_REPLY:
        return "无响应";
    case HAL_CORE1_BAD_REPLY:
        return "响应异常";
    case HAL_CORE1_NO_STOP:
        return "无法停止";
    case HAL_CORE1_BUSY:
        return "正在播放音乐";
    }
    return "";
}

static const char *step_name(HalStartStep step, bool finished)
{
    static const char *const OPEN_NAMES[HAL_START_STEP_COUNT] = {"",
                                                                 "背光开",
                                                                 "背光关",
                                                                 "切换时钟",
                                                                 "Core 1 启动",
                                                                 "Core 1 停止",
                                                                 "写卡",
                                                                 "看门狗测试"};
    static const char *const DONE_NAMES[HAL_START_STEP_COUNT] = {"",
                                                                 "背光开",
                                                                 "背光关",
                                                                 "切换时钟",
                                                                 "Core 1 启动",
                                                                 "Core 1 停止",
                                                                 "写卡",
                                                                 "看门狗测试"};
    return finished ? DONE_NAMES[step] : OPEN_NAMES[step];
}

void diagnostics_last_step_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm || info.last_step == HAL_START_STEP_NONE ||
        info.last_step >= HAL_START_STEP_COUNT) {
        string_builder_append(&builder, "无");
        return;
    }
    if (!info.last_step_finished) {
        string_builder_append(&builder, "输入 ");
    }
    string_builder_append(&builder, step_name(info.last_step, info.last_step_finished));
    string_builder_append(&builder, " 于 ");
    string_builder_append_unsigned(&builder, info.last_step_ms / 1000u);
    string_builder_append(&builder, " s");
}

void diagnostics_last_battery_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm || info.last_millivolts == 0) {
        string_builder_append(&builder, "无读数");
        return;
    }
    string_builder_append_unsigned(&builder, info.last_millivolts);
    string_builder_append(&builder, " mV");
}

void diagnostics_core1_text(bool run, HalCore1Test test, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    if (test.status != HAL_CORE1_RAN) {
        string_builder_append(&builder, core1_failure_text(test.status));
        return;
    }
    string_builder_append_unsigned(&builder, test.mhz);
    append_centi(&builder, " r", test.cycles.register_centi);
    append_centi(&builder, " m", test.cycles.memory_centi);
}

static void append_tenths(StringBuilder *builder, uint32_t microseconds)
{
    uint32_t tenths = (microseconds + 50u) / 100u;
    string_builder_append_unsigned(builder, tenths / 10u);
    string_builder_append_char(builder, '.');
    string_builder_append_unsigned(builder, tenths % 10u);
}

void diagnostics_core1_accel_text(bool run, HalCore1Accel result, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    if (result.status != HAL_CORE1_RAN) {
        string_builder_append(&builder, core1_failure_text(result.status));
        return;
    }
    if (result.timed_out) {
        string_builder_append(&builder, "超时于 t");
        string_builder_append_unsigned(&builder, result.failed_vector);
        return;
    }
    string_builder_append(&builder, result.narrow_ok ? "N 正常" : "N 异常");
    string_builder_append(&builder, result.wide_ok ? " W 正常" : " W 异常");
    if (result.failed_vector > 0) {
        string_builder_append(&builder, " t");
        string_builder_append_unsigned(&builder, result.failed_vector);
        string_builder_append_char(&builder, '@');
        string_builder_append_unsigned(&builder, result.failed_sample);
        return;
    }
    string_builder_append_char(&builder, ' ');
    append_tenths(&builder, result.hardware_us);
    string_builder_append_char(&builder, '/');
    append_tenths(&builder, result.software_us);
    string_builder_append(&builder, " ms");
}

void diagnostics_battery_text(HalBattery battery, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!battery.known) {
        string_builder_append(&builder, "无读数");
        return;
    }
    string_builder_append_unsigned(&builder, battery.millivolts);
    string_builder_append(&builder, " mV");
    if (battery.usb_power) {
        string_builder_append(&builder, battery.charging ? " 充电中" : " 已充满");
    }
}

void diagnostics_boot_hold_text(HalPowerHold hold, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, "fn");
    string_builder_append_unsigned(&builder, hold.function);
    string_builder_append(&builder, hold.output ? " 输出" : " 输入");
    string_builder_append(&builder, hold.high ? " 高" : " 低");
    if (hold.usb_power) {
        string_builder_append(&builder, " · USB");
    }
    if (hold.key_down) {
        string_builder_append(&builder, " · key");
    }
}

void diagnostics_start_text(HalStartInfo info, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!info.warm) {
        string_builder_append(&builder, "冷启动");
        return;
    }
    if (info.fault_status != 0 || info.hard_fault_status != 0) {
        string_builder_append(&builder, "故障于 ");
        append_hex(&builder, info.fault_pc, 8);
        return;
    }
    string_builder_append_char(&builder, '#');
    string_builder_append_unsigned(&builder, info.starts);
    if (info.previous_power_off) {
        string_builder_append(&builder, " 关机后");
        return;
    }
    string_builder_append(&builder, " 重启于 ");
    string_builder_append_unsigned(&builder, info.previous_uptime_ms / 1000u);
    string_builder_append(&builder, " s");
}

void diagnostics_high_ram_text(bool run, HalHighRamTest test, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    if (!run) {
        return;
    }
    switch (test.status) {
    case HAL_HIGH_RAM_OK:
        string_builder_append(&builder, "正常 ");
        string_builder_append_unsigned(&builder, test.size / 1024u);
        string_builder_append(&builder, " KB，调用正常");
        return;
    case HAL_HIGH_RAM_UNAVAILABLE:
        string_builder_append(&builder, "本机不支持");
        return;
    case HAL_HIGH_RAM_BAD_MEMORY:
        string_builder_append(&builder, "异常于 ");
        append_hex(&builder, test.bad_address, 8);
        return;
    case HAL_HIGH_RAM_BAD_CALL:
        string_builder_append(&builder, "返回 ");
        append_hex(&builder, test.call_result, 8);
        return;
    }
}

void diagnostics_jack_pin_text(HalJackPin pin, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, pin.high ? "高，启动" : "低，启动");
    string_builder_append_unsigned(&builder, pin.boot_function);
    string_builder_append(&builder, pin.boot_output ? " 输出" : " 输入");
    if (pin.pull) {
        string_builder_append(&builder, " 上拉");
    }
}
