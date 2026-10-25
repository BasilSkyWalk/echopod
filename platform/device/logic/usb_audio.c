#include "device/usb_audio.h"

#include <stddef.h>

enum {
    TYPE_STANDARD_DEVICE_IN = 0x80,
    TYPE_STANDARD_INTERFACE_IN = 0x81,
    TYPE_STANDARD_INTERFACE_OUT = 0x01,
    TYPE_CLASS_INTERFACE_OUT = 0x21,
    TYPE_CLASS_ENDPOINT_OUT = 0x22,
    TYPE_CLASS_INTERFACE_IN = 0xA1,
    TYPE_CLASS_ENDPOINT_IN = 0xA2,
    RECIPIENT_MASK = 0x1F,
    RECIPIENT_INTERFACE = 1,
    RECIPIENT_ENDPOINT = 2,
    REQUEST_GET_DESCRIPTOR = 6,
    REQUEST_GET_INTERFACE = 10,
    REQUEST_SET_INTERFACE = 11,
    REQUEST_SET_CUR = 0x01,
    REQUEST_GET_CUR = 0x81,
    REQUEST_GET_MIN = 0x82,
    REQUEST_GET_MAX = 0x83,
    REQUEST_GET_RES = 0x84,
    DESCRIPTOR_DEVICE = 1,
    DESCRIPTOR_CONFIGURATION = 2,
    STREAMING_INTERFACE = 1,
    LAST_ALT = 2,
    FEATURE_UNIT_TARGET = 0x0200,
    SELECTOR_MUTE = 1,
    SELECTOR_VOLUME = 2,
    SELECTOR_SAMPLING_FREQUENCY = 1,
    VOLUME_MIN = -16384,
    VOLUME_MAX = 0,
    VOLUME_RESOLUTION = 128,
    RATE_BYTES = 3,
    FRAMES_PER_SECOND = 1000,
    CHANNELS = 2
};

typedef enum AudioControl {
    CONTROL_NONE,
    CONTROL_MUTE,
    CONTROL_VOLUME,
    CONTROL_RATE
} AudioControl;

static const uint32_t RATES[] = {44100, 48000, 88200, 96000};

static const uint8_t DEVICE_DESCRIPTOR[] = {
    18,   1,    0x10, 0x01, 0x00, 0x00, 0x00, USB_PACKET_SIZE, 0x09, 0x12, 0x02,
    0x00, 0x00, 0x01, 1,    2,    0,    1};

static const uint8_t CONFIGURATION_DESCRIPTOR[] = {
    9,    2,    171,  0,    2,    1,    0,    0x80, 50,   9,    4,    0,    0,    0,    1,    1,
    0,    0,    9,    0x24, 1,    0x00, 0x01, 40,   0,    1,    1,    12,   0x24, 2,    1,    0x01,
    0x01, 0,    2,    0x03, 0x00, 0,    0,    10,   0x24, 6,    2,    1,    1,    0x03, 0x00, 0x00,
    0,    9,    0x24, 3,    3,    0x02, 0x03, 0,    2,    0,    9,    4,    1,    0,    0,    1,
    2,    0,    0,    9,    4,    1,    1,    1,    1,    2,    0,    0,    7,    0x24, 1,    1,
    1,    0x01, 0x00, 20,   0x24, 2,    1,    2,    2,    16,   4,    0x44, 0xAC, 0x00, 0x80, 0xBB,
    0x00, 0x88, 0x58, 0x01, 0x00, 0x77, 0x01, 9,    5,    0x01, 0x09, 0x84, 0x01, 1,    0,    0,
    7,    0x25, 1,    0x01, 0,    0,    0,    9,    4,    1,    2,    1,    1,    2,    0,    0,
    7,    0x24, 1,    1,    1,    0x01, 0x00, 20,   0x24, 2,    1,    2,    3,    24,   4,    0x44,
    0xAC, 0x00, 0x80, 0xBB, 0x00, 0x88, 0x58, 0x01, 0x00, 0x77, 0x01, 9,    5,    0x01, 0x09, 0x46,
    0x02, 1,    0,    0,    7,    0x25, 1,    0x01, 0,    0,    0};

void usb_audio_init(UsbAudio *audio, UsbDevice *device)
{
    usb_device_init(device, "", 1);
    usb_device_allow_high_speed(device, false);
    *audio = (UsbAudio){.device = device, .rate = USB_AUDIO_DEFAULT_RATE};
}

static void set_alt(UsbAudio *audio, uint8_t alt)
{
    if (audio->alt != alt) {
        audio->alt = alt;
        audio->changes |= USB_AUDIO_CHANGED_FORMAT;
    }
}

void usb_audio_reset(UsbAudio *audio)
{
    set_alt(audio, 0);
}

static UsbReply stall(void)
{
    return (UsbReply){USB_REPLY_STALL, NULL, 0, USB_ACTION_NONE, 0};
}

static UsbReply status(UsbAction action, uint16_t value)
{
    return (UsbReply){USB_REPLY_STATUS, NULL, 0, action, value};
}

static UsbReply data_reply(const uint8_t *bytes, uint16_t length, UsbSetup setup)
{
    uint16_t sent = length < setup.length ? length : setup.length;
    return (UsbReply){USB_REPLY_DATA, bytes, sent, USB_ACTION_NONE, 0};
}

static UsbReply little_endian(UsbAudio *audio, uint32_t value, uint16_t length, UsbSetup setup)
{
    for (uint16_t index = 0; index < length; index++) {
        audio->reply[index] = (uint8_t)(value >> (8 * index));
    }
    return data_reply(audio->reply, length, setup);
}

static bool rate_supported(uint32_t rate)
{
    for (size_t index = 0; index < sizeof RATES / sizeof RATES[0]; index++) {
        if (RATES[index] == rate) {
            return true;
        }
    }
    return false;
}

static AudioControl control_of(UsbSetup setup)
{
    uint8_t selector = (uint8_t)(setup.value >> 8);
    uint8_t recipient = setup.request_type & RECIPIENT_MASK;
    if (recipient == RECIPIENT_ENDPOINT) {
        bool rate =
            (setup.index & 0xFF) == USB_AUDIO_ENDPOINT && selector == SELECTOR_SAMPLING_FREQUENCY;
        return rate ? CONTROL_RATE : CONTROL_NONE;
    }
    if (recipient != RECIPIENT_INTERFACE || setup.index != FEATURE_UNIT_TARGET ||
        (setup.value & 0xFF) != 0) {
        return CONTROL_NONE;
    }
    return selector == SELECTOR_MUTE     ? CONTROL_MUTE
           : selector == SELECTOR_VOLUME ? CONTROL_VOLUME
                                         : CONTROL_NONE;
}

static UsbReply volume_reply(UsbAudio *audio, UsbSetup setup)
{
    switch (setup.request) {
    case REQUEST_GET_CUR:
        return little_endian(audio, (uint16_t)audio->volume, 2, setup);
    case REQUEST_GET_MIN:
        return little_endian(audio, (uint16_t)VOLUME_MIN, 2, setup);
    case REQUEST_GET_MAX:
        return little_endian(audio, VOLUME_MAX, 2, setup);
    case REQUEST_GET_RES:
        return little_endian(audio, VOLUME_RESOLUTION, 2, setup);
    default:
        return stall();
    }
}

static UsbReply class_in(UsbAudio *audio, UsbSetup setup)
{
    AudioControl control = control_of(setup);
    if (control == CONTROL_VOLUME) {
        return volume_reply(audio, setup);
    }
    if (setup.request != REQUEST_GET_CUR) {
        return stall();
    }
    if (control == CONTROL_MUTE) {
        return little_endian(audio, audio->muted, 1, setup);
    }
    return control == CONTROL_RATE ? little_endian(audio, audio->rate, RATE_BYTES, setup) : stall();
}

static UsbReply class_out(UsbSetup setup)
{
    bool known =
        setup.request == REQUEST_SET_CUR && setup.length > 0 && control_of(setup) != CONTROL_NONE;
    return known ? (UsbReply){USB_REPLY_RECEIVE, NULL, setup.length, USB_ACTION_NONE, 0} : stall();
}

static UsbReply set_interface(UsbAudio *audio, UsbSetup setup)
{
    if (setup.index == STREAMING_INTERFACE && setup.value <= LAST_ALT) {
        set_alt(audio, (uint8_t)setup.value);
        return status(USB_ACTION_STREAM, setup.value);
    }
    return setup.index == 0 && setup.value == 0 ? status(USB_ACTION_NONE, 0) : stall();
}

static bool own_descriptor(UsbSetup setup)
{
    uint8_t type = (uint8_t)(setup.value >> 8);
    return setup.request_type == TYPE_STANDARD_DEVICE_IN &&
           setup.request == REQUEST_GET_DESCRIPTOR &&
           (type == DESCRIPTOR_DEVICE || type == DESCRIPTOR_CONFIGURATION);
}

static UsbReply descriptor(UsbSetup setup)
{
    if ((setup.value >> 8) == DESCRIPTOR_DEVICE) {
        return data_reply(DEVICE_DESCRIPTOR, sizeof DEVICE_DESCRIPTOR, setup);
    }
    return data_reply(CONFIGURATION_DESCRIPTOR, sizeof CONFIGURATION_DESCRIPTOR, setup);
}

static UsbReply standard(UsbAudio *audio, UsbSetup setup)
{
    UsbReply reply = usb_device_setup(audio->device, setup);
    if (reply.action == USB_ACTION_CONFIGURE) {
        set_alt(audio, 0);
    }
    return reply;
}

UsbReply usb_audio_setup(UsbAudio *audio, UsbSetup setup)
{
    if (own_descriptor(setup)) {
        return descriptor(setup);
    }
    switch (setup.request_type) {
    case TYPE_CLASS_INTERFACE_IN:
    case TYPE_CLASS_ENDPOINT_IN:
        return class_in(audio, setup);
    case TYPE_CLASS_INTERFACE_OUT:
    case TYPE_CLASS_ENDPOINT_OUT:
        return class_out(setup);
    case TYPE_STANDARD_INTERFACE_IN:
        return setup.request == REQUEST_GET_INTERFACE
                   ? little_endian(audio, setup.index == STREAMING_INTERFACE ? audio->alt : 0, 1,
                                   setup)
                   : standard(audio, setup);
    case TYPE_STANDARD_INTERFACE_OUT:
        return setup.request == REQUEST_SET_INTERFACE ? set_interface(audio, setup)
                                                      : standard(audio, setup);
    default:
        return standard(audio, setup);
    }
}

static int16_t clamped_volume(int32_t volume)
{
    return (int16_t)(volume < VOLUME_MIN ? VOLUME_MIN : volume > VOLUME_MAX ? VOLUME_MAX : volume);
}

static bool take_rate(UsbAudio *audio, uint32_t rate)
{
    if (!rate_supported(rate)) {
        return false;
    }
    if (audio->rate != rate) {
        audio->rate = rate;
        audio->changes |= USB_AUDIO_CHANGED_FORMAT;
    }
    return true;
}

static uint32_t read_little_endian(const uint8_t *data, uint16_t length)
{
    uint32_t value = 0;
    for (uint16_t index = length; index > 0; index--) {
        value = value << 8 | data[index - 1];
    }
    return value;
}

UsbReply usb_audio_receive(UsbAudio *audio, UsbSetup setup, const uint8_t *data, uint16_t length)
{
    AudioControl control = control_of(setup);
    uint16_t needed = (uint16_t)(control == CONTROL_RATE     ? RATE_BYTES
                                 : control == CONTROL_VOLUME ? 2
                                                             : 1);
    if (control == CONTROL_NONE || length < needed) {
        return stall();
    }
    uint32_t value = read_little_endian(data, needed);
    if (control == CONTROL_RATE) {
        return take_rate(audio, value) ? status(USB_ACTION_NONE, 0) : stall();
    }
    if (control == CONTROL_MUTE) {
        audio->muted = value != 0;
    } else {
        audio->volume = clamped_volume((int16_t)value);
    }
    audio->changes |= USB_AUDIO_CHANGED_VOLUME;
    return status(USB_ACTION_NONE, 0);
}

uint8_t usb_audio_take_changes(UsbAudio *audio)
{
    uint8_t changes = audio->changes;
    audio->changes = 0;
    return changes;
}

uint8_t usb_audio_sample_bytes(const UsbAudio *audio)
{
    return audio->alt == 0 ? 0 : (uint8_t)(audio->alt + 1);
}

uint16_t usb_audio_packet_bytes(const UsbAudio *audio)
{
    uint32_t sample_bytes = usb_audio_sample_bytes(audio);
    if (sample_bytes == 0) {
        return USB_AUDIO_MAX_PACKET;
    }
    uint32_t frames = (audio->rate + FRAMES_PER_SECOND - 1) / FRAMES_PER_SECOND + 1;
    uint32_t bytes = frames * CHANNELS * sample_bytes;
    return (uint16_t)(bytes < USB_AUDIO_MAX_PACKET ? bytes : USB_AUDIO_MAX_PACKET);
}

uint8_t usb_audio_attenuation(const UsbAudio *audio)
{
    return audio->muted ? USB_AUDIO_MUTED : (uint8_t)(-audio->volume / VOLUME_RESOLUTION);
}
