#include "app/update_record.h"

#include "hal/storage.h"

const char UPDATE_RECORD_PATH[] = "/.echo/update.bin";

enum {
    RECORD_MAGIC = 0x44505545,
    RESULT_AT = 4,
    DETAIL_AT = 5,
    REPLIED_AT = 6,
    STATUS_AT = 8,
    CHECK_AT = 12
};

static void put_word(uint8_t *bytes, uint32_t value)
{
    for (int index = 0; index < 4; index++) {
        bytes[index] = (uint8_t)(value >> (8 * index));
    }
}

static uint32_t get_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static uint32_t check_of(const uint8_t *bytes)
{
    uint32_t sum = 0;
    for (int index = 0; index < CHECK_AT; index++) {
        sum = sum * 31u + bytes[index];
    }
    return ~sum;
}

void update_record_encode(HalUpdateRecord record, uint8_t *bytes)
{
    for (int index = 0; index < UPDATE_RECORD_BYTES; index++) {
        bytes[index] = 0;
    }
    put_word(bytes, RECORD_MAGIC);
    bytes[RESULT_AT] = (uint8_t)record.result;
    bytes[DETAIL_AT] = record.detail;
    bytes[REPLIED_AT] = record.emmc_replied ? 1 : 0;
    put_word(bytes + STATUS_AT, record.emmc_status);
    put_word(bytes + CHECK_AT, check_of(bytes));
}

bool update_record_decode(const uint8_t *bytes, HalUpdateRecord *record)
{
    if (get_word(bytes) != RECORD_MAGIC || get_word(bytes + CHECK_AT) != check_of(bytes) ||
        bytes[RESULT_AT] >= HAL_UPDATE_RESULT_COUNT) {
        return false;
    }
    *record = (HalUpdateRecord){.result = (HalUpdateResult)bytes[RESULT_AT],
                                .detail = bytes[DETAIL_AT],
                                .emmc_replied = bytes[REPLIED_AT] != 0,
                                .emmc_status = get_word(bytes + STATUS_AT)};
    return true;
}

bool update_record_read(HalUpdateRecord *record)
{
    HalFile file;
    uint8_t bytes[UPDATE_RECORD_BYTES];
    if (!hal_file_open(&file, UPDATE_RECORD_PATH)) {
        return false;
    }
    bool read = file.size >= UPDATE_RECORD_BYTES && hal_file_read(file, 0, bytes, sizeof bytes);
    hal_file_close(&file);
    return read && update_record_decode(bytes, record);
}

bool update_record_save(HalUpdateRecord record)
{
    uint8_t bytes[UPDATE_RECORD_BYTES];
    update_record_encode(record, bytes);
    return hal_file_write_at(UPDATE_RECORD_PATH, 0, bytes, sizeof bytes);
}
