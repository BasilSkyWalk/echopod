#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    MSC_MAX_LUNS = 2,
    MSC_SECTOR_SIZE = 512,
    MSC_PACKET_SIZE = 64,
    MSC_PRODUCT_LENGTH = 16,
    MSC_RESPONSE_CAPACITY = 36,
    MSC_HISTORY_CAPACITY = 8
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

typedef struct MscStats {
    uint32_t commands;
    uint32_t sectors_read;
    uint32_t sectors_written;
    uint32_t lun_sectors_written[MSC_MAX_LUNS];
    uint32_t read_failures;
    uint32_t write_failures;
    uint32_t last_failed_lba;
    uint32_t last_written_lba[MSC_MAX_LUNS];
    uint32_t writes_after_eject;
} MscStats;

typedef struct MscHistoryEntry {
    uint8_t opcode;
    uint8_t lun;
    uint8_t status;
} MscHistoryEntry;

typedef struct MscHistory {
    MscHistoryEntry entries[MSC_HISTORY_CAPACITY];
    uint8_t count;
} MscHistory;

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
    uint32_t read_first;
    uint32_t read_count;
    uint8_t sense_key[MSC_MAX_LUNS];
    uint8_t sense_code[MSC_MAX_LUNS];
    uint8_t ejected;
    uint8_t present_at_start;
    bool data_out;
    MscStats stats;
    uint8_t opcode;
    uint8_t history_next;
    uint8_t history_count;
    MscHistoryEntry history[MSC_HISTORY_CAPACITY];
    uint8_t response[MSC_RESPONSE_CAPACITY];
    uint32_t response_length;
    uint32_t sector[MSC_SECTOR_SIZE / 4];
    uint32_t *write_buffer;
    uint32_t write_sectors;
    uint32_t batch_bytes;
} Msc;

void msc_init(Msc *msc, const MscLun *luns, uint8_t count);
void msc_use_write_buffer(Msc *msc, uint32_t *words, uint32_t sectors);
void msc_reset(Msc *msc);
void msc_receive(Msc *msc, const uint8_t *packet, uint32_t length);
bool msc_next_in(Msc *msc, uint8_t *buffer, uint32_t capacity, uint32_t *length);
void msc_prefetch(Msc *msc);
bool msc_ejected(const Msc *msc, uint8_t lun);
bool msc_finished(const Msc *msc);
uint8_t msc_left_mounted(const Msc *msc);
MscStats msc_stats(const Msc *msc);
MscHistory msc_history(const Msc *msc);
