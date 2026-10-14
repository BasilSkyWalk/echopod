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
    CHECK_EQUAL('E', product.data[2]);
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
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
