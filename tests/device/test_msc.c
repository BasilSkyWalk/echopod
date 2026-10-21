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

static uint32_t disk_reads;

static bool ram_read(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    RamDisk *disk = context;
    disk_reads += count;
    memcpy(buffer, disk->bytes + lba * 512, count * 512);
    return true;
}

static uint32_t failing_write_lba = UINT32_MAX;

typedef struct WriteCall {
    uint32_t lba;
    uint32_t count;
} WriteCall;

static WriteCall write_calls[32];
static int write_call_count;

static bool ram_write(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    RamDisk *disk = context;
    if (write_call_count < 32) {
        write_calls[write_call_count] = (WriteCall){lba, count};
    }
    write_call_count++;
    bool fails = failing_write_lba >= lba && failing_write_lba - lba < count;
    uint32_t written = fails ? failing_write_lba - lba : count;
    memcpy(disk->bytes + lba * 512, buffer, written * 512);
    return !fails;
}

static void check_write_call(int index, uint32_t lba, uint32_t count)
{
    CHECK_EQUAL(lba, write_calls[index].lba);
    CHECK_EQUAL(count, write_calls[index].count);
}

static uint32_t write_buffer[4 * 128];

static Msc *started(void)
{
    static Msc msc;
    failing_write_lba = UINT32_MAX;
    write_call_count = 0;
    disk_reads = 0;
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

static Msc *started_with_buffer(void)
{
    Msc *msc = started();
    msc_use_write_buffer(msc, write_buffer, 4);
    return msc;
}

static void fill_pattern(uint8_t *bytes, uint32_t length, uint8_t seed)
{
    for (uint32_t index = 0; index < length; index++) {
        bytes[index] = (uint8_t)(index * 13 + seed);
    }
}

static Exchange *collect(Msc *msc)
{
    static Exchange exchange;
    memset(&exchange, 0, sizeof exchange);
    uint8_t packet[64];
    uint32_t length = 0;
    while (msc_next_in(msc, packet, sizeof packet, &length)) {
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

static Exchange *sense(Msc *msc, uint8_t lun)
{
    static const uint8_t block[6] = {0x03, 0, 0, 0, 18, 0};
    send_command(msc, 18, true, lun, block, 6);
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
    Exchange *exchange = sense(msc, 1);
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
    Exchange *exchange = sense(msc, 1);
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
    CHECK_EQUAL(0x21, sense(msc, 0)->data[12]);
}

static void an_unknown_command_fails_with_an_illegal_request(void)
{
    static const uint8_t block[16] = {0x9E};
    Msc *msc = started();
    send_command(msc, 32, true, 0, block, 16);
    Exchange *exchange = collect(msc);
    CHECK_EQUAL(32, exchange->length);
    check_csw(exchange, 1, 32);
    Exchange *sensed = sense(msc, 0);
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
    CHECK(!msc_next_in(msc, packet, sizeof packet, &length));
}

static void a_reset_drops_a_transfer_in_progress(void)
{
    uint8_t block[10];
    uint8_t packet[64];
    uint32_t length = 0;
    Msc *msc = started();
    read_10_block(block, 0x28, 0, 4);
    send_command(msc, 4 * 512, true, 0, block, 10);
    CHECK(msc_next_in(msc, packet, sizeof packet, &length));
    msc_reset(msc);
    CHECK(!msc_next_in(msc, packet, sizeof packet, &length));
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

static void counters_record_commands_sectors_and_failures(void)
{
    uint8_t block[10];
    uint8_t bytes[1024] = {0};
    Msc *msc = started();
    read_10_block(block, 0x28, 0, 2);
    send_command(msc, 1024, true, 0, block, 10);
    collect(msc);
    failing_write_lba = 7;
    read_10_block(block, 0x2A, 6, 2);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 1, 1024);
    MscStats stats = msc_stats(msc);
    CHECK_EQUAL(2, stats.commands);
    CHECK_EQUAL(2, stats.sectors_read);
    CHECK_EQUAL(1, stats.sectors_written);
    CHECK_EQUAL(0, stats.read_failures);
    CHECK_EQUAL(1, stats.write_failures);
    CHECK_EQUAL(7, stats.last_failed_lba);
}

static void a_failed_write_skips_the_rest_of_the_command(void)
{
    uint8_t block[10];
    uint8_t bytes[1536];
    memset(bytes, 0x3C, sizeof bytes);
    Msc *msc = started();
    failing_write_lba = 11;
    read_10_block(block, 0x2A, 10, 3);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    collect(msc);
    CHECK_EQUAL(0x3C, writable_disk.bytes[10 * 512]);
    CHECK(writable_disk.bytes[12 * 512] != 0x3C);
    CHECK_EQUAL(1, msc_stats(msc).write_failures);
}

static void inquiry_for_a_vital_product_data_page_is_an_invalid_field(void)
{
    static const uint8_t block[6] = {0x12, 0x01, 0x80, 0, 255, 0};
    Msc *msc = started();
    send_command(msc, 255, true, 0, block, 6);
    Exchange *exchange = collect(msc);
    check_csw(exchange, 1, 255);
    Exchange *sensed = sense(msc, 0);
    CHECK_EQUAL(0x05, sensed->data[2]);
    CHECK_EQUAL(0x24, sensed->data[12]);
}

static void history_keeps_the_last_commands_other_than_reads_writes_and_ready_checks(void)
{
    static const uint8_t inquiry[6] = {0x12, 0, 0, 0, 36, 0};
    static const uint8_t ready[6] = {0x00};
    static const uint8_t mode[6] = {0x1A, 0, 0x3F, 0, 4, 0};
    uint8_t read[10];
    Msc *msc = started();
    read_10_block(read, 0x28, 0, 1);
    send_command(msc, 36, true, 0, inquiry, 6);
    collect(msc);
    send_command(msc, 0, false, 0, ready, 6);
    collect(msc);
    send_command(msc, 512, true, 0, read, 10);
    collect(msc);
    send_command(msc, 4, true, 0, mode, 6);
    collect(msc);
    MscHistory history = msc_history(msc);
    CHECK_EQUAL(2, history.count);
    CHECK_EQUAL(0x12, history.entries[0].opcode);
    CHECK_EQUAL(0, history.entries[0].status);
    CHECK_EQUAL(0x1A, history.entries[1].opcode);
}

static void history_holds_the_most_recent_entries(void)
{
    static const uint8_t mode[6] = {0x1A, 0, 0x3F, 0, 4, 0};
    static const uint8_t inquiry[6] = {0x12, 0, 0, 0, 36, 0};
    Msc *msc = started();
    for (int index = 0; index < MSC_HISTORY_CAPACITY + 3; index++) {
        send_command(msc, 4, true, 0, mode, 6);
        collect(msc);
    }
    send_command(msc, 36, true, 0, inquiry, 6);
    collect(msc);
    MscHistory history = msc_history(msc);
    CHECK_EQUAL(MSC_HISTORY_CAPACITY, history.count);
    CHECK_EQUAL(0x12, history.entries[MSC_HISTORY_CAPACITY - 1].opcode);
    CHECK_EQUAL(0x1A, history.entries[0].opcode);
}

static void each_lun_keeps_its_own_sense(void)
{
    static const uint8_t ready[6] = {0x00};
    Msc *msc = started();
    msc->luns[1].present = false;
    send_command(msc, 0, false, 1, ready, 6);
    check_csw(collect(msc), 1, 0);
    CHECK_EQUAL(0x00, sense(msc, 0)->data[2]);
    CHECK_EQUAL(0x00, sense(msc, 0)->data[12]);
    Exchange *absent = sense(msc, 1);
    CHECK_EQUAL(0x02, absent->data[2]);
    CHECK_EQUAL(0x3A, absent->data[12]);
}

static void history_records_only_the_first_lun(void)
{
    static const uint8_t sense_block[6] = {0x03, 0, 0, 0, 18, 0};
    static const uint8_t inquiry[6] = {0x12, 0, 0, 0, 36, 0};
    Msc *msc = started();
    send_command(msc, 18, true, 1, sense_block, 6);
    collect(msc);
    send_command(msc, 36, true, 0, inquiry, 6);
    collect(msc);
    MscHistory history = msc_history(msc);
    CHECK_EQUAL(1, history.count);
    CHECK_EQUAL(0x12, history.entries[0].opcode);
}

static int collect_chunks(Msc *msc, uint32_t capacity, uint8_t *data, uint32_t *lengths)
{
    static uint32_t chunk[128];
    uint32_t length = 0;
    uint32_t offset = 0;
    int chunks = 0;
    while (msc_next_in(msc, (uint8_t *)chunk, capacity, &length)) {
        if (length == 13 && memcmp(chunk, "USBS", 4) == 0) {
            break;
        }
        memcpy(data + offset, chunk, length);
        offset += length;
        lengths[chunks++] = length;
    }
    return chunks;
}

static void sectors_come_out_in_whole_sector_chunks(void)
{
    static uint8_t data[3 * 512];
    uint32_t lengths[8] = {0};
    uint8_t block[10];
    Msc *msc = started();
    read_10_block(block, 0x28, 4, 3);
    send_command(msc, 3 * 512, true, 0, block, 10);
    CHECK_EQUAL(3, collect_chunks(msc, 512, data, lengths));
    CHECK_EQUAL(512, lengths[0]);
    CHECK_EQUAL(512, lengths[2]);
    CHECK(memcmp(data, writable_disk.bytes + 4 * 512, sizeof data) == 0);
}

static void a_reply_fits_in_one_chunk(void)
{
    static uint8_t data[512];
    uint32_t lengths[8] = {0};
    static const uint8_t block[6] = {0x1A, 0, 0x3F, 0, 192, 0};
    Msc *msc = started();
    send_command(msc, 192, true, 0, block, 6);
    CHECK_EQUAL(1, collect_chunks(msc, 512, data, lengths));
    CHECK_EQUAL(192, lengths[0]);
}

static void prefetching_reads_the_next_sector_once(void)
{
    static uint32_t chunk[128];
    uint32_t length = 0;
    uint8_t block[10];
    Msc *msc = started();
    read_10_block(block, 0x28, 0, 2);
    send_command(msc, 2 * 512, true, 0, block, 10);
    CHECK(msc_next_in(msc, (uint8_t *)chunk, 512, &length));
    CHECK_EQUAL(1, disk_reads);
    msc_prefetch(msc);
    CHECK_EQUAL(2, disk_reads);
    msc_prefetch(msc);
    CHECK_EQUAL(2, disk_reads);
    CHECK(msc_next_in(msc, (uint8_t *)chunk, 512, &length));
    CHECK_EQUAL(512, length);
    CHECK_EQUAL(2, disk_reads);
    CHECK(memcmp(chunk, writable_disk.bytes + 512, 512) == 0);
}

static void prefetching_outside_a_read_does_nothing(void)
{
    static const uint8_t inquiry[6] = {0x12, 0, 0, 0, 36, 0};
    Msc *msc = started();
    msc_prefetch(msc);
    send_command(msc, 36, true, 0, inquiry, 6);
    msc_prefetch(msc);
    CHECK_EQUAL(0, disk_reads);
}

static void eject(Msc *msc, uint8_t lun)
{
    static const uint8_t block[6] = {0x1B, 0, 0, 0, 0x02, 0};
    send_command(msc, 0, false, lun, block, 6);
    collect(msc);
}

static void the_session_finishes_only_when_every_present_lun_is_ejected(void)
{
    Msc *msc = started();
    CHECK(!msc_finished(msc));
    eject(msc, 0);
    CHECK(!msc_finished(msc));
    eject(msc, 1);
    CHECK(msc_finished(msc));
}

static void an_absent_lun_does_not_need_ejecting(void)
{
    static Msc msc;
    MscLun luns[2] = {
        {ram_read, ram_write, &writable_disk, DISK_SECTORS, true, true, "Internal storage"},
        {ram_read, ram_write, &read_only_disk, DISK_SECTORS, true, false, "SD card"},
    };
    msc_init(&msc, luns, 2);
    eject(&msc, 0);
    CHECK(msc_finished(&msc));
}

static void writes_are_counted_per_lun(void)
{
    uint8_t block[10];
    uint8_t bytes[1024] = {0};
    Msc *msc = started();
    msc->luns[1].writable = true;
    read_10_block(block, 0x2A, 2, 2);
    send_command(msc, sizeof bytes, false, 1, block, 10);
    send_data(msc, bytes, sizeof bytes);
    collect(msc);
    read_10_block(block, 0x2A, 2, 1);
    send_command(msc, 512, false, 0, block, 10);
    send_data(msc, bytes, 512);
    collect(msc);
    MscStats stats = msc_stats(msc);
    CHECK_EQUAL(1, stats.lun_sectors_written[0]);
    CHECK_EQUAL(2, stats.lun_sectors_written[1]);
    CHECK_EQUAL(3, stats.sectors_written);
}

static void without_a_buffer_each_sector_is_its_own_write(void)
{
    uint8_t block[10];
    static uint8_t bytes[3 * 512];
    fill_pattern(bytes, sizeof bytes, 1);
    Msc *msc = started();
    read_10_block(block, 0x2A, 1, 3);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 0, 0);
    CHECK_EQUAL(3, write_call_count);
    check_write_call(0, 1, 1);
    check_write_call(2, 3, 1);
    CHECK(memcmp(writable_disk.bytes + 512, bytes, sizeof bytes) == 0);
}

static void buffered_writes_reach_the_drive_in_buffer_sized_runs(void)
{
    uint8_t block[10];
    static uint8_t bytes[10 * 512];
    fill_pattern(bytes, sizeof bytes, 2);
    Msc *msc = started_with_buffer();
    read_10_block(block, 0x2A, 20, 10);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 0, 0);
    CHECK_EQUAL(3, write_call_count);
    check_write_call(0, 20, 4);
    check_write_call(1, 24, 4);
    check_write_call(2, 28, 2);
    CHECK(memcmp(writable_disk.bytes + 20 * 512, bytes, sizeof bytes) == 0);
    MscStats stats = msc_stats(msc);
    CHECK_EQUAL(10, stats.sectors_written);
    CHECK_EQUAL(10, stats.lun_sectors_written[0]);
}

static void a_buffered_write_in_odd_sized_packets_lands_intact(void)
{
    uint8_t block[10];
    static uint8_t bytes[6 * 512];
    fill_pattern(bytes, sizeof bytes, 3);
    Msc *msc = started_with_buffer();
    read_10_block(block, 0x2A, 30, 6);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    for (uint32_t offset = 0; offset < sizeof bytes; offset += 37) {
        uint32_t chunk = sizeof bytes - offset < 37 ? sizeof bytes - offset : 37;
        msc_receive(msc, bytes + offset, chunk);
    }
    check_csw(collect(msc), 0, 0);
    CHECK_EQUAL(2, write_call_count);
    check_write_call(0, 30, 4);
    check_write_call(1, 34, 2);
    CHECK(memcmp(writable_disk.bytes + 30 * 512, bytes, sizeof bytes) == 0);
}

static void a_failed_batch_fails_the_command_and_stops_writing(void)
{
    uint8_t block[10];
    static uint8_t bytes[12 * 512];
    memset(bytes, 0x6B, sizeof bytes);
    Msc *msc = started_with_buffer();
    failing_write_lba = 5;
    uint8_t untouched = writable_disk.bytes[8 * 512];
    read_10_block(block, 0x2A, 0, 12);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 1, sizeof bytes);
    CHECK_EQUAL(2, write_call_count);
    check_write_call(1, 4, 4);
    CHECK_EQUAL(untouched, writable_disk.bytes[8 * 512]);
    MscStats stats = msc_stats(msc);
    CHECK_EQUAL(4, stats.sectors_written);
    CHECK_EQUAL(1, stats.write_failures);
    CHECK_EQUAL(4, stats.last_failed_lba);
    Exchange *sensed = sense(msc, 0);
    CHECK_EQUAL(0x03, sensed->data[2]);
    CHECK_EQUAL(0x0C, sensed->data[12]);
}

static void a_short_data_phase_writes_only_the_whole_sectors_received(void)
{
    uint8_t block[10];
    static uint8_t bytes[700];
    fill_pattern(bytes, sizeof bytes, 4);
    Msc *msc = started_with_buffer();
    uint8_t untouched = writable_disk.bytes[7 * 512];
    read_10_block(block, 0x2A, 6, 2);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 2, 0);
    CHECK_EQUAL(1, write_call_count);
    check_write_call(0, 6, 1);
    CHECK(memcmp(writable_disk.bytes + 6 * 512, bytes, 512) == 0);
    CHECK_EQUAL(untouched, writable_disk.bytes[7 * 512]);
}

static void a_reset_drops_a_partly_filled_batch(void)
{
    uint8_t block[10];
    static uint8_t bytes[4 * 512];
    fill_pattern(bytes, sizeof bytes, 5);
    Msc *msc = started_with_buffer();
    read_10_block(block, 0x2A, 0, 4);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, 2 * 512);
    msc_reset(msc);
    CHECK_EQUAL(0, write_call_count);
    read_10_block(block, 0x2A, 8, 1);
    send_command(msc, 512, false, 0, block, 10);
    send_data(msc, bytes, 512);
    check_csw(collect(msc), 0, 0);
    CHECK_EQUAL(1, write_call_count);
    check_write_call(0, 8, 1);
    CHECK(memcmp(writable_disk.bytes + 8 * 512, bytes, 512) == 0);
}

static void a_missing_buffer_falls_back_to_single_sectors(void)
{
    uint8_t block[10];
    static uint8_t bytes[2 * 512];
    fill_pattern(bytes, sizeof bytes, 6);
    Msc *msc = started();
    msc_use_write_buffer(msc, NULL, 4);
    read_10_block(block, 0x2A, 2, 2);
    send_command(msc, sizeof bytes, false, 0, block, 10);
    send_data(msc, bytes, sizeof bytes);
    check_csw(collect(msc), 0, 0);
    CHECK_EQUAL(2, write_call_count);
    check_write_call(1, 3, 1);
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
        TEST_CASE(counters_record_commands_sectors_and_failures),
        TEST_CASE(a_failed_write_skips_the_rest_of_the_command),
        TEST_CASE(inquiry_for_a_vital_product_data_page_is_an_invalid_field),
        TEST_CASE(history_keeps_the_last_commands_other_than_reads_writes_and_ready_checks),
        TEST_CASE(history_holds_the_most_recent_entries),
        TEST_CASE(each_lun_keeps_its_own_sense),
        TEST_CASE(history_records_only_the_first_lun),
        TEST_CASE(sectors_come_out_in_whole_sector_chunks),
        TEST_CASE(a_reply_fits_in_one_chunk),
        TEST_CASE(prefetching_reads_the_next_sector_once),
        TEST_CASE(prefetching_outside_a_read_does_nothing),
        TEST_CASE(the_session_finishes_only_when_every_present_lun_is_ejected),
        TEST_CASE(an_absent_lun_does_not_need_ejecting),
        TEST_CASE(writes_are_counted_per_lun),
        TEST_CASE(without_a_buffer_each_sector_is_its_own_write),
        TEST_CASE(buffered_writes_reach_the_drive_in_buffer_sized_runs),
        TEST_CASE(a_buffered_write_in_odd_sized_packets_lands_intact),
        TEST_CASE(a_failed_batch_fails_the_command_and_stops_writing),
        TEST_CASE(a_short_data_phase_writes_only_the_whole_sectors_received),
        TEST_CASE(a_reset_drops_a_partly_filled_batch),
        TEST_CASE(a_missing_buffer_falls_back_to_single_sectors),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
