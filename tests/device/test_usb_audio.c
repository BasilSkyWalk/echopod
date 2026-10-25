#include "check.h"
#include "device/usb_audio.h"

enum {
    CONFIGURATION_LENGTH = 171
};

static UsbDevice device;

static UsbSetup setup(uint8_t type, uint8_t request, uint16_t value, uint16_t index,
                      uint16_t length)
{
    return (UsbSetup){type, request, value, index, length};
}

static UsbAudio ready_audio(void)
{
    UsbAudio audio;
    usb_audio_init(&audio, &device);
    return audio;
}

static UsbReply configuration(UsbAudio *audio)
{
    return usb_audio_setup(audio, setup(0x80, 6, 0x0200, 0, 255));
}

static const uint8_t *find_descriptor(UsbReply reply, uint8_t type, int occurrence)
{
    for (uint16_t offset = 0; offset < reply.length; offset += reply.data[offset]) {
        if (reply.data[offset + 1] == type && occurrence-- == 0) {
            return reply.data + offset;
        }
    }
    return NULL;
}

static uint32_t rate_at(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16;
}

static void set_cur(UsbAudio *audio, UsbSetup request, const uint8_t *bytes, uint16_t length)
{
    CHECK_EQUAL(USB_REPLY_RECEIVE, usb_audio_setup(audio, request).kind);
    CHECK_EQUAL(USB_REPLY_STATUS, usb_audio_receive(audio, request, bytes, length).kind);
}

static void the_device_is_a_full_speed_composite_of_audio_interfaces(void)
{
    UsbAudio audio = ready_audio();
    UsbReply reply = usb_audio_setup(&audio, setup(0x80, 6, 0x0100, 0, 64));
    CHECK_EQUAL(18, reply.length);
    CHECK_EQUAL(0x10, reply.data[2]);
    CHECK_EQUAL(0x01, reply.data[3]);
    CHECK_EQUAL(0, reply.data[4]);
    CHECK_EQUAL(0x02, reply.data[10]);
    CHECK_EQUAL(0, reply.data[16]);
}

static void the_configuration_walks_to_its_declared_length(void)
{
    UsbAudio audio = ready_audio();
    UsbReply reply = configuration(&audio);
    CHECK_EQUAL(CONFIGURATION_LENGTH, reply.length);
    CHECK_EQUAL(CONFIGURATION_LENGTH, reply.data[2]);
    uint16_t offset = 0;
    while (offset < reply.length && reply.data[offset] > 0) {
        offset += reply.data[offset];
    }
    CHECK_EQUAL(CONFIGURATION_LENGTH, offset);
}

static void the_control_header_covers_its_units(void)
{
    UsbAudio audio = ready_audio();
    UsbReply reply = configuration(&audio);
    const uint8_t *header = find_descriptor(reply, 0x24, 0);
    CHECK_EQUAL(1, header[2]);
    uint16_t total = 0;
    for (const uint8_t *unit = header; unit[1] == 0x24; unit += unit[0]) {
        total += unit[0];
    }
    CHECK_EQUAL(header[5], total);
}

static const uint8_t *format_of_setting(UsbReply reply, int setting)
{
    for (uint16_t offset = 0; offset < reply.length; offset += reply.data[offset]) {
        const uint8_t *bytes = reply.data + offset;
        if (bytes[0] == 20 && bytes[1] == 0x24 && bytes[2] == 2 && setting-- == 0) {
            return bytes;
        }
    }
    return NULL;
}

static void both_streaming_settings_offer_four_rates(void)
{
    UsbAudio audio = ready_audio();
    UsbReply reply = configuration(&audio);
    static const uint32_t rates[] = {44100, 48000, 88200, 96000};
    for (int setting = 0; setting < 2; setting++) {
        const uint8_t *format = format_of_setting(reply, setting);
        CHECK(format != NULL);
        CHECK_EQUAL(setting == 0 ? 2 : 3, format[5]);
        CHECK_EQUAL(setting == 0 ? 16 : 24, format[6]);
        CHECK_EQUAL(4, format[7]);
        for (int index = 0; index < 4; index++) {
            CHECK_EQUAL(rates[index], rate_at(format + 8 + 3 * index));
        }
    }
}

static void each_streaming_endpoint_fits_ninety_seven_frames(void)
{
    UsbAudio audio = ready_audio();
    UsbReply reply = configuration(&audio);
    const uint8_t *sixteen = find_descriptor(reply, 5, 0);
    const uint8_t *twenty_four = find_descriptor(reply, 5, 1);
    CHECK_EQUAL(USB_AUDIO_ENDPOINT, sixteen[2]);
    CHECK_EQUAL(0x09, sixteen[3]);
    CHECK_EQUAL(97 * 4, sixteen[4] | sixteen[5] << 8);
    CHECK_EQUAL(USB_AUDIO_MAX_PACKET, twenty_four[4] | twenty_four[5] << 8);
    CHECK_EQUAL(97 * 6, USB_AUDIO_MAX_PACKET);
}

static void choosing_a_streaming_setting_reports_the_sample_size(void)
{
    UsbAudio audio = ready_audio();
    UsbReply reply = usb_audio_setup(&audio, setup(0x01, 11, 2, 1, 0));
    CHECK_EQUAL(USB_ACTION_STREAM, reply.action);
    CHECK_EQUAL(2, reply.value);
    CHECK_EQUAL(3, usb_audio_sample_bytes(&audio));
    CHECK_EQUAL(USB_AUDIO_CHANGED_FORMAT, usb_audio_take_changes(&audio));
    CHECK_EQUAL(0, usb_audio_take_changes(&audio));
    usb_audio_setup(&audio, setup(0x01, 11, 1, 1, 0));
    CHECK_EQUAL(2, usb_audio_sample_bytes(&audio));
    reply = usb_audio_setup(&audio, setup(0x81, 10, 0, 1, 1));
    CHECK_EQUAL(1, reply.data[0]);
}

static void unknown_settings_stall(void)
{
    UsbAudio audio = ready_audio();
    CHECK_EQUAL(USB_REPLY_STALL, usb_audio_setup(&audio, setup(0x01, 11, 3, 1, 0)).kind);
    CHECK_EQUAL(USB_REPLY_STALL, usb_audio_setup(&audio, setup(0x01, 11, 1, 0, 0)).kind);
    CHECK_EQUAL(USB_REPLY_STATUS, usb_audio_setup(&audio, setup(0x01, 11, 0, 0, 0)).kind);
}

static void a_bus_reset_or_new_configuration_stops_streaming(void)
{
    UsbAudio audio = ready_audio();
    usb_audio_setup(&audio, setup(0x01, 11, 1, 1, 0));
    usb_audio_reset(&audio);
    CHECK_EQUAL(0, usb_audio_sample_bytes(&audio));
    usb_audio_setup(&audio, setup(0x01, 11, 1, 1, 0));
    UsbReply reply = usb_audio_setup(&audio, setup(0x00, 9, 1, 0, 0));
    CHECK_EQUAL(USB_ACTION_CONFIGURE, reply.action);
    CHECK_EQUAL(0, usb_audio_sample_bytes(&audio));
}

static void the_host_sets_and_reads_the_sample_rate(void)
{
    UsbAudio audio = ready_audio();
    CHECK_EQUAL(48000, audio.rate);
    static const uint8_t rate[] = {0x44, 0xAC, 0x00};
    UsbSetup request = setup(0x22, 0x01, 0x0100, 0x01, 3);
    set_cur(&audio, request, rate, 3);
    CHECK_EQUAL(44100, audio.rate);
    CHECK_EQUAL(USB_AUDIO_CHANGED_FORMAT, usb_audio_take_changes(&audio));
    UsbReply reply = usb_audio_setup(&audio, setup(0xA2, 0x81, 0x0100, 0x01, 3));
    CHECK_EQUAL(3, reply.length);
    CHECK_EQUAL(44100, rate_at(reply.data));
}

static void an_unsupported_rate_stalls_and_keeps_the_old_one(void)
{
    UsbAudio audio = ready_audio();
    static const uint8_t rate[] = {0x00, 0xEE, 0x02};
    UsbReply reply = usb_audio_receive(&audio, setup(0x22, 0x01, 0x0100, 0x01, 3), rate, 3);
    CHECK_EQUAL(USB_REPLY_STALL, reply.kind);
    CHECK_EQUAL(48000, audio.rate);
}

static void the_volume_range_is_minus_sixty_four_to_zero_decibels(void)
{
    UsbAudio audio = ready_audio();
    UsbReply minimum = usb_audio_setup(&audio, setup(0xA1, 0x82, 0x0200, 0x0200, 2));
    CHECK_EQUAL(0x00, minimum.data[0]);
    CHECK_EQUAL(0xC0, minimum.data[1]);
    UsbReply resolution = usb_audio_setup(&audio, setup(0xA1, 0x84, 0x0200, 0x0200, 2));
    CHECK_EQUAL(0x80, resolution.data[0]);
    UsbReply maximum = usb_audio_setup(&audio, setup(0xA1, 0x83, 0x0200, 0x0200, 2));
    CHECK_EQUAL(0, maximum.data[0] | maximum.data[1]);
}

static void host_volume_becomes_half_decibel_attenuation(void)
{
    UsbAudio audio = ready_audio();
    CHECK_EQUAL(0, usb_audio_attenuation(&audio));
    static const uint8_t minus_ten_db[] = {0x00, 0xF6};
    set_cur(&audio, setup(0x21, 0x01, 0x0200, 0x0200, 2), minus_ten_db, 2);
    CHECK_EQUAL(20, usb_audio_attenuation(&audio));
    CHECK_EQUAL(USB_AUDIO_CHANGED_VOLUME, usb_audio_take_changes(&audio));
    UsbReply reply = usb_audio_setup(&audio, setup(0xA1, 0x81, 0x0200, 0x0200, 2));
    CHECK_EQUAL(0xF6, reply.data[1]);
}

static void volume_below_the_range_is_clamped(void)
{
    UsbAudio audio = ready_audio();
    static const uint8_t silent[] = {0x00, 0x80};
    set_cur(&audio, setup(0x21, 0x01, 0x0200, 0x0200, 2), silent, 2);
    CHECK_EQUAL(128, usb_audio_attenuation(&audio));
}

static void mute_overrides_the_volume(void)
{
    UsbAudio audio = ready_audio();
    static const uint8_t on[] = {1};
    set_cur(&audio, setup(0x21, 0x01, 0x0100, 0x0200, 1), on, 1);
    CHECK_EQUAL(USB_AUDIO_MUTED, usb_audio_attenuation(&audio));
    UsbReply reply = usb_audio_setup(&audio, setup(0xA1, 0x81, 0x0100, 0x0200, 1));
    CHECK_EQUAL(1, reply.data[0]);
}

static void requests_for_other_units_or_channels_stall(void)
{
    UsbAudio audio = ready_audio();
    CHECK_EQUAL(USB_REPLY_STALL,
                usb_audio_setup(&audio, setup(0xA1, 0x81, 0x0201, 0x0200, 2)).kind);
    CHECK_EQUAL(USB_REPLY_STALL,
                usb_audio_setup(&audio, setup(0x21, 0x01, 0x0200, 0x0500, 2)).kind);
    CHECK_EQUAL(USB_REPLY_STALL, usb_audio_setup(&audio, setup(0xA1, 0xFE, 0, 0, 1)).kind);
}

static void standard_requests_reach_the_usb_device(void)
{
    UsbAudio audio = ready_audio();
    UsbReply address = usb_audio_setup(&audio, setup(0x00, 5, 7, 0, 0));
    CHECK_EQUAL(USB_ACTION_SET_ADDRESS, address.action);
    CHECK_EQUAL(7, address.value);
    CHECK_EQUAL(USB_REPLY_STALL, usb_audio_setup(&audio, setup(0x80, 6, 0x0600, 0, 10)).kind);
    CHECK_EQUAL(USB_REPLY_DATA, usb_audio_setup(&audio, setup(0x80, 6, 0x0302, 0, 64)).kind);
}

static void the_fifo_split_gives_reception_all_but_one_control_packet(void)
{
    UsbFifoSizes sizes = usb_audio_fifo_sizes((UsbFifoSizes){.rx = 147, .tx0 = 128u << 16 | 147});
    CHECK_EQUAL(259, sizes.rx);
    CHECK_EQUAL(16u << 16 | 259, sizes.tx0);
}

static void an_unexpected_fifo_layout_is_left_alone(void)
{
    UsbFifoSizes odd = {.rx = 0x120, .tx0 = 0x20u << 16 | 0x140};
    UsbFifoSizes sizes = usb_audio_fifo_sizes(odd);
    CHECK_EQUAL(odd.rx, sizes.rx);
    CHECK_EQUAL(odd.tx0, sizes.tx0);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_device_is_a_full_speed_composite_of_audio_interfaces),
        TEST_CASE(the_configuration_walks_to_its_declared_length),
        TEST_CASE(the_control_header_covers_its_units),
        TEST_CASE(both_streaming_settings_offer_four_rates),
        TEST_CASE(each_streaming_endpoint_fits_ninety_seven_frames),
        TEST_CASE(choosing_a_streaming_setting_reports_the_sample_size),
        TEST_CASE(unknown_settings_stall),
        TEST_CASE(a_bus_reset_or_new_configuration_stops_streaming),
        TEST_CASE(the_host_sets_and_reads_the_sample_rate),
        TEST_CASE(an_unsupported_rate_stalls_and_keeps_the_old_one),
        TEST_CASE(the_volume_range_is_minus_sixty_four_to_zero_decibels),
        TEST_CASE(host_volume_becomes_half_decibel_attenuation),
        TEST_CASE(volume_below_the_range_is_clamped),
        TEST_CASE(mute_overrides_the_volume),
        TEST_CASE(requests_for_other_units_or_channels_stall),
        TEST_CASE(standard_requests_reach_the_usb_device),
        TEST_CASE(the_fifo_split_gives_reception_all_but_one_control_packet),
        TEST_CASE(an_unexpected_fifo_layout_is_left_alone),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
