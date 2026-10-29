#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "diagnostics_rows.h"
#include "diagnostics_values.h"
#include "hal/audio.h"
#include "hal/power.h"
#include "hal/system.h"

static const char *year_value(const void *context)
{
    return context;
}

static const MenuItem LONG_NAME_ITEMS[] = {
    {.label = "Sigur Rós — Ágætis byrjun", .context = "1999", .value = year_value},
    {.label = "Godspeed You! Black Emperor — Lift Your Skinny Fists Like Antennas to Heaven",
     .context = "2000",
     .value = year_value},
    {.label = "Björk — Homogenic", .context = "1997", .value = year_value},
    {.label = "Sufjan Stevens — Illinois (Come On Feel the Illinoise)",
     .context = "2005",
     .value = year_value},
    {.label = "Motörhead — Ace of Spades", .context = "1980", .value = year_value},
    {.label = "Jóhann Jóhannsson — Orphée", .context = "2016", .value = year_value},
    {.label = "Ólafur Arnalds — re:member", .context = "2018", .value = year_value},
    {.label = "Röyksopp — Melody A.M.", .context = "2001", .value = year_value},
    {.label = "Café Tacvba — Re", .context = "1994", .value = year_value},
    {.label = "Beyoncé — Lemonade", .context = "2016", .value = year_value},
    {.label = "Mötley Crüe — Dr. Feelgood", .context = "1989", .value = year_value},
    {.label = "Daft Punk — Random Access Memories", .context = "2013", .value = year_value},
    {.label = "Explosions in the Sky — The Earth Is Not a Cold Dead Place",
     .context = "2003",
     .value = year_value},
    {.label = "Hania Rani — Esja", .context = "2019", .value = year_value},
    {.label = "Nils Frahm — All Melody", .context = "2018", .value = year_value},
    {.label = "The Avalanches — Since I Left You", .context = "2000", .value = year_value},
};

enum {
    LONG_NAME_COUNT = sizeof LONG_NAME_ITEMS / sizeof LONG_NAME_ITEMS[0]
};

static ScreenAction toggle_frame_time(void *context)
{
    AppOptions *options = context;
    options->show_frame_time = !options->show_frame_time;
    return screen_action_none();
}

static const char *frame_time_value(const void *context)
{
    const AppOptions *options = context;
    return options->show_frame_time ? "On" : "Off";
}

static ScreenAction toggle_power_taps(void *context)
{
    AppOptions *options = context;
    options->ignore_power_taps = !options->ignore_power_taps;
    return screen_action_none();
}

static const char *power_taps_value(const void *context)
{
    const AppOptions *options = context;
    return options->ignore_power_taps ? "Ignored" : "Normal";
}

static char clock_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction cycle_clock(void *context)
{
    ClockOption *clock = context;
    clock->failed = !hal_system_set_clock_mhz(diagnostics_next_clock_mhz(hal_system_clock_mhz()));
    return screen_action_none();
}

static const char *clock_value(const void *context)
{
    const ClockOption *clock = context;
    diagnostics_clock_text(hal_system_clock_mhz(), clock->failed, clock_text, sizeof clock_text);
    return clock_text;
}

static char high_ram_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_high_ram_test(void *context)
{
    HighRamOption *option = context;
    option->test = hal_system_high_ram_test();
    option->run = true;
    return screen_action_none();
}

static const char *high_ram_value(const void *context)
{
    const HighRamOption *option = context;
    diagnostics_high_ram_text(option->run, option->test, high_ram_text, sizeof high_ram_text);
    return high_ram_text;
}

static char loop_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_loop_benchmark(void *context)
{
    LoopOption *loop = context;
    loop->cycles = hal_system_loop_cycles();
    loop->run = true;
    return screen_action_none();
}

static const char *loop_value(const void *context)
{
    const LoopOption *loop = context;
    diagnostics_loop_text(loop->run, loop->cycles, loop_text, sizeof loop_text);
    return loop_text;
}

static char core1_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_test(void *context)
{
    Core1Option *core1 = context;
    core1->test = hal_system_core1_test();
    core1->run = true;
    return screen_action_none();
}

static const char *core1_value(const void *context)
{
    const Core1Option *core1 = context;
    diagnostics_core1_text(core1->run, core1->test, core1_text, sizeof core1_text);
    return core1_text;
}

static char core1_accel_text[DIAGNOSTICS_VALUE_CAPACITY];

static ScreenAction run_core1_accel_test(void *context)
{
    Core1AccelOption *accel = context;
    accel->result = hal_system_core1_accel_test();
    accel->run = true;
    return screen_action_none();
}

static const char *core1_accel_value(const void *context)
{
    const Core1AccelOption *accel = context;
    diagnostics_core1_accel_text(accel->run, accel->result, core1_accel_text,
                                 sizeof core1_accel_text);
    return core1_accel_text;
}

static char boot_hold_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *boot_hold_value(const void *context)
{
    (void)context;
    diagnostics_boot_hold_text(hal_power_boot_hold(), boot_hold_text, sizeof boot_hold_text);
    return boot_hold_text;
}

static ScreenAction run_stall_test(void *context)
{
    (void)context;
    hal_system_stall_test();
    return screen_action_none();
}

static ScreenAction run_watchdog_test(void *context)
{
    (void)context;
    hal_system_watchdog_test();
    return screen_action_none();
}

static ScreenAction play_test_tone(void *context)
{
    ToneOption *tone = context;
    tone->result = hal_audio_test_tone();
    tone->tried = true;
    return screen_action_none();
}

static char tone_text[DIAGNOSTICS_VALUE_CAPACITY];
static char boot_clocks_text[DIAGNOSTICS_VALUE_CAPACITY];
static char boot_pll_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *tone_value(const void *context)
{
    const ToneOption *tone = context;
    if (!tone->tried) {
        return "";
    }
    diagnostics_tone_text(tone->result, hal_audio_tone_detail(), tone_text, sizeof tone_text);
    return tone_text;
}

static const char *boot_clocks_value(const void *context)
{
    (void)context;
    diagnostics_boot_clocks_text(hal_system_boot_clocks(), boot_clocks_text,
                                 sizeof boot_clocks_text);
    return boot_clocks_text;
}

static const char *boot_pll_value(const void *context)
{
    (void)context;
    diagnostics_boot_pll_text(hal_system_boot_clocks(), boot_pll_text, sizeof boot_pll_text);
    return boot_pll_text;
}

static char usb_config_text[DIAGNOSTICS_VALUE_CAPACITY];
static char usb_phy_text[DIAGNOSTICS_VALUE_CAPACITY];
static char usb_fifo_text[DIAGNOSTICS_VALUE_CAPACITY];

static const char *usb_config_value(const void *context)
{
    (void)context;
    HalUsbCore core = hal_system_usb_core();
    diagnostics_usb_registers_text(core.captured, core.hwcfg2, core.hwcfg3, usb_config_text,
                                   sizeof usb_config_text);
    return usb_config_text;
}

static const char *usb_phy_value(const void *context)
{
    (void)context;
    HalUsbCore core = hal_system_usb_core();
    diagnostics_usb_registers_text(core.captured, core.hwcfg4, core.usbcfg, usb_phy_text,
                                   sizeof usb_phy_text);
    return usb_phy_text;
}

static const char *usb_fifo_value(const void *context)
{
    (void)context;
    HalUsbCore core = hal_system_usb_core();
    diagnostics_usb_registers_text(core.captured, core.rx_fifo, core.nptx_fifo, usb_fifo_text,
                                   sizeof usb_fifo_text);
    return usb_fifo_text;
}

static ScreenAction toggle_dma_tone(void *context)
{
    DmaToneOption *tone = context;
    if (hal_audio_dma_tone_status().running) {
        hal_audio_dma_tone_stop();
        tone->stopped = true;
        return screen_action_none();
    }
    tone->result = hal_audio_dma_tone_start(tone->sample_rate);
    tone->tried = true;
    tone->stopped = false;
    return screen_action_none();
}

enum {
    LOW_TONE_RATE = 44100,
    HIGH_TONE_RATE = 192000,
    TONE_176_RATE = 176400
};

static char dma_tone_text[DIAGNOSTICS_VALUE_CAPACITY];
static char high_rate_tone_text[DIAGNOSTICS_VALUE_CAPACITY];
static char tone_176_text[DIAGNOSTICS_VALUE_CAPACITY];

static char *tone_text_for(uint32_t sample_rate)
{
    switch (sample_rate) {
    case HIGH_TONE_RATE:
        return high_rate_tone_text;
    case TONE_176_RATE:
        return tone_176_text;
    default:
        return dma_tone_text;
    }
}

static const char *dma_tone_value(const void *context)
{
    const DmaToneOption *tone = context;
    char *text = tone_text_for(tone->sample_rate);
    DmaToneView view = {.tried = tone->tried,
                        .stopped = tone->stopped,
                        .result = tone->result,
                        .status = hal_audio_dma_tone_status(),
                        .detail = hal_audio_tone_detail()};
    diagnostics_dma_tone_text(view, text, DIAGNOSTICS_VALUE_CAPACITY);
    return text;
}

void diagnostics_menu_build(AppScreens *screens, AppOptions *options, const Theme *theme)
{
    Screen *long_names = app_screens_build_menu(&screens->long_names, "Long names", LONG_NAME_ITEMS,
                                                LONG_NAME_COUNT, theme);
    screens->diagnostics_items[0] =
        app_screens_open_item("Key test", test_pattern_screen_init(&screens->test_pattern));
    screens->diagnostics_items[1] =
        app_screens_open_item("Fonts", font_sample_screen_init(&screens->font_sample, theme));
    screens->diagnostics_items[2] = app_screens_open_item("Long names", long_names);
    screens->diagnostics_items[3] = (MenuItem){
        .label = "Frame time",
        .select = toggle_frame_time,
        .context = options,
        .value = frame_time_value,
    };
    screens->diagnostics_items[4] = (MenuItem){
        .label = "Firmware", .select = NULL, .context = NULL, .value = diagnostics_row_firmware};
    screens->clock = (ClockOption){.failed = false};
    screens->diagnostics_items[5] = (MenuItem){.label = "CPU clock",
                                               .select = cycle_clock,
                                               .context = &screens->clock,
                                               .value = clock_value};
    screens->diagnostics_items[6] =
        app_screens_open_item("Stopwatch", stopwatch_screen_init(&screens->stopwatch, theme));
    screens->tone = (ToneOption){.tried = false, .result = HAL_TONE_PLAYED};
    screens->diagnostics_items[7] = (MenuItem){.label = "Test tone",
                                               .select = play_test_tone,
                                               .context = &screens->tone,
                                               .value = tone_value};
    screens->dma_tone = (DmaToneOption){
        .sample_rate = LOW_TONE_RATE, .tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
    screens->high_rate_tone = (DmaToneOption){
        .sample_rate = HIGH_TONE_RATE, .tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
    screens->tone_176 = (DmaToneOption){
        .sample_rate = TONE_176_RATE, .tried = false, .stopped = false, .result = HAL_TONE_PLAYED};
    screens->diagnostics_items[8] = (MenuItem){.label = "DMA tone",
                                               .select = toggle_dma_tone,
                                               .context = &screens->dma_tone,
                                               .value = dma_tone_value};
    screens->diagnostics_items[9] = (MenuItem){
        .label = "Boot clocks", .select = NULL, .context = NULL, .value = boot_clocks_value};
    screens->diagnostics_items[10] =
        (MenuItem){.label = "Boot PLL", .select = NULL, .context = NULL, .value = boot_pll_value};
    screens->loop = (LoopOption){.run = false};
    screens->diagnostics_items[11] = (MenuItem){.label = "Loop cycles",
                                                .select = run_loop_benchmark,
                                                .context = &screens->loop,
                                                .value = loop_value};
    screens->core1 = (Core1Option){.run = false};
    screens->diagnostics_items[12] = (MenuItem){.label = "Core 1",
                                                .select = run_core1_test,
                                                .context = &screens->core1,
                                                .value = core1_value};
    screens->diagnostics_items[13] = (MenuItem){
        .label = "Battery", .select = NULL, .context = NULL, .value = diagnostics_row_battery};
    screens->diagnostics_items[14] =
        (MenuItem){.label = "Boot hold", .select = NULL, .context = NULL, .value = boot_hold_value};
    screens->diagnostics_items[15] = (MenuItem){
        .label = "Last start", .select = NULL, .context = NULL, .value = diagnostics_row_start};
    screens->diagnostics_items[16] = (MenuItem){
        .label = "Last step", .select = NULL, .context = NULL, .value = diagnostics_row_last_step};
    screens->diagnostics_items[17] = (MenuItem){.label = "Last battery",
                                                .select = NULL,
                                                .context = NULL,
                                                .value = diagnostics_row_last_battery};
    screens->diagnostics_items[18] = (MenuItem){.label = "Bluetooth",
                                                .select = bluetooth_screen_open_details,
                                                .context = &screens->bluetooth,
                                                .value = NULL,
                                                .opens = true};
    screens->diagnostics_items[19] =
        (MenuItem){.label = "Stall test", .select = run_stall_test, .context = NULL, .value = NULL};
    screens->high_ram = (HighRamOption){.run = false};
    screens->diagnostics_items[21] = (MenuItem){.label = "High RAM",
                                                .select = run_high_ram_test,
                                                .context = &screens->high_ram,
                                                .value = high_ram_value};
    screens->diagnostics_items[20] = (MenuItem){.label = "Power taps",
                                                .select = toggle_power_taps,
                                                .context = options,
                                                .value = power_taps_value};
    screens->diagnostics_items[22] = (MenuItem){.label = "Jack 3.5",
                                                .select = NULL,
                                                .context = NULL,
                                                .value = diagnostics_row_jack_single_ended};
    screens->diagnostics_items[23] = (MenuItem){.label = "Jack 4.4",
                                                .select = NULL,
                                                .context = NULL,
                                                .value = diagnostics_row_jack_balanced};
    screens->core1_accel = (Core1AccelOption){.run = false};
    screens->diagnostics_items[24] = (MenuItem){.label = "LPC accel",
                                                .select = run_core1_accel_test,
                                                .context = &screens->core1_accel,
                                                .value = core1_accel_value};
    screens->diagnostics_items[25] = (MenuItem){.label = "Tone 192k",
                                                .select = toggle_dma_tone,
                                                .context = &screens->high_rate_tone,
                                                .value = dma_tone_value};
    screens->diagnostics_items[26] = (MenuItem){
        .label = "Watchdog test", .select = run_watchdog_test, .context = NULL, .value = NULL};
    screens->diagnostics_items[27] = (MenuItem){.label = "Tone 176k",
                                                .select = toggle_dma_tone,
                                                .context = &screens->tone_176,
                                                .value = dma_tone_value};
    screens->diagnostics_items[28] = (MenuItem){
        .label = "USB HW2 HW3", .select = NULL, .context = NULL, .value = usb_config_value};
    screens->diagnostics_items[29] =
        (MenuItem){.label = "USB HW4 CFG", .select = NULL, .context = NULL, .value = usb_phy_value};
    screens->diagnostics_items[30] =
        (MenuItem){.label = "USB RX TX", .select = NULL, .context = NULL, .value = usb_fifo_value};
    screens->diagnostics_items[31] = (MenuItem){
        .label = "Last USB", .select = NULL, .context = NULL, .value = diagnostics_row_usb_last};
    app_screens_build_menu(&screens->diagnostics, "Diagnostics", screens->diagnostics_items,
                           DIAGNOSTICS_ITEM_COUNT, theme);
}
