#include <string.h>

#include "check.h"
#include "device/usb_device.h"

static UsbSetup setup(uint8_t type, uint8_t request, uint16_t value, uint16_t index,
                      uint16_t length)
{
    return (UsbSetup){type, request, value, index, length};
}

static UsbDevice ready_device(void)
{
    UsbDevice device;
    usb_device_init(&device, "1A2B3C4D", 2);
    return device;
}

static void a_setup_packet_is_parsed_little_endian(void)
{
    static const uint8_t packet[8] = {0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00};
    UsbSetup parsed = usb_setup_parse(packet);
    CHECK_EQUAL(0x80, parsed.request_type);
    CHECK_EQUAL(6, parsed.request);
    CHECK_EQUAL(0x0100, parsed.value);
    CHECK_EQUAL(0, parsed.index);
    CHECK_EQUAL(64, parsed.length);
}

static void the_device_descriptor_names_a_full_speed_mass_storage_device(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0100, 0, 64));
    CHECK_EQUAL(USB_REPLY_DATA, reply.kind);
    CHECK_EQUAL(18, reply.length);
    CHECK_EQUAL(18, reply.data[0]);
    CHECK_EQUAL(1, reply.data[1]);
    CHECK_EQUAL(0x00, reply.data[2]);
    CHECK_EQUAL(0x02, reply.data[3]);
    CHECK_EQUAL(64, reply.data[7]);
    CHECK_EQUAL(0x09, reply.data[8]);
    CHECK_EQUAL(0x12, reply.data[9]);
    CHECK_EQUAL(0x01, reply.data[10]);
    CHECK_EQUAL(0x00, reply.data[11]);
    CHECK_EQUAL(1, reply.data[17]);
}

static void a_short_request_gets_a_short_reply(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0100, 0, 8));
    CHECK_EQUAL(8, reply.length);
}

static void the_configuration_has_one_bulk_only_scsi_interface(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0200, 0, 255));
    CHECK_EQUAL(32, reply.length);
    CHECK_EQUAL(32, reply.data[2]);
    CHECK_EQUAL(1, reply.data[4]);
    CHECK_EQUAL(0x08, reply.data[9 + 5]);
    CHECK_EQUAL(0x06, reply.data[9 + 6]);
    CHECK_EQUAL(0x50, reply.data[9 + 7]);
    CHECK_EQUAL(USB_ENDPOINT_BULK_IN, reply.data[18 + 2]);
    CHECK_EQUAL(0x02, reply.data[18 + 3]);
    CHECK_EQUAL(64, reply.data[18 + 4]);
    CHECK_EQUAL(USB_ENDPOINT_BULK_OUT, reply.data[25 + 2]);
}

static bool utf16_is(const char *text, UsbReply reply)
{
    uint32_t length = (uint32_t)strlen(text);
    if (reply.length != 2 + 2 * length || reply.data[0] != reply.length || reply.data[1] != 3) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        if (reply.data[2 + 2 * index] != (uint8_t)text[index] || reply.data[3 + 2 * index] != 0) {
            return false;
        }
    }
    return true;
}

static void string_descriptors_are_utf16(void)
{
    UsbDevice device = ready_device();
    UsbReply languages = usb_device_setup(&device, setup(0x80, 6, 0x0300, 0, 255));
    CHECK_EQUAL(4, languages.length);
    CHECK_EQUAL(0x09, languages.data[2]);
    CHECK_EQUAL(0x04, languages.data[3]);
    UsbReply serial = usb_device_setup(&device, setup(0x80, 6, 0x0303, 0x0409, 255));
    CHECK_EQUAL(18, serial.length);
    CHECK_EQUAL(3, serial.data[1]);
    CHECK_EQUAL('1', serial.data[2]);
    CHECK_EQUAL(0, serial.data[3]);
    CHECK_EQUAL('D', serial.data[16]);
    UsbReply product = usb_device_setup(&device, setup(0x80, 6, 0x0302, 0x0409, 255));
    CHECK_EQUAL(USB_REPLY_DATA, product.kind);
    CHECK(utf16_is("EchoPod", product));
    UsbReply maker = usb_device_setup(&device, setup(0x80, 6, 0x0301, 0x0409, 255));
    CHECK(utf16_is("EchoPod", maker));
}

static void unknown_descriptors_stall(void)
{
    UsbDevice device = ready_device();
    CHECK_EQUAL(USB_REPLY_STALL, usb_device_setup(&device, setup(0x80, 6, 0x0600, 0, 10)).kind);
    CHECK_EQUAL(USB_REPLY_STALL, usb_device_setup(&device, setup(0x80, 6, 0x0304, 0, 10)).kind);
}

static void set_address_acknowledges_and_asks_for_the_address(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x00, 5, 23, 0, 0));
    CHECK_EQUAL(USB_REPLY_STATUS, reply.kind);
    CHECK_EQUAL(USB_ACTION_SET_ADDRESS, reply.action);
    CHECK_EQUAL(23, reply.value);
}

static void set_configuration_activates_the_endpoints_and_is_reported_back(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x00, 9, 1, 0, 0));
    CHECK_EQUAL(USB_REPLY_STATUS, reply.kind);
    CHECK_EQUAL(USB_ACTION_CONFIGURE, reply.action);
    UsbReply configuration = usb_device_setup(&device, setup(0x80, 8, 0, 0, 1));
    CHECK_EQUAL(1, configuration.length);
    CHECK_EQUAL(1, configuration.data[0]);
    CHECK_EQUAL(USB_REPLY_STALL, usb_device_setup(&device, setup(0x00, 9, 2, 0, 0)).kind);
}

static void get_status_answers_two_zero_bytes(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x80, 0, 0, 0, 2));
    CHECK_EQUAL(2, reply.length);
    CHECK_EQUAL(0, reply.data[0]);
    CHECK_EQUAL(0, reply.data[1]);
}

static void clearing_an_endpoint_halt_names_the_endpoint(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x02, 1, 0, USB_ENDPOINT_BULK_IN, 0));
    CHECK_EQUAL(USB_REPLY_STATUS, reply.kind);
    CHECK_EQUAL(USB_ACTION_CLEAR_HALT, reply.action);
    CHECK_EQUAL(USB_ENDPOINT_BULK_IN, reply.value);
}

static void get_max_lun_reports_the_last_lun(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0xA1, 0xFE, 0, 0, 1));
    CHECK_EQUAL(1, reply.length);
    CHECK_EQUAL(1, reply.data[0]);
}

static void a_mass_storage_reset_is_acknowledged(void)
{
    UsbDevice device = ready_device();
    UsbReply reply = usb_device_setup(&device, setup(0x21, 0xFF, 0, 0, 0));
    CHECK_EQUAL(USB_REPLY_STATUS, reply.kind);
    CHECK_EQUAL(USB_ACTION_MSC_RESET, reply.action);
}

static void unknown_requests_stall(void)
{
    UsbDevice device = ready_device();
    CHECK_EQUAL(USB_REPLY_STALL, usb_device_setup(&device, setup(0x40, 0x33, 0, 0, 0)).kind);
}

static UsbDevice high_speed_device(UsbSpeed speed)
{
    UsbDevice device = ready_device();
    usb_device_allow_high_speed(&device, true);
    usb_device_set_speed(&device, speed);
    return device;
}

static uint16_t packet_size_at(const uint8_t *configuration, int offset)
{
    return (uint16_t)(configuration[offset + 4] | configuration[offset + 5] << 8);
}

static void without_high_speed_the_qualifier_and_other_speed_stall(void)
{
    UsbDevice device = ready_device();
    CHECK_EQUAL(USB_REPLY_STALL, usb_device_setup(&device, setup(0x80, 6, 0x0600, 0, 10)).kind);
    CHECK_EQUAL(USB_REPLY_STALL, usb_device_setup(&device, setup(0x80, 6, 0x0700, 0, 255)).kind);
    CHECK_EQUAL(USB_PACKET_SIZE, usb_device_bulk_packet_size(&device));
}

static void a_high_speed_device_has_a_qualifier(void)
{
    static const uint8_t expected[10] = {10, 6, 0x00, 0x02, 0, 0, 0, 64, 1, 0};
    UsbDevice device = high_speed_device(USB_SPEED_FULL);
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0600, 0, 10));
    CHECK_EQUAL(USB_REPLY_DATA, reply.kind);
    CHECK_EQUAL(10, reply.length);
    if (reply.kind != USB_REPLY_DATA) {
        return;
    }
    for (int index = 0; index < 10; index++) {
        CHECK_EQUAL(expected[index], reply.data[index]);
    }
}

static void at_high_speed_bulk_packets_are_512_bytes(void)
{
    UsbDevice device = high_speed_device(USB_SPEED_HIGH);
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0200, 0, 255));
    CHECK_EQUAL(32, reply.length);
    CHECK_EQUAL(2, reply.data[1]);
    CHECK_EQUAL(USB_ENDPOINT_BULK_IN, reply.data[18 + 2]);
    CHECK_EQUAL(512, packet_size_at(reply.data, 18));
    CHECK_EQUAL(512, packet_size_at(reply.data, 25));
    CHECK_EQUAL(0, reply.data[25 + 6]);
    CHECK_EQUAL(USB_HIGH_SPEED_PACKET_SIZE, usb_device_bulk_packet_size(&device));
}

static void at_high_speed_the_other_speed_is_full_speed(void)
{
    UsbDevice device = high_speed_device(USB_SPEED_HIGH);
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0700, 0, 255));
    CHECK_EQUAL(USB_REPLY_DATA, reply.kind);
    CHECK_EQUAL(32, reply.length);
    if (reply.kind != USB_REPLY_DATA) {
        return;
    }
    CHECK_EQUAL(7, reply.data[1]);
    CHECK_EQUAL(64, packet_size_at(reply.data, 18));
    CHECK_EQUAL(64, packet_size_at(reply.data, 25));
}

static void at_full_speed_the_other_speed_is_high_speed(void)
{
    UsbDevice device = high_speed_device(USB_SPEED_FULL);
    UsbReply current = usb_device_setup(&device, setup(0x80, 6, 0x0200, 0, 255));
    CHECK_EQUAL(2, current.data[1]);
    CHECK_EQUAL(64, packet_size_at(current.data, 18));
    UsbReply other = usb_device_setup(&device, setup(0x80, 6, 0x0700, 0, 255));
    CHECK_EQUAL(USB_REPLY_DATA, other.kind);
    if (other.kind != USB_REPLY_DATA) {
        return;
    }
    CHECK_EQUAL(7, other.data[1]);
    CHECK_EQUAL(512, packet_size_at(other.data, 18));
    CHECK_EQUAL(512, packet_size_at(other.data, 25));
    CHECK_EQUAL(USB_PACKET_SIZE, usb_device_bulk_packet_size(&device));
}

static void a_short_configuration_request_still_gets_the_right_type(void)
{
    UsbDevice device = high_speed_device(USB_SPEED_HIGH);
    UsbReply reply = usb_device_setup(&device, setup(0x80, 6, 0x0700, 0, 9));
    CHECK_EQUAL(9, reply.length);
    if (reply.kind != USB_REPLY_DATA) {
        return;
    }
    CHECK_EQUAL(7, reply.data[1]);
    CHECK_EQUAL(32, reply.data[2]);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_setup_packet_is_parsed_little_endian),
        TEST_CASE(the_device_descriptor_names_a_full_speed_mass_storage_device),
        TEST_CASE(a_short_request_gets_a_short_reply),
        TEST_CASE(the_configuration_has_one_bulk_only_scsi_interface),
        TEST_CASE(string_descriptors_are_utf16),
        TEST_CASE(unknown_descriptors_stall),
        TEST_CASE(set_address_acknowledges_and_asks_for_the_address),
        TEST_CASE(set_configuration_activates_the_endpoints_and_is_reported_back),
        TEST_CASE(get_status_answers_two_zero_bytes),
        TEST_CASE(clearing_an_endpoint_halt_names_the_endpoint),
        TEST_CASE(get_max_lun_reports_the_last_lun),
        TEST_CASE(a_mass_storage_reset_is_acknowledged),
        TEST_CASE(unknown_requests_stall),
        TEST_CASE(without_high_speed_the_qualifier_and_other_speed_stall),
        TEST_CASE(a_high_speed_device_has_a_qualifier),
        TEST_CASE(at_high_speed_bulk_packets_are_512_bytes),
        TEST_CASE(at_high_speed_the_other_speed_is_full_speed),
        TEST_CASE(at_full_speed_the_other_speed_is_high_speed),
        TEST_CASE(a_short_configuration_request_still_gets_the_right_type),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
