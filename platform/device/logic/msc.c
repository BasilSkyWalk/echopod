#include "device/msc.h"

#include <stddef.h>

enum {
    CBW_LENGTH = 31,
    CSW_LENGTH = 13,
    CBW_FLAGS = 12,
    CBW_LUN = 13,
    CBW_BLOCK = 15,
    DIRECTION_IN = 0x80,
    STATUS_GOOD = 0,
    STATUS_FAILED = 1,
    STATUS_PHASE_ERROR = 2,
    SCSI_TEST_UNIT_READY = 0x00,
    SCSI_REQUEST_SENSE = 0x03,
    SCSI_INQUIRY = 0x12,
    SCSI_MODE_SENSE_6 = 0x1A,
    SCSI_START_STOP_UNIT = 0x1B,
    SCSI_PREVENT_ALLOW = 0x1E,
    SCSI_READ_FORMAT_CAPACITIES = 0x23,
    SCSI_READ_CAPACITY_10 = 0x25,
    SCSI_READ_10 = 0x28,
    SCSI_WRITE_10 = 0x2A,
    SCSI_VERIFY_10 = 0x2F,
    SCSI_SYNCHRONIZE_CACHE = 0x35,
    SCSI_MODE_SENSE_10 = 0x5A,
    SENSE_NONE = 0x00,
    SENSE_NOT_READY = 0x02,
    SENSE_MEDIUM_ERROR = 0x03,
    SENSE_ILLEGAL_REQUEST = 0x05,
    SENSE_DATA_PROTECT = 0x07,
    CODE_WRITE_ERROR = 0x0C,
    CODE_UNRECOVERED_READ = 0x11,
    CODE_INVALID_COMMAND = 0x20,
    CODE_OUT_OF_RANGE = 0x21,
    CODE_INVALID_FIELD = 0x24,
    CODE_INVALID_LUN = 0x25,
    CODE_WRITE_PROTECTED = 0x27,
    CODE_MEDIUM_NOT_PRESENT = 0x3A,
    WRITE_PROTECT_BIT = 0x80,
    START_STOP_LOAD_EJECT = 0x02,
    START_STOP_START = 0x01,
    INQUIRY_EVPD = 0x01
};

static const uint8_t INQUIRY_HEADER[8] = {0x00, 0x80, 0x02, 0x02, 31, 0, 0, 0};
static const char INQUIRY_VENDOR[] = "ECHOFW  ";
static const char INQUIRY_REVISION[] = "0.1 ";

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static uint32_t be32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

static void put_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void put_le32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

void msc_init(Msc *msc, const MscLun *luns, uint8_t count)
{
    *msc = (Msc){.lun_count = count, .stats = {.last_failed_lba = UINT32_MAX}};
    for (uint8_t index = 0; index < count && index < MSC_MAX_LUNS; index++) {
        msc->luns[index] = luns[index];
    }
}

void msc_reset(Msc *msc)
{
    msc->phase = MSC_PHASE_COMMAND;
    msc->source = MSC_SOURCE_NONE;
}

MscStats msc_stats(const Msc *msc)
{
    return msc->stats;
}

MscHistory msc_history(const Msc *msc)
{
    MscHistory history = {.count = msc->history_count};
    uint32_t first = (uint32_t)(msc->history_next + MSC_HISTORY_CAPACITY - msc->history_count);
    for (uint32_t index = 0; index < msc->history_count; index++) {
        history.entries[index] = msc->history[(first + index) % MSC_HISTORY_CAPACITY];
    }
    return history;
}

static bool is_routine(uint8_t opcode)
{
    return opcode == SCSI_READ_10 || opcode == SCSI_WRITE_10 || opcode == SCSI_TEST_UNIT_READY;
}

static void remember(Msc *msc)
{
    if (msc->lun != 0 || is_routine(msc->opcode)) {
        return;
    }
    msc->history[msc->history_next] = (MscHistoryEntry){msc->opcode, msc->lun, msc->status};
    msc->history_next = (uint8_t)((msc->history_next + 1) % MSC_HISTORY_CAPACITY);
    if (msc->history_count < MSC_HISTORY_CAPACITY) {
        msc->history_count++;
    }
}

bool msc_ejected(const Msc *msc, uint8_t lun)
{
    return (msc->ejected & (1u << lun)) != 0;
}

static void set_sense(Msc *msc, uint8_t key, uint8_t code)
{
    if (msc->lun < MSC_MAX_LUNS) {
        msc->sense_key[msc->lun] = key;
        msc->sense_code[msc->lun] = code;
    }
    if (key != SENSE_NONE) {
        msc->status = STATUS_FAILED;
    }
}

static void respond(Msc *msc, uint32_t length)
{
    msc->source = MSC_SOURCE_RESPONSE;
    msc->response_length = length;
    msc->valid_bytes = length < msc->expected ? length : msc->expected;
}

static void fill_text(uint8_t *bytes, const char *text, uint32_t length)
{
    uint32_t index = 0;
    for (; index < length && text[index] != '\0'; index++) {
        bytes[index] = (uint8_t)text[index];
    }
    for (; index < length; index++) {
        bytes[index] = ' ';
    }
}

static void inquiry(Msc *msc, const MscLun *lun, const uint8_t *block)
{
    if ((block[1] & INQUIRY_EVPD) != 0) {
        set_sense(msc, SENSE_ILLEGAL_REQUEST, CODE_INVALID_FIELD);
        return;
    }
    for (uint32_t index = 0; index < sizeof INQUIRY_HEADER; index++) {
        msc->response[index] = INQUIRY_HEADER[index];
    }
    fill_text(msc->response + 8, INQUIRY_VENDOR, 8);
    fill_text(msc->response + 16, lun->product, MSC_PRODUCT_LENGTH);
    fill_text(msc->response + 32, INQUIRY_REVISION, 4);
    respond(msc, MSC_RESPONSE_CAPACITY);
}

static void request_sense(Msc *msc)
{
    for (uint32_t index = 0; index < 18; index++) {
        msc->response[index] = 0;
    }
    msc->response[0] = 0x70;
    msc->response[2] = msc->sense_key[msc->lun];
    msc->response[7] = 10;
    msc->response[12] = msc->sense_code[msc->lun];
    msc->sense_key[msc->lun] = SENSE_NONE;
    msc->sense_code[msc->lun] = 0;
    respond(msc, 18);
}

static void read_capacity(Msc *msc, const MscLun *lun)
{
    put_be32(msc->response, lun->sector_count - 1);
    put_be32(msc->response + 4, MSC_SECTOR_SIZE);
    respond(msc, 8);
}

static void read_format_capacities(Msc *msc, const MscLun *lun)
{
    static const uint8_t HEADER[4] = {0, 0, 0, 8};
    for (uint32_t index = 0; index < 4; index++) {
        msc->response[index] = HEADER[index];
    }
    put_be32(msc->response + 4, lun->sector_count);
    put_be32(msc->response + 8, 0x02000200u);
    respond(msc, 12);
}

static void mode_sense(Msc *msc, const MscLun *lun, bool ten_byte)
{
    uint8_t protect = lun->writable ? 0 : WRITE_PROTECT_BIT;
    for (uint32_t index = 0; index < 8; index++) {
        msc->response[index] = 0;
    }
    if (ten_byte) {
        msc->response[1] = 6;
        msc->response[3] = protect;
        respond(msc, 8);
    } else {
        msc->response[0] = 3;
        msc->response[2] = protect;
        respond(msc, 4);
    }
}

static bool start_transfer(Msc *msc, const MscLun *lun, const uint8_t *block)
{
    uint32_t count = (uint32_t)block[7] << 8 | block[8];
    msc->lba = be32(block + 2);
    msc->sector_offset = 0;
    if (msc->lba > lun->sector_count || count > lun->sector_count - msc->lba) {
        set_sense(msc, SENSE_ILLEGAL_REQUEST, CODE_OUT_OF_RANGE);
        return false;
    }
    if (count * MSC_SECTOR_SIZE != msc->expected) {
        msc->status = STATUS_PHASE_ERROR;
    }
    msc->valid_bytes =
        count * MSC_SECTOR_SIZE < msc->expected ? count * MSC_SECTOR_SIZE : msc->expected;
    return true;
}

static void read_10(Msc *msc, const MscLun *lun, const uint8_t *block)
{
    if (start_transfer(msc, lun, block)) {
        msc->source = MSC_SOURCE_SECTORS;
    }
}

static void write_10(Msc *msc, const MscLun *lun, const uint8_t *block)
{
    if (!lun->writable) {
        set_sense(msc, SENSE_DATA_PROTECT, CODE_WRITE_PROTECTED);
        return;
    }
    if (start_transfer(msc, lun, block)) {
        msc->source = MSC_SOURCE_SECTORS;
        msc->data_out = true;
    }
}

static void start_stop(Msc *msc, const uint8_t *block)
{
    if ((block[4] & START_STOP_LOAD_EJECT) != 0 && (block[4] & START_STOP_START) == 0) {
        msc->ejected = (uint8_t)(msc->ejected | (1u << msc->lun));
        msc->luns[msc->lun].present = false;
    }
}

static void execute_ready(Msc *msc, const MscLun *lun, const uint8_t *block)
{
    switch (block[0]) {
    case SCSI_READ_CAPACITY_10:
        read_capacity(msc, lun);
        break;
    case SCSI_READ_FORMAT_CAPACITIES:
        read_format_capacities(msc, lun);
        break;
    case SCSI_MODE_SENSE_6:
    case SCSI_MODE_SENSE_10:
        mode_sense(msc, lun, block[0] == SCSI_MODE_SENSE_10);
        break;
    case SCSI_READ_10:
        read_10(msc, lun, block);
        break;
    case SCSI_WRITE_10:
        write_10(msc, lun, block);
        break;
    case SCSI_TEST_UNIT_READY:
    case SCSI_PREVENT_ALLOW:
    case SCSI_VERIFY_10:
    case SCSI_SYNCHRONIZE_CACHE:
        break;
    default:
        set_sense(msc, SENSE_ILLEGAL_REQUEST, CODE_INVALID_COMMAND);
        break;
    }
}

static void execute(Msc *msc, const uint8_t *block)
{
    if (msc->lun >= msc->lun_count) {
        set_sense(msc, SENSE_ILLEGAL_REQUEST, CODE_INVALID_LUN);
        return;
    }
    const MscLun *lun = &msc->luns[msc->lun];
    if (block[0] == SCSI_INQUIRY) {
        inquiry(msc, lun, block);
    } else if (block[0] == SCSI_REQUEST_SENSE) {
        request_sense(msc);
    } else if (block[0] == SCSI_START_STOP_UNIT) {
        start_stop(msc, block);
    } else if (!lun->present) {
        set_sense(msc, SENSE_NOT_READY, CODE_MEDIUM_NOT_PRESENT);
    } else {
        execute_ready(msc, lun, block);
    }
}

static void begin_command(Msc *msc, const uint8_t *cbw)
{
    msc->stats.commands++;
    msc->opcode = cbw[CBW_BLOCK];
    msc->tag = le32(cbw + 4);
    msc->expected = le32(cbw + 8);
    msc->lun = cbw[CBW_LUN] & 0x0F;
    msc->transferred = 0;
    msc->valid_bytes = 0;
    msc->status = STATUS_GOOD;
    msc->source = MSC_SOURCE_NONE;
    msc->data_out = false;
    execute(msc, cbw + CBW_BLOCK);
    bool host_in = (cbw[CBW_FLAGS] & DIRECTION_IN) != 0;
    if (msc->source != MSC_SOURCE_NONE && msc->expected > 0 && host_in == msc->data_out) {
        msc->status = STATUS_PHASE_ERROR;
        msc->source = MSC_SOURCE_NONE;
        msc->valid_bytes = 0;
    }
    if (msc->expected == 0) {
        msc->phase = MSC_PHASE_STATUS;
    } else {
        msc->phase = host_in ? MSC_PHASE_DATA_IN : MSC_PHASE_DATA_OUT;
    }
}

static bool is_command_block(const uint8_t *packet, uint32_t length)
{
    return length == CBW_LENGTH && packet[0] == 'U' && packet[1] == 'S' && packet[2] == 'B' &&
           packet[3] == 'C';
}

static void store_out_byte(Msc *msc, uint8_t value)
{
    uint8_t *sector = (uint8_t *)msc->sector;
    bool keep = msc->source == MSC_SOURCE_SECTORS && msc->transferred < msc->valid_bytes;
    msc->transferred++;
    if (!keep) {
        return;
    }
    sector[msc->sector_offset++] = value;
    if (msc->sector_offset == MSC_SECTOR_SIZE) {
        const MscLun *lun = &msc->luns[msc->lun];
        if (lun->write(lun->context, msc->lba, 1, msc->sector)) {
            msc->stats.sectors_written++;
        } else {
            set_sense(msc, SENSE_MEDIUM_ERROR, CODE_WRITE_ERROR);
            msc->source = MSC_SOURCE_NONE;
            msc->stats.write_failures++;
            msc->stats.last_failed_lba = msc->lba;
        }
        msc->lba++;
        msc->sector_offset = 0;
    }
}

void msc_receive(Msc *msc, const uint8_t *packet, uint32_t length)
{
    if (msc->phase == MSC_PHASE_COMMAND) {
        if (is_command_block(packet, length)) {
            begin_command(msc, packet);
        }
        return;
    }
    if (msc->phase != MSC_PHASE_DATA_OUT) {
        return;
    }
    for (uint32_t index = 0; index < length && msc->transferred < msc->expected; index++) {
        store_out_byte(msc, packet[index]);
    }
    if (msc->transferred == msc->expected) {
        msc->phase = MSC_PHASE_STATUS;
    }
}

static uint8_t sector_byte(Msc *msc)
{
    const MscLun *lun = &msc->luns[msc->lun];
    if (msc->sector_offset == 0) {
        if (!lun->read(lun->context, msc->lba, 1, msc->sector)) {
            set_sense(msc, SENSE_MEDIUM_ERROR, CODE_UNRECOVERED_READ);
            msc->source = MSC_SOURCE_NONE;
            msc->stats.read_failures++;
            msc->stats.last_failed_lba = msc->lba;
            return 0;
        }
        msc->stats.sectors_read++;
    }
    uint8_t value = ((const uint8_t *)msc->sector)[msc->sector_offset++];
    if (msc->sector_offset == MSC_SECTOR_SIZE) {
        msc->sector_offset = 0;
        msc->lba++;
    }
    return value;
}

static uint8_t next_in_byte(Msc *msc)
{
    uint32_t position = msc->transferred++;
    if (position >= msc->valid_bytes) {
        return 0;
    }
    if (msc->source == MSC_SOURCE_RESPONSE) {
        return msc->response[position];
    }
    return msc->source == MSC_SOURCE_SECTORS ? sector_byte(msc) : 0;
}

static uint32_t residue(const Msc *msc)
{
    bool delivered = msc->status == STATUS_GOOD || msc->status == STATUS_PHASE_ERROR;
    uint32_t used = delivered ? msc->valid_bytes : 0;
    return msc->expected - used;
}

static void status_packet(Msc *msc, uint8_t *packet, uint32_t *length)
{
    packet[0] = 'U';
    packet[1] = 'S';
    packet[2] = 'B';
    packet[3] = 'S';
    put_le32(packet + 4, msc->tag);
    put_le32(packet + 8, residue(msc));
    packet[12] = msc->status;
    *length = CSW_LENGTH;
    remember(msc);
    msc->phase = MSC_PHASE_COMMAND;
}

bool msc_next_in(Msc *msc, uint8_t *packet, uint32_t *length)
{
    if (msc->phase == MSC_PHASE_STATUS) {
        status_packet(msc, packet, length);
        return true;
    }
    if (msc->phase != MSC_PHASE_DATA_IN) {
        return false;
    }
    uint32_t count = 0;
    while (count < MSC_PACKET_SIZE && msc->transferred < msc->expected) {
        packet[count++] = next_in_byte(msc);
    }
    *length = count;
    if (msc->transferred == msc->expected) {
        msc->phase = MSC_PHASE_STATUS;
    }
    return true;
}
