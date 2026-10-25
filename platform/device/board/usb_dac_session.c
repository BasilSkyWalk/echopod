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
#include "usb.h"

enum {
    HOST_TIMEOUT_MS = 3000,
    STREAM_IDLE_MS = 100,
    BACK_HOLD_MS = 1000,
    POLLS_PER_SERVICE = 4,
    NOTICE_CAPACITY = 64,
    HZ_PER_KHZ = 1000,
    BITS_PER_BYTE = 8
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

static void append_format(StringBuilder *line)
{
    string_builder_append(line, "Playing from computer \xC2\xB7 ");
    string_builder_append_unsigned(line, audio.rate / HZ_PER_KHZ);
    if (audio.rate % HZ_PER_KHZ != 0) {
        string_builder_append_char(line, '.');
        string_builder_append_unsigned(line, audio.rate % HZ_PER_KHZ / 100);
    }
    string_builder_append(line, " kHz ");
    string_builder_append_unsigned(line, BITS_PER_BYTE * usb_audio_sample_bytes(&audio));
    string_builder_append(line, "-bit");
}

static void show_state(void)
{
    char text[NOTICE_CAPACITY];
    StringBuilder line = string_builder_make(text, sizeof text);
    if (usb_audio_sample_bytes(&audio) == 0) {
        string_builder_append(&line, "Connected. Choose EchoPod as the computer's sound output.");
    } else if (open_rate == 0) {
        string_builder_append(&line, "The audio output didn't start.");
    } else {
        append_format(&line);
    }
    app_show_notice(TITLE, text);
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
    show_state();
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
    int level = board_audio_volume_level() + step;
    if (level >= 0 && level <= HAL_AUDIO_VOLUME_MAX) {
        hal_audio_set_volume((uint8_t)level);
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
        if (key.pressed && (key.key == KEY_VOLUME_UP || key.key == KEY_VOLUME_DOWN)) {
            step_volume(key.key == KEY_VOLUME_UP ? 1 : -1);
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
}

void usb_dac_session_run(void)
{
    app_show_notice(TITLE, "Computer found. Starting...");
    app_release_audio();
    board_audio_shut_down();
    uint8_t level = board_audio_volume_level();
    start_session();
    if (usb_wait_for_host(&port, HOST_TIMEOUT_MS)) {
        show_state();
        serve();
    }
    receiving = false;
    stop_output();
    board_audio_stream_close();
    board_audio_set_attenuation(0);
    if (board_audio_volume_level() != level) {
        hal_audio_set_volume(level);
    }
    usb_power_down();
}
