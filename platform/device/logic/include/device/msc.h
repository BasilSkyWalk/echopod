#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    MSC_MAX_LUNS = 2,
    MSC_SECTOR_SIZE = 512,
    MSC_PACKET_SIZE = 64,
    MSC_PRODUCT_LENGTH = 16,
    MSC_RESPONSE_CAPACITY = 36
};

typedef struct MscLun {
    bool (*read)(void *context, uint32_t lba, uint32_t count, void *buffer);
    bool (*write)(void *context, uint32_t lba, uint32_t count, const void *buffer);
    void *context;
    uint32_t sector_count;
    bool writable;
    bool present;
    char product[MSC_PRODUCT_LENGTH + 1];
} MscLun;

typedef enum MscPhase {
    MSC_PHASE_COMMAND,
    MSC_PHASE_DATA_IN,
    MSC_PHASE_DATA_OUT,
    MSC_PHASE_STATUS
} MscPhase;

typedef enum MscSource {
    MSC_SOURCE_RESPONSE,
    MSC_SOURCE_SECTORS,
    MSC_SOURCE_NONE
} MscSource;

typedef struct Msc {
    MscLun luns[MSC_MAX_LUNS];
    uint8_t lun_count;
    MscPhase phase;
    MscSource source;
    uint32_t tag;
    uint32_t expected;
    uint32_t transferred;
    uint32_t valid_bytes;
    uint8_t status;
    uint8_t lun;
    uint32_t lba;
    uint32_t sector_offset;
    uint8_t sense_key;
    uint8_t sense_code;
    uint8_t ejected;
    bool data_out;
    uint8_t response[MSC_RESPONSE_CAPACITY];
    uint32_t response_length;
    uint32_t sector[MSC_SECTOR_SIZE / 4];
} Msc;

void msc_init(Msc *msc, const MscLun *luns, uint8_t count);
void msc_reset(Msc *msc);
void msc_receive(Msc *msc, const uint8_t *packet, uint32_t length);
bool msc_next_in(Msc *msc, uint8_t *packet, uint32_t *length);
bool msc_ejected(const Msc *msc, uint8_t lun);
