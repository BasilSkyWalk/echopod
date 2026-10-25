#include "usb_dac_session.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/string_builder.h"
#include "device/usb_audio.h"
#include "device/usb_audio_sink.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/input.h"
#include "settings/settings.h"
#include "usb.h"
#include "usb_report.h"

enum {
    HOST_TIMEOUT_MS = 3000,
    STREAM_IDLE_MS = 100,
    BACK_HOLD_MS = 1000,
    POLLS_PER_SERVICE = 4,
    NOTICE_CAPACITY = 64,
    HZ_PER_KHZ = 1000,
    BITS_PER_BYTE = 8,
    COUNTER_REFRESH_MS = 1000,
    PAGE_COUNT = 4,
    VOLUME_REPEAT_DELAY_MS = 400,
    VOLUME_REPEAT_MS = 80
};

static const char TITLE[] = "USB DAC";

static UsbPort port;
static UsbAudio audio;
static UsbAudioSink sink;
static uint32_t open_rate;
static uint32_t last_packet_ms;
static uint32_t back_pressed_ms;
static bool receiving;
static bool playing;
static uint32_t counters_shown_ms;
static uint32_t page;
static uint8_t dac_volume;
static bool dac_volume_known;
static bool holding_volume;
static int held_step;
static uint32_t next_repeat_ms;

void hal_system_set_dac_volume(uint8_t level)
{
    dac_volume = level;
    dac_volume_known = true;
}

static void receive_stream(const uint8_t *bytes, uint32_t length)
{
    if (receiving) {
        last_packet_ms = hal_clock_milliseconds();
        usb_audio_sink_write(&sink, bytes, length, board_audio_stream_consumed());
    }
}

static void serve_usb(void)
{
    for (int poll = 0; poll < POLLS_PER_SERVICE; poll++) {
        usb_poll(&port);
    }
}

bool usb_dac_session_listen(void)
{
    port = (UsbPort){.audio = &audio, .receive_stream = receive_stream};
    usb_audio_init(&audio, &port.device);
    usb_power_up();
    if (usb_connect(&port)) {
        return true;
    }
    usb_power_down();
    return false;
}

static void append_volume(StringBuilder *line)
{
    string_builder_append(line, " \xC2\xB7 Volume ");
    string_builder_append_unsigned(line, dac_volume);
}

static void append_format(StringBuilder *line)
{
    string_builder_append_unsigned(line, audio.rate / HZ_PER_KHZ);
    if (audio.rate % HZ_PER_KHZ != 0) {
        string_builder_append_char(line, '.');
        string_builder_append_unsigned(line, audio.rate % HZ_PER_KHZ / 100);
    }
    string_builder_append(line, " kHz ");
    string_builder_append_unsigned(line, BITS_PER_BYTE * usb_audio_sample_bytes(&audio));
    string_builder_append(line, "-bit");
    append_volume(line);
}

static void show_state(void)
{
    char text[NOTICE_CAPACITY];
    StringBuilder line = string_builder_make(text, sizeof text);
    if (usb_audio_sample_bytes(&audio) == 0) {
        string_builder_append(&line, "Choose EchoPod as the sound output");
        append_volume(&line);
    } else if (open_rate == 0) {
        string_builder_append(&line, "The audio output didn't start.");
    } else {
        append_format(&line);
    }
    app_show_notice(TITLE, text);
}

static void append_count(StringBuilder *line, char label, uint32_t value)
{
    string_builder_append_char(line, ' ');
    string_builder_append_char(line, label);
    string_builder_append_unsigned(line, value);
}

static void append_word(StringBuilder *line, char label, uint32_t value)
{
    string_builder_append_char(line, ' ');
    string_builder_append_char(line, label);
    usb_report_append_hex(line, value, 8);
}

static void counter_page(StringBuilder *top, StringBuilder *line)
{
    UsbIsoStats iso = usb_iso_stats();
    string_builder_append(top, "1 USB");
    append_count(top, 'a', iso.armed);
    append_count(top, 'r', iso.received);
    append_count(top, 'i', iso.incomplete);
    string_builder_append_unsigned(line, audio.rate / HZ_PER_KHZ);
    string_builder_append(line, playing ? "k on" : "k off");
    append_count(line, 'w', sink.written);
    append_count(line, 'c', board_audio_stream_consumed());
    append_count(line, 'x', sink.stats.realigned);
}

static void endpoint_page(StringBuilder *top, StringBuilder *line)
{
    UsbIsoRegisters registers = usb_iso_registers();
    string_builder_append(top, "2 EP");
    append_word(top, 'C', registers.control);
    append_word(top, 'T', registers.size);
    append_word(line, 'E', registers.events);
    append_word(line, 'S', registers.status);
}

static void fifo_page(StringBuilder *top, StringBuilder *line)
{
    UsbIsoRegisters registers = usb_iso_registers();
    string_builder_append(top, "3 FIFO");
    append_word(top, 'R', registers.rx_fifo);
    append_word(top, 'N', registers.tx0_fifo);
    append_word(line, 'M', registers.interrupts);
    append_word(line, 'H', registers.hardware);
}

static void show_counters(void)
{
    char title[NOTICE_CAPACITY];
    char text[NOTICE_CAPACITY];
    StringBuilder top = string_builder_make(title, sizeof title);
    StringBuilder line = string_builder_make(text, sizeof text);
    if (page == 2) {
        endpoint_page(&top, &line);
    } else if (page == 3) {
        fifo_page(&top, &line);
    } else {
        counter_page(&top, &line);
    }
    app_show_notice(title, text);
}

static void show_page(void)
{
    if (page == 0) {
        show_state();
    } else {
        show_counters();
    }
}

static void refresh_counters(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    if (page != 0 && now_ms - counters_shown_ms >= COUNTER_REFRESH_MS) {
        counters_shown_ms = now_ms;
        show_counters();
    }
}

static void stop_output(void)
{
    if (playing) {
        board_audio_stream_pause();
        playing = false;
    }
    usb_audio_sink_start(&sink, audio.rate, usb_audio_sample_bytes(&audio));
}

static void follow_format(void)
{
    receiving = false;
    stop_output();
    if (usb_audio_sample_bytes(&audio) != 0 && audio.rate != open_rate) {
        open_rate = board_audio_stream_open(audio.rate, serve_usb) ? audio.rate : 0;
    }
    receiving = open_rate != 0 && usb_audio_sample_bytes(&audio) != 0;
    show_page();
}

static void follow_host(void)
{
    uint8_t changes = usb_audio_take_changes(&audio);
    if ((changes & USB_AUDIO_CHANGED_VOLUME) != 0) {
        board_audio_set_attenuation(usb_audio_attenuation(&audio));
    }
    if ((changes & USB_AUDIO_CHANGED_FORMAT) != 0) {
        follow_format();
    }
}

static void keep_output(void)
{
    if (playing && hal_clock_milliseconds() - last_packet_ms > STREAM_IDLE_MS) {
        stop_output();
    } else if (!playing && receiving && usb_audio_sink_primed(&sink)) {
        playing = board_audio_stream_play();
        usb_audio_sink_set_playing(&sink, playing);
    }
}

static void step_volume(int step)
{
    int level = dac_volume + step;
    if (level < 0 || level > HAL_AUDIO_VOLUME_MAX) {
        return;
    }
    dac_volume = (uint8_t)level;
    hal_audio_set_volume(dac_volume);
    if (page == 0) {
        show_state();
    }
}

static void start_holding(int step, uint32_t now_ms)
{
    step_volume(step);
    holding_volume = true;
    held_step = step;
    next_repeat_ms = now_ms + VOLUME_REPEAT_DELAY_MS;
}

static void repeat_volume(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    if (holding_volume && (int32_t)(now_ms - next_repeat_ms) >= 0) {
        step_volume(held_step);
        next_repeat_ms = now_ms + VOLUME_REPEAT_MS;
    }
}

static void follow_volume_key(HalKeyTransition key)
{
    int step = key.key == KEY_VOLUME_UP ? 1 : -1;
    if (key.pressed) {
        start_holding(step, key.time_ms);
    } else if (holding_volume && held_step == step) {
        holding_volume = false;
    }
}

static bool back_released_after_hold(HalKeyTransition key)
{
    if (key.pressed) {
        back_pressed_ms = key.time_ms;
        return false;
    }
    return key.time_ms - back_pressed_ms >= BACK_HOLD_MS;
}

static bool user_left(void)
{
    HalKeyTransition key;
    while (hal_input_next_transition(&key)) {
        if (key.key == KEY_BACK && back_released_after_hold(key)) {
            return true;
        }
        if (key.pressed && key.key == KEY_PLAY) {
            page = (page + 1) % PAGE_COUNT;
            counters_shown_ms = hal_clock_milliseconds();
            show_page();
        }
        if (key.key == KEY_VOLUME_UP || key.key == KEY_VOLUME_DOWN) {
            follow_volume_key(key);
        }
    }
    return false;
}

static void serve(void)
{
    back_pressed_ms = hal_clock_milliseconds();
    while (usb_vbus_present() && !user_left()) {
        serve_usb();
        hal_audio_service();
        follow_host();
        keep_output();
        repeat_volume();
        refresh_counters();
    }
}

static void start_session(void)
{
    uint32_t frames = 0;
    uint32_t *ring = board_audio_stream_ring(&frames);
    usb_audio_sink_init(&sink, ring, frames);
    open_rate = 0;
    receiving = false;
    playing = false;
    page = 0;
    holding_volume = false;
    dac_volume = dac_volume_known ? dac_volume : (uint8_t)SETTINGS_DEFAULT_DAC_VOLUME;
    hal_audio_set_volume(dac_volume);
}

void usb_dac_session_run(void)
{
    app_show_notice(TITLE, "Computer found. Starting...");
    app_release_audio();
    board_audio_shut_down();
    uint8_t level = board_audio_volume_level();
    start_session();
    uint8_t starting_volume = dac_volume;
    if (usb_wait_for_host(&port, HOST_TIMEOUT_MS)) {
        show_state();
        serve();
    }
    receiving = false;
    stop_output();
    board_audio_stream_close();
    board_audio_set_attenuation(0);
    hal_audio_set_volume(level);
    usb_power_down();
    if (dac_volume != starting_volume) {
        app_remember_dac_volume(dac_volume);
    }
}
