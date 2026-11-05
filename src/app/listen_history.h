#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/library.h"
#include "player.h"

extern const char LISTEN_HISTORY_PATH[];

enum {
    HISTORY_SLOT_BYTES = 512,
    HISTORY_SLOT_COUNT = 2048,
    HISTORY_FILE_BYTES = HISTORY_SLOT_BYTES * HISTORY_SLOT_COUNT,
    HISTORY_FIELDS_AT = 4,
    HISTORY_TEXT_AT = 44,
    HISTORY_CHECK_AT = HISTORY_SLOT_BYTES - 4,
    HISTORY_NAME_BYTES = 160,
    HISTORY_MIN_PLAY_MS = 1000,
    HISTORY_RESTART_MS = 3000
};

typedef enum HistoryKind {
    HISTORY_PLAY = 1,
    HISTORY_COMPUTER = 2
} HistoryKind;

typedef struct HistoryRecord {
    uint32_t sequence;
    uint32_t session;
    uint8_t kind;
    uint8_t number;
    uint16_t reserved;
    uint32_t start_ms;
    uint32_t end_ms;
    uint32_t unix_time;
    uint32_t played_ms;
    uint32_t duration_ms;
    uint32_t start_position_ms;
    uint32_t end_position_ms;
} HistoryRecord;

typedef struct ListenHistory {
    bool open;
    bool playing;
    bool located;
    uint16_t track;
    uint32_t position_ms;
    uint32_t last_ms;
    uint32_t sequence;
    uint32_t session;
    uint32_t slots;
    HistoryRecord record;
    uint8_t slot[HISTORY_SLOT_BYTES];
} ListenHistory;

void listen_history_encode(const HistoryRecord *record, uint8_t *slot);
bool listen_history_decode(const uint8_t *slot, HistoryRecord *record);
void listen_history_follow(ListenHistory *history, const Library *library,
                           const PlayerStatus *status, uint32_t now_ms);
void listen_history_close(ListenHistory *history, uint32_t now_ms);
void listen_history_note_computer(ListenHistory *history, uint32_t now_ms);
