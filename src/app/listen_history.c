#include "listen_history.h"

#include "core/hash.h"
#include "hal/storage.h"

const char LISTEN_HISTORY_PATH[] = "/.echo/history.bin";

_Static_assert(sizeof(HistoryRecord) == HISTORY_TEXT_AT - HISTORY_FIELDS_AT,
               "a record's fields fill the bytes before its texts");

static const uint8_t MAGIC[HISTORY_FIELDS_AT] = {'E', 'H', 'S', 'T'};

enum {
    NO_TEXT = -1
};

static uint32_t check_of(const uint8_t *slot)
{
    return hash_update(HASH_START, slot, HISTORY_CHECK_AT);
}

void listen_history_encode(const HistoryRecord *record, uint8_t *slot)
{
    __builtin_memcpy(slot, MAGIC, sizeof MAGIC);
    __builtin_memcpy(slot + HISTORY_FIELDS_AT, record, sizeof *record);
    uint32_t check = check_of(slot);
    __builtin_memcpy(slot + HISTORY_CHECK_AT, &check, sizeof check);
}

bool listen_history_decode(const uint8_t *slot, HistoryRecord *record)
{
    uint32_t check;
    __builtin_memcpy(record, slot + HISTORY_FIELDS_AT, sizeof *record);
    __builtin_memcpy(&check, slot + HISTORY_CHECK_AT, sizeof check);
    return __builtin_memcmp(slot, MAGIC, sizeof MAGIC) == 0 && record->sequence != 0 &&
           check == check_of(slot);
}

static bool slot_record(HalFile file, uint32_t slots, uint32_t index, HistoryRecord *record)
{
    uint8_t slot[HISTORY_SLOT_BYTES];
    return hal_file_read(file, index * HISTORY_SLOT_BYTES, slot, sizeof slot) &&
           listen_history_decode(slot, record) && (record->sequence - 1) % slots == index;
}

static void locate(ListenHistory *history)
{
    HalFile file;
    HistoryRecord record;
    HistoryRecord newest;
    if (!hal_file_open(&file, LISTEN_HISTORY_PATH)) {
        return;
    }
    uint32_t slots = file.size / HISTORY_SLOT_BYTES;
    uint32_t low = 1;
    uint32_t high = slots;
    if (slots > 0 && slot_record(file, slots, 0, &newest)) {
        while (low < high) {
            uint32_t middle = low + (high - low) / 2;
            if (slot_record(file, slots, middle, &record) &&
                record.sequence == newest.sequence + middle - (low - 1)) {
                newest = record;
                low = middle + 1;
            } else {
                high = middle;
            }
        }
    } else if (slots == 0 || !slot_record(file, slots, slots - 1, &newest)) {
        newest = (HistoryRecord){.sequence = 0, .session = 0};
    }
    hal_file_close(&file);
    history->slots = slots;
    history->sequence = newest.sequence + 1;
    history->session = newest.session + 1;
    history->located = slots > 0;
}

static void store(ListenHistory *history, HistoryRecord *record)
{
    if (!history->located) {
        locate(history);
    }
    if (!history->located) {
        return;
    }
    record->sequence = history->sequence;
    record->session = history->session;
    if (history->clock == NULL ||
        !wall_clock_unix(history->clock, record->end_ms, &record->unix_time)) {
        record->unix_time = 0;
    }
    listen_history_encode(record, history->slot);
    uint32_t offset = (record->sequence - 1) % history->slots * HISTORY_SLOT_BYTES;
    if (hal_file_write_at(LISTEN_HISTORY_PATH, offset, history->slot, HISTORY_SLOT_BYTES)) {
        history->sequence++;
    }
}

static void clear_texts(uint8_t *slot)
{
    __builtin_memset(slot + HISTORY_TEXT_AT, 0, HISTORY_CHECK_AT - HISTORY_TEXT_AT);
}

static uint32_t put_text(uint8_t *slot, uint32_t at, uint32_t end, const Library *library,
                         uint32_t offset)
{
    (void)library_read_string(library, offset, (char *)slot + at, end - at);
    while (slot[at] != 0) {
        at++;
    }
    return at + 1;
}

static void begin(ListenHistory *history, const Library *library, const PlayerStatus *status,
                  uint32_t now_ms)
{
    LibraryTrack track;
    LibraryGroup artist;
    LibraryAlbum album;
    bool known = library_read_track(library, status->track, &track);
    uint32_t artist_name =
        known && library_read_group(library, LIBRARY_ARTISTS, track.artist, &artist)
            ? artist.name
            : (uint32_t)NO_TEXT;
    uint32_t album_title =
        known && library_read_album(library, track.album, &album) ? album.title : (uint32_t)NO_TEXT;
    clear_texts(history->slot);
    uint32_t at = HISTORY_TEXT_AT;
    at = put_text(history->slot, at, at + HISTORY_NAME_BYTES, library, artist_name);
    at = put_text(history->slot, at, at + HISTORY_NAME_BYTES, library, album_title);
    put_text(history->slot, at, HISTORY_CHECK_AT, library, known ? track.title : (uint32_t)NO_TEXT);
    history->record = (HistoryRecord){.kind = HISTORY_PLAY,
                                      .number = known ? track.number : 0,
                                      .start_ms = now_ms,
                                      .duration_ms = status->duration_ms,
                                      .start_position_ms = status->position_ms};
    history->track = status->track;
    history->open = true;
}

static void count(ListenHistory *history, uint32_t now_ms)
{
    if (history->open && history->playing) {
        history->record.played_ms += now_ms - history->last_ms;
    }
    history->last_ms = now_ms;
}

void listen_history_close(ListenHistory *history, uint32_t now_ms)
{
    count(history, now_ms);
    if (!history->open) {
        return;
    }
    history->open = false;
    history->record.end_ms = now_ms;
    history->record.end_position_ms = history->position_ms;
    if (history->record.played_ms >= HISTORY_MIN_PLAY_MS) {
        store(history, &history->record);
    }
}

void listen_history_follow(ListenHistory *history, const Library *library,
                           const PlayerStatus *status, uint32_t now_ms)
{
    count(history, now_ms);
    bool listening = status->has_track && !status->spoken;
    bool restarted = status->position_ms < HISTORY_RESTART_MS &&
                     history->position_ms >= status->position_ms + HISTORY_RESTART_MS;
    if (history->open && (!listening || status->track != history->track || restarted)) {
        listen_history_close(history, now_ms);
    }
    history->playing = status->state == PLAYER_PLAYING;
    if (!history->open && listening && history->playing) {
        begin(history, library, status, now_ms);
    }
    history->position_ms = status->position_ms;
}

void listen_history_note_computer(ListenHistory *history, uint32_t now_ms)
{
    listen_history_close(history, now_ms);
    clear_texts(history->slot);
    HistoryRecord record = {.kind = HISTORY_COMPUTER, .start_ms = now_ms, .end_ms = now_ms};
    store(history, &record);
}
