#include "usb_session.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "device/usb_audio.h"
#include "device/usb_audio_sink.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/power.h"
#include "settings/settings.h"
#include "usb.h"

enum {
    HOST_TIMEOUT_MS = 3000,
    STREAM_IDLE_MS = 100,
    POLLS_PER_SERVICE = 4,
    NOTICE_CAPACITY = 192,
    NUMBER_CAPACITY = 12,
    FORMAT_CAPACITY = 16,
    HZ_PER_KHZ = 1000,
    BITS_PER_BYTE = 8,
    NOTICE_REFRESH_MS = 1000,
    REPORT_QUEUE_CAPACITY = 8
};

static const char TITLE[] = "USB DAC";

static UsbShared *shared;
static UsbPort port;
static UsbAudio audio;
static UsbAudioSink sink;
static uint32_t open_rate;
static uint32_t last_packet_ms;
static bool receiving;
static bool playing;
static uint32_t notice_refreshed_ms;
static char shown_text[NOTICE_CAPACITY];
static uint8_t reports[REPORT_QUEUE_CAPACITY];
static uint8_t report_start;
static uint8_t report_count;

static void receive_stream(const uint8_t *bytes, uint32_t length)
{
    if (receiving) {
        last_packet_ms = hal_clock_milliseconds();
        usb_audio_sink_count_frame(&sink, port.packet_frame);
        usb_audio_sink_write(&sink, bytes, length, board_audio_stream_consumed());
    }
}

static void serve_usb(void)
{
    for (int poll = 0; poll < POLLS_PER_SERVICE; poll++) {
        usb_poll(&port);
    }
}

static void number_text(char *text, size_t capacity, uint32_t value)
{
    StringBuilder number = string_builder_make(text, capacity);
    string_builder_append_unsigned(&number, value);
}

static void append_format(StringBuilder *line)
{
    char rate[FORMAT_CAPACITY];
    char bits[NUMBER_CAPACITY];
    char volume[NUMBER_CAPACITY];
    StringBuilder rate_builder = string_builder_make(rate, sizeof rate);
    string_builder_append_unsigned(&rate_builder, audio.rate / HZ_PER_KHZ);
    if (audio.rate % HZ_PER_KHZ != 0) {
        string_builder_append_char(&rate_builder, '.');
        string_builder_append_unsigned(&rate_builder, audio.rate % HZ_PER_KHZ / 100);
    }
    number_text(bits, sizeof bits, BITS_PER_BYTE * usb_audio_sample_bytes(&audio));
    number_text(volume, sizeof volume, shared->dac_volume);
    const char *arguments[] = {rate, bits, volume};
    i18n_format(line, STR_USB_FORMAT_VOLUME, arguments, 3);
}

static void append_counters(StringBuilder *line)
{
    char missed[NUMBER_CAPACITY];
    char dropped[NUMBER_CAPACITY];
    char repeated[NUMBER_CAPACITY];
    char restarts[NUMBER_CAPACITY];
    number_text(missed, sizeof missed, sink.stats.missed);
    number_text(dropped, sizeof dropped, sink.stats.dropped);
    number_text(repeated, sizeof repeated, sink.stats.repeated);
    number_text(restarts, sizeof restarts, sink.stats.realigned);
    const char *arguments[] = {missed, dropped, repeated, restarts};
    string_builder_append(line, " \xC2\xB7 ");
    i18n_format(line, STR_USB_COUNTERS, arguments, 4);
}

static void append_battery(StringBuilder *line)
{
    HalBattery battery = hal_power_battery();
    char percent[NUMBER_CAPACITY];
    number_text(percent, sizeof percent, battery.percent);
    const char *arguments[] = {percent};
    string_builder_append(line, " · ");
    if (!battery.known) {
        string_builder_append(line, i18n_text(STR_USB_BATTERY_UNKNOWN));
        return;
    }
    i18n_format(line, battery.charging ? STR_USB_BATTERY_CHARGING : STR_USB_BATTERY, arguments, 1);
}

static void state_text(char *text, size_t capacity)
{
    StringBuilder line = string_builder_make(text, capacity);
    if (usb_audio_sample_bytes(&audio) == 0) {
        char volume[NUMBER_CAPACITY];
        number_text(volume, sizeof volume, shared->dac_volume);
        const char *arguments[] = {volume};
        i18n_format(&line, STR_USB_CHOOSE_OUTPUT, arguments, 1);
    } else if (open_rate == 0) {
        string_builder_append(&line, i18n_text(STR_USB_OUTPUT_FAILED));
    } else {
        append_format(&line);
        append_counters(&line);
    }
    append_battery(&line);
}

static bool text_differs(const char *text)
{
    for (size_t index = 0;; index++) {
        if (text[index] != shown_text[index]) {
            return true;
        }
        if (text[index] == '\0') {
            return false;
        }
    }
}

static void show_state(void)
{
    state_text(shown_text, sizeof shown_text);
    notice_refreshed_ms = hal_clock_milliseconds();
    app_show_notice(TITLE, shown_text);
}

static void refresh_state(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    if (playing || now_ms - notice_refreshed_ms < NOTICE_REFRESH_MS) {
        return;
    }
    notice_refreshed_ms = now_ms;
    char text[NOTICE_CAPACITY];
    state_text(text, sizeof text);
    if (text_differs(text)) {
        show_state();
    }
}

static void apply_attenuation(void)
{
    uint8_t player = board_audio_stream_attenuation();
    uint8_t host = usb_audio_attenuation(&audio);
    uint32_t total = player + (uint32_t)host;
    bool muted = player == USB_AUDIO_SINK_MUTED || host == USB_AUDIO_MUTED;
    usb_audio_sink_set_attenuation(
        &sink, muted || total >= USB_AUDIO_SINK_MUTED ? USB_AUDIO_SINK_MUTED : (uint8_t)total);
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
    apply_attenuation();
    receiving = open_rate != 0 && usb_audio_sample_bytes(&audio) != 0;
    show_state();
}

static void follow_host(void)
{
    uint8_t changes = usb_audio_take_changes(&audio);
    if ((changes & USB_AUDIO_CHANGED_VOLUME) != 0) {
        apply_attenuation();
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
    int level = shared->dac_volume + step;
    if (level < 0 || level > HAL_AUDIO_VOLUME_MAX) {
        return;
    }
    shared->dac_volume = (uint8_t)level;
    hal_audio_set_volume(shared->dac_volume);
    apply_attenuation();
    show_state();
}

static void queue_report(uint8_t report)
{
    if (report_count + 2 > REPORT_QUEUE_CAPACITY) {
        return;
    }
    reports[(report_start + report_count++) % REPORT_QUEUE_CAPACITY] = report;
    reports[(report_start + report_count++) % REPORT_QUEUE_CAPACITY] = 0;
}

static void send_reports(void)
{
    if (report_count > 0 && usb_send_report(&port, reports[report_start])) {
        report_start = (uint8_t)((report_start + 1) % REPORT_QUEUE_CAPACITY);
        report_count--;
    }
}

static bool user_left(void)
{
    for (;;) {
        switch (app_dac_next_key()) {
        case APP_DAC_KEY_NONE:
            return false;
        case APP_DAC_KEY_VOLUME_UP:
            step_volume(1);
            break;
        case APP_DAC_KEY_VOLUME_DOWN:
            step_volume(-1);
            break;
        case APP_DAC_KEY_PLAY_PAUSE:
            queue_report(USB_HID_PLAY_PAUSE);
            break;
        case APP_DAC_KEY_NEXT:
            queue_report(USB_HID_NEXT);
            break;
        case APP_DAC_KEY_PREVIOUS:
            queue_report(USB_HID_PREVIOUS);
            break;
        case APP_DAC_KEY_LEAVE:
            return true;
        }
    }
}

static void serve(void)
{
    app_dac_keys_start();
    while (usb_vbus_present() && !user_left()) {
        serve_usb();
        hal_audio_service();
        board_display_service();
        follow_host();
        keep_output();
        send_reports();
        refresh_state();
    }
    app_dac_keys_stop();
}

static void start_session(void)
{
    uint32_t frames = 0;
    uint32_t *ring = board_audio_stream_ring(&frames);
    usb_audio_sink_init(&sink, ring, frames);
    open_rate = 0;
    receiving = false;
    playing = false;
    report_count = 0;
    port = (UsbPort){.link = shared->link,
                     .bulk_packet = USB_PACKET_SIZE,
                     .audio = &audio,
                     .receive_stream = receive_stream};
    usb_audio_init(&audio, &port.device);
    if (!shared->dac_volume_known) {
        shared->dac_volume = (uint8_t)SETTINGS_DEFAULT_DAC_VOLUME;
    }
    hal_audio_set_volume(shared->dac_volume);
}

void usb_dac_session(UsbShared *state)
{
    shared = state;
    app_show_notice(TITLE, i18n_text(STR_USB_COMPUTER_FOUND));
    uint8_t level = board_audio_volume_level();
    start_session();
    uint8_t starting_volume = shared->dac_volume;
    if (usb_wait_for_host(&port, HOST_TIMEOUT_MS)) {
        show_state();
        serve();
    }
    receiving = false;
    stop_output();
    board_audio_stream_close();
    hal_audio_set_volume(level);
    usb_power_down();
    if (shared->dac_volume != starting_volume) {
        app_remember_dac_volume(shared->dac_volume);
    }
}
