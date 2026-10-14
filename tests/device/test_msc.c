#include <string.h>

#include "check.h"
#include "device/msc.h"

enum {
    DISK_SECTORS = 64,
    TRANSFER_CAPACITY = 8192
};

typedef struct RamDisk {
    uint8_t bytes[DISK_SECTORS * 512];
} RamDisk;

typedef struct Exchange {
    uint8_t data[TRANSFER_CAPACITY];
    uint32_t length;
    uint8_t csw[13];
    int packets;
    int largest_packet;
} Exchange;

static RamDisk writable_disk;
static RamDisk read_only_disk;

static bool ram_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    RamDisk *disk = context;
    memcpy(buffer, disk->bytes + lba * 512, count * 512);
    return true;
}

static bool ram_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    RamDisk *disk = context;
    memcpy(disk->bytes + lba * 512, buffer, count * 512);
    return true;
}

static Msc *started(void)
{
    static Msc msc;
    for (uint32_t index = 0; index < sizeof writable_disk.bytes; index++) {
        writable_disk.bytes[index] = (uint8_t)(index * 7 + 1);
        read_only_disk.bytes[index] = (uint8_t)(index * 3 + 2);
    }
    MscLun luns[2] = {
        {ram_read, ram_write, &writable_disk, DISK_SECTORS, true, true, "Internal storage"},
        {ram_read, ram_write, &read_only_disk, DISK_SECTORS, false, true, "SD card"},
    };
    msc_init(&msc, luns, 2);
    return &msc;
}

static void send_command(Msc *msc, uint32_t expected, bool in, uint8_t lun, const uint8_t *block,
                         uint8_t block_length)
{
    uint8_t cbw[31] = {'U', 'S', 'B', 'C', 0x78, 0x56, 0x34, 0x12};
    cbw[8] = (uint8_t)expected;
    cbw[9] = (uint8_t)(expected >> 8);
    cbw[10] = (uint8_t)(expected >> 16);
    cbw[11] = (uint8_t)(expected >> 24);
    cbw[12] = in ? 0x80 : 0x00;
    cbw[13] = lun;
    cbw[14] = block_length;
    memcpy(cbw + 15, block, block_length);
    msc_receive(msc, cbw, sizeof cbw);
}

static void send_data(Msc *msc, const uint8_t *bytes, uint32_t length)
{
    for (uint32_t offset = 0; offset < length; offset += 64) {
        uint32_t chunk = length - offset < 64 ? length - offset : 64;
        msc_receive(msc, bytes + offset, chunk);
    }
}

static Exchange *collect(Msc *msc)
{
    static Exchange exchange;
    memset(&exchange, 0, sizeof exchange);
    uint8_t packet[64];
    uint32_t length = 0;
    while (msc_next_in(msc, packet, &length)) {
        exchange.packets++;
        exchange.largest_packet =
            (int)length > exchange.largest_packet ? (int)length : exchange.largest_packet;
        if (length == 13 && memcmp(packet, "USBS", 4) == 0) {
            memcpy(exchange.csw, packet, 13);
            break;
        }
        memcpy(exchange.data + exchange.length, packet, length);
        exchange.length += length;
    }
    return &exchange;
}

static uint32_t le32_at(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static uint32_t be32_at(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

static void check_csw(const Exchange *exchange, uint8_t status, uint32_t residue)
{
    CHECK_EQUAL(0x12345678, le32_at(exchange->csw + 4));
    CHECK_EQUAL(residue, le32_at(exchange->csw + 8));
    CHECK_EQUAL(status, exchange->csw[12]);
}

static Exchange *sense(Msc *msc)
{
    static const uint8_t block[6] = {0x03, 0, 0, 0, 18, 0};
    send_command(msc, 18, true, 0, block, 6);
    return collect(msc);
}

static void read_10_block(uint8_t *block, uint8_t opcode, uint32_t lba, uint16_t count)
{
    memset(block, 0, 10);
    block[0] = opcode;
    block[2] = (uint8_t)(lba >> 24);
    block[3] = (uint8_t)(lba >> 16);
    block[4] = (uint8_t)(lba >> 8);
    block[5] = (uint8_t)lba;
    block[7] = (uint8_t)(count >> 8);
    block[8] = (uint8_t)count;
}

static void inquiry_names_the_vendor_and_the_lun(void)
{
    static const uint8_t block[6] = {0x12, 0, 0, 0, 36, 0};
    Msc *msc = started();
    send_command(msc, 36, true, 1, block, 6);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(36, exchange->length);
    CHECK_EQUAL(0x80, exchange->data[1]);
    CHECK(memcmp(exchange->data + 8, "ECHOFW  ", 8) == 0);
    CHECK(memcmp(exchange->data + 16, "SD card         ", 16) == 0);
    check_csw(exchange, 0, 0);
}

static void a_longer_request_is_padded_and_reports_the_residue(void)
{
    static const uint8_t block[6] = {0x12, 0, 0, 0, 255, 0};
    Msc *msc = started();
    send_command(msc, 255, true, 0, block, 6);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(255, exchange->length);
    CHECK_EQUAL(0, exchange->data[200]);
    CHECK_EQUAL(64, exchange->largest_packet);
    check_csw(exchange, 0, 255 - 36);
}

static void test_unit_ready_passes_for_a_present_lun(void)
{
    static const uint8_t block[6] = {0x00};
    Msc *msc = started();
    send_command(msc, 0, false, 0, block, 6);
    check_csw(collect(msc), 0, 0);
}

static void an_absent_lun_is_not_ready_with_medium_not_present(void)
{
    static const uint8_t block[6] = {0x00};
    Msc *msc = started();
    msc->luns[1].present = false;
    send_command(msc, 0, false, 1, block, 6);
    check_csw(collect(msc), 1, 0);
    Exchange *exchange = sense(msc);
    CHECK_EQUAL(0x70, exchange->data[0]);
    CHECK_EQUAL(0x02, exchange->data[2]);
    CHECK_EQUAL(0x3A, exchange->data[12]);
}

static void read_capacity_reports_the_last_block_and_its_size(void)
{
    static const uint8_t block[10] = {0x25};
    Msc *msc = started();
    send_command(msc, 8, true, 0, block, 10);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(8, exchange->length);
    CHECK_EQUAL(DISK_SECTORS - 1, be32_at(exchange->data));
    CHECK_EQUAL(512, be32_at(exchange->data + 4));
    check_csw(exchange, 0, 0);
}

static void read_10_streams_the_sectors(void)
{
    uint8_t block[10];
    Msc *msc = started();
    read_10_block(block, 0x28, 2, 3);
    send_command(msc, 3 * 512, true, 0, block, 10);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(3 * 512, exchange->length);
    CHECK(memcmp(exchange->data, writable_disk.bytes + 2 * 512, 3 * 512) == 0);
    CHECK_EQUAL(24, exchange->packets - 1);
    check_csw(exchange, 0, 0);
}

static void write_10_stores_the_sectors(void)
{
    uint8_t block[10];
    uint8_t bytes[1024];
    for (uint32_t index = 0; index < sizeof bytes; index++) {
        bytes[index] = (uint8_t)(index ^ 0x5A);
    }
    Msc *msc = started();
    read_10_block(block, 0x2A, 5, 2);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 0, 0);
    CHECK(memcmp(writable_disk.bytes + 5 * 512, bytes, sizeof bytes) == 0);
}

static void writing_a_read_only_lun_is_refused_without_changing_it(void)
{
    uint8_t block[10];
    uint8_t bytes[512] = {0};
    Msc *msc = started();
    uint8_t before = read_only_disk.bytes[5 * 512];
    read_10_block(block, 0x2A, 5, 1);
    send_command(msc, sizeof bytes, false, 1, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 1, 512);
    CHECK_EQUAL(before, read_only_disk.bytes[5 * 512]);
    Exchange *exchange = sense(msc);
    CHECK_EQUAL(0x07, exchange->data[2]);
    CHECK_EQUAL(0x27, exchange->data[12]);
}

static void mode_sense_marks_a_read_only_lun_write_protected(void)
{
    static const uint8_t block[6] = {0x1A, 0, 0x3F, 0, 192, 0};
    Msc *msc = started();
    send_command(msc, 192, true, 1, block, 6);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(0x80, exchange->data[2]);
    send_command(msc, 192, true, 0, block, 6);
    exchange = collect(msc);
    CHECK_EQUAL(0x00, exchange->data[2]);
}

static void a_read_past_the_end_fails_with_an_out_of_range_sense(void)
{
    uint8_t block[10];
    Msc *msc = started();
    read_10_block(block, 0x28, DISK_SECTORS - 1, 2);
    send_command(msc, 1024, true, 0, block, 10);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(1024, exchange->length);
    check_csw(exchange, 1, 1024);
    CHECK_EQUAL(0x21, sense(msc)->data[12]);
}

static void an_unknown_command_fails_with_an_illegal_request(void)
{
    static const uint8_t block[16] = {0x9E};
    Msc *msc = started();
    send_command(msc, 32, true, 0, block, 16);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(32, exchange->length);
    check_csw(exchange, 1, 32);
    Exchange *sensed = sense(msc);
    CHECK_EQUAL(0x05, sensed->data[2]);
    CHECK_EQUAL(0x20, sensed->data[12]);
}

static void a_length_mismatch_is_a_phase_error(void)
{
    uint8_t block[10];
    Msc *msc = started();
    read_10_block(block, 0x28, 0, 2);
    send_command(msc, 512, true, 0, block, 10);
    check_csw(collect(msc), 2, 0);
}

static void an_eject_marks_only_that_lun_ejected_and_absent(void)
{
    static const uint8_t eject[6] = {0x1B, 0, 0, 0, 0x02, 0};
    static const uint8_t ready[6] = {0x00};
    Msc *msc = started();
    CHECK(!msc_ejected(msc, 1));
    send_command(msc, 0, false, 1, eject, 6);
    check_csw(collect(msc), 0, 0);
    CHECK(msc_ejected(msc, 1));
    CHECK(!msc_ejected(msc, 0));
    send_command(msc, 0, false, 1, ready, 6);
    check_csw(collect(msc), 1, 0);
    send_command(msc, 0, false, 0, ready, 6);
    check_csw(collect(msc), 0, 0);
}

static void a_start_without_eject_changes_nothing(void)
{
    static const uint8_t start[6] = {0x1B, 0, 0, 0, 0x01, 0};
    Msc *msc = started();
    send_command(msc, 0, false, 0, start, 6);
    check_csw(collect(msc), 0, 0);
    CHECK(!msc_ejected(msc, 0));
}

static void an_invalid_command_block_is_ignored(void)
{
    uint8_t garbage[31] = {'X', 'S', 'B', 'C'};
    uint8_t packet[64];
    uint32_t length = 0;
    Msc *msc = started();
    msc_receive(msc, garbage, sizeof garbage);
    CHECK(!msc_next_in(msc, packet, &length));
}

static void a_reset_drops_a_transfer_in_progress(void)
{
    uint8_t block[10];
    uint8_t packet[64];
    uint32_t length = 0;
    Msc *msc = started();
    read_10_block(block, 0x28, 0, 4);
    send_command(msc, 4 * 512, true, 0, block, 10);
    CHECK(msc_next_in(msc, packet, &length));
    msc_reset(msc);
    CHECK(!msc_next_in(msc, packet, &length));
    static const uint8_t ready[6] = {0x00};
    send_command(msc, 0, false, 0, ready, 6);
    check_csw(collect(msc), 0, 0);
}

static void a_read_sent_as_host_to_device_is_a_phase_error_and_writes_nothing(void)
{
    uint8_t block[10];
    uint8_t bytes[512];
    memset(bytes, 0xEE, sizeof bytes);
    Msc *msc = started();
    uint8_t before = writable_disk.bytes[3 * 512];
    read_10_block(block, 0x28, 3, 1);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 2, 512);
    CHECK_EQUAL(before, writable_disk.bytes[3 * 512]);
}

static void a_write_sent_as_device_to_host_is_a_phase_error(void)
{
    uint8_t block[10];
    Msc *msc = started();
    read_10_block(block, 0x2A, 3, 1);
    send_command(msc, 512, true, 0, block, 10);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(512, exchange->length);
    check_csw(exchange, 2, 512);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(inquiry_names_the_vendor_and_the_lun),
        TEST_CASE(a_longer_request_is_padded_and_reports_the_residue),
        TEST_CASE(test_unit_ready_passes_for_a_present_lun),
        TEST_CASE(an_absent_lun_is_not_ready_with_medium_not_present),
        TEST_CASE(read_capacity_reports_the_last_block_and_its_size),
        TEST_CASE(read_10_streams_the_sectors),
        TEST_CASE(write_10_stores_the_sectors),
        TEST_CASE(writing_a_read_only_lun_is_refused_without_changing_it),
        TEST_CASE(mode_sense_marks_a_read_only_lun_write_protected),
        TEST_CASE(a_read_past_the_end_fails_with_an_out_of_range_sense),
        TEST_CASE(an_unknown_command_fails_with_an_illegal_request),
        TEST_CASE(a_length_mismatch_is_a_phase_error),
        TEST_CASE(a_read_sent_as_host_to_device_is_a_phase_error_and_writes_nothing),
        TEST_CASE(a_write_sent_as_device_to_host_is_a_phase_error),
        TEST_CASE(an_eject_marks_only_that_lun_ejected_and_absent),
        TEST_CASE(a_start_without_eject_changes_nothing),
        TEST_CASE(an_invalid_command_block_is_ignored),
        TEST_CASE(a_reset_drops_a_transfer_in_progress),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
