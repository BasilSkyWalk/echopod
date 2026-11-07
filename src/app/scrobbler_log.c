#include "scrobbler_log.h"

#include "core/string_builder.h"
#include "hal/storage.h"
#include "hal/system.h"

const char SCROBBLER_LOG_PATH[] = "/.scrobbler.log";
const char SCROBBLER_MARK_PATH[] = "/.echo/history-exported.txt";

static const char HEADER[] = "#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/EchoPod";

enum {
    LINE_LIMIT = HISTORY_CHECK_AT - HISTORY_TEXT_AT + 40,
    OUT_BYTES = 3 * LINE_LIMIT + 1,
    VERSION_LIMIT = 32,
    TEXT_COUNT = 3,
    SHORTEST_SCROBBLE_MS = 30000,
    ENOUGH_LISTENING_MS = 240000,
    CONTINUE_SLACK_MS = 5000,
    MS_PER_SECOND = 1000,
    DECIMAL_BASE = 10
};

typedef struct Chain {
    uint32_t played_ms;
    uint32_t start;
    bool done;
} Chain;

typedef struct Export {
    HalFile history;
    uint32_t slots;
    uint32_t oldest;
    uint32_t newest;
    uint32_t first;
    uint32_t processed;
    uint32_t covered;
    uint32_t log_size;
    uint32_t lines;
    uint32_t buffered;
    bool log_made;
    bool mark_made;
    bool failed;
    StringBuilder out;
    char bytes[OUT_BYTES];
} Export;

static bool load(Export *export, uint32_t sequence, uint8_t *slot, HistoryRecord *record)
{
    hal_system_keep_alive();
    return hal_file_read(export->history, (sequence - 1) % export->slots * HISTORY_SLOT_BYTES, slot,
                         HISTORY_SLOT_BYTES) &&
           listen_history_decode(slot, record) && record->sequence == sequence;
}

static uint32_t text_end(const uint8_t *slot, uint32_t at)
{
    while (at < HISTORY_CHECK_AT && slot[at] != 0) {
        at++;
    }
    return at;
}

static uint32_t after_text(const uint8_t *slot, uint32_t at)
{
    at = text_end(slot, at);
    return at < HISTORY_CHECK_AT ? at + 1 : at;
}

static uint32_t texts_end(const uint8_t *slot)
{
    uint32_t at = HISTORY_TEXT_AT;
    for (int text = 0; text < TEXT_COUNT; text++) {
        at = after_text(slot, at);
    }
    return at;
}

static bool continues(const uint8_t *earlier_slot, const HistoryRecord *earlier,
                      const uint8_t *later_slot, const HistoryRecord *later)
{
    uint32_t end = texts_end(earlier_slot);
    uint32_t from = later->start_position_ms;
    uint32_t to = earlier->end_position_ms;
    return earlier->duration_ms == later->duration_ms && end == texts_end(later_slot) &&
           __builtin_memcmp(earlier_slot + HISTORY_TEXT_AT, later_slot + HISTORY_TEXT_AT,
                            end - HISTORY_TEXT_AT) == 0 &&
           from >= CONTINUE_SLACK_MS && (from > to ? from - to : to - from) <= CONTINUE_SLACK_MS;
}

static bool listened(uint32_t duration_ms, uint32_t played_ms)
{
    uint32_t half = duration_ms / 2;
    return duration_ms > SHORTEST_SCROBBLE_MS &&
           played_ms >= (half < ENOUGH_LISTENING_MS ? half : ENOUGH_LISTENING_MS);
}

static void put_number(Export *export, uint32_t value)
{
    string_builder_append_unsigned(&export->out, value);
    string_builder_append_char(&export->out, '\t');
}

static void emit(Export *export, const uint8_t *slot, const HistoryRecord *record, uint32_t start,
                 char rating)
{
    if (record->sequence < export->first) {
        return;
    }
    uint32_t at = HISTORY_TEXT_AT;
    for (int text = 0; text < TEXT_COUNT; text++) {
        for (uint32_t end = text_end(slot, at); at < end; at++) {
            char character = (char)slot[at];
            bool breaks = character == '\t' || character == '\r' || character == '\n';
            string_builder_append_char(&export->out, breaks ? ' ' : character);
        }
        string_builder_append_char(&export->out, '\t');
        at = after_text(slot, at);
    }
    if (record->number != 0) {
        string_builder_append_unsigned(&export->out, record->number);
    }
    string_builder_append_char(&export->out, '\t');
    put_number(export, record->duration_ms / MS_PER_SECOND);
    string_builder_append_char(&export->out, rating);
    string_builder_append_char(&export->out, '\t');
    put_number(export, start);
    string_builder_append_char(&export->out, '\n');
    export->buffered++;
}

static bool mark_ready(void)
{
    HalFile file;
    HalReserveResult result = hal_file_reserve(SCROBBLER_MARK_PATH, SCROBBLER_MARK_BYTES);
    if (result == HAL_RESERVE_CREATED) {
        return true;
    }
    if (result != HAL_RESERVE_EXISTS || !hal_file_open(&file, SCROBBLER_MARK_PATH)) {
        return false;
    }
    uint32_t size = file.size;
    hal_file_close(&file);
    return size >= SCROBBLER_MARK_BYTES ||
           hal_file_grow(SCROBBLER_MARK_PATH, SCROBBLER_MARK_BYTES) == HAL_RESERVE_CREATED;
}

static HalReserveResult make_room(const Export *export, uint32_t length)
{
    return export->log_made ? hal_file_grow(SCROBBLER_LOG_PATH, export->log_size + length)
                            : hal_file_reserve(SCROBBLER_LOG_PATH, length);
}

static void flush(Export *export, uint32_t covered)
{
    if (export->failed || export->buffered == 0) {
        return;
    }
    uint32_t length = (uint32_t)export->out.length;
    export->mark_made = export->mark_made || mark_ready();
    export->failed =
        !export->mark_made || make_room(export, length) != HAL_RESERVE_CREATED ||
        !hal_file_write_at(SCROBBLER_LOG_PATH, export->log_size, export->bytes, length);
    if (export->failed) {
        return;
    }
    export->log_made = true;
    export->log_size += length;
    export->covered = covered;
    export->lines += export->buffered;
    export->buffered = 0;
    export->out = string_builder_make(export->bytes, OUT_BYTES);
}

static uint32_t find_song(Export *export, uint32_t sequence, bool back, uint8_t *slot,
                          HistoryRecord *record)
{
    while (sequence >= export->oldest && sequence <= export->newest) {
        if (load(export, sequence, slot, record) && record->kind == HISTORY_PLAY) {
            return sequence;
        }
        sequence = back ? sequence - 1 : sequence + 1;
    }
    return 0;
}

static uint32_t walk_back(Export *export, uint8_t *later_slot, uint8_t *earlier_slot)
{
    HistoryRecord later;
    HistoryRecord earlier;
    uint32_t start = export->first;
    uint32_t at = find_song(export, start, false, later_slot, &later);
    while (at != 0) {
        uint32_t before = find_song(export, at - 1, true, earlier_slot, &earlier);
        if (before == 0 || !continues(earlier_slot, &earlier, later_slot, &later)) {
            break;
        }
        uint8_t *swapped = later_slot;
        later_slot = earlier_slot;
        earlier_slot = swapped;
        later = earlier;
        start = at = before;
    }
    return start;
}

static bool session_offset(Export *export, uint32_t sequence, uint8_t *slot, int64_t *offset)
{
    HistoryRecord record;
    bool started = false;
    bool clocked = false;
    uint32_t session = 0;
    uint32_t last_end = 0;
    int64_t gap = 0;
    int64_t found = 0;
    for (; sequence <= export->newest; sequence++) {
        if (!load(export, sequence, slot, &record)) {
            continue;
        }
        if (started && record.session != session) {
            if (clocked) {
                break;
            }
            gap += last_end;
        }
        started = true;
        session = record.session;
        if (record.unix_time != 0) {
            clocked = true;
            found = (int64_t)record.unix_time * MS_PER_SECOND - record.end_ms;
        }
        last_end = record.end_ms;
    }
    *offset = found - gap;
    return clocked;
}

static void finish_play(Export *export, const Chain *chain, const uint8_t *slot,
                        const HistoryRecord *record, bool joined)
{
    if (!chain->done && !joined) {
        emit(export, slot, record, chain->start, 'S');
    }
}

static void run(Export *export, uint8_t *spare)
{
    uint8_t own[HISTORY_SLOT_BYTES];
    uint8_t *reading = own;
    uint8_t *pending = spare;
    HistoryRecord record;
    HistoryRecord previous = {.sequence = 0};
    Chain chain = {.done = false};
    bool open = false;
    bool in_session = false;
    bool clocked = false;
    uint32_t session = 0;
    uint32_t last_end = 0;
    uint32_t dark = 0;
    int64_t offset = 0;
    uint32_t sequence = walk_back(export, reading, pending);
    for (; sequence <= export->newest && !export->failed; sequence++) {
        if (!load(export, sequence, reading, &record)) {
            continue;
        }
        if (!in_session || record.session != session) {
            if (in_session && !clocked) {
                offset += last_end;
            } else if (!session_offset(export, sequence, reading, &offset)) {
                dark = sequence;
                break;
            } else if (!load(export, sequence, reading, &record)) {
                continue;
            }
            in_session = true;
            session = record.session;
            clocked = false;
        }
        clocked = clocked || record.unix_time != 0;
        last_end = record.end_ms;
        if (record.kind == HISTORY_PLAY) {
            bool joined = open && continues(pending, &previous, reading, &record);
            if (open) {
                finish_play(export, &chain, pending, &previous, joined);
            }
            if (!joined) {
                chain = (Chain){.start = (uint32_t)((offset + record.start_ms) / MS_PER_SECOND)};
            }
            chain.played_ms += record.played_ms;
            if (!chain.done && listened(record.duration_ms, chain.played_ms)) {
                emit(export, reading, &record, chain.start, 'L');
                chain.done = true;
            }
            uint8_t *swapped = pending;
            pending = reading;
            reading = swapped;
            previous = record;
            open = true;
        }
        export->processed = sequence;
        if (export->out.length > LINE_LIMIT) {
            flush(export, open && !chain.done ? previous.sequence - 1 : sequence);
        }
    }
    if (open && !export->failed) {
        bool joined = dark != 0 && find_song(export, dark, false, reading, &record) != 0 &&
                      continues(pending, &previous, reading, &record);
        finish_play(export, &chain, pending, &previous, joined);
    }
    flush(export, export->processed);
    if (!export->failed && export->lines > 0) {
        export->covered = export->processed;
    }
}

static uint32_t read_mark(uint32_t newest)
{
    HalFile file;
    char text[SCROBBLER_MARK_BYTES];
    uint32_t value = 0;
    if (!hal_file_open(&file, SCROBBLER_MARK_PATH)) {
        return 0;
    }
    uint32_t length = file.size < SCROBBLER_MARK_BYTES ? file.size : SCROBBLER_MARK_BYTES;
    if (hal_file_read(file, 0, text, length)) {
        for (uint32_t index = 0; index < length && text[index] >= '0' && text[index] <= '9';
             index++) {
            value = value * DECIMAL_BASE + (uint32_t)(text[index] - '0');
        }
    }
    hal_file_close(&file);
    return value <= newest ? value : 0;
}

static void write_mark(uint32_t covered)
{
    char text[SCROBBLER_MARK_BYTES];
    text[SCROBBLER_MARK_BYTES - 1] = '\n';
    for (int index = SCROBBLER_MARK_BYTES - 2; index >= 0; index--) {
        text[index] = (char)('0' + covered % DECIMAL_BASE);
        covered /= DECIMAL_BASE;
    }
    (void)hal_file_write_at(SCROBBLER_MARK_PATH, 0, text, SCROBBLER_MARK_BYTES);
}

static void open_log(Export *export, const char *version)
{
    HalFile log;
    char last = '\n';
    export->out = string_builder_make(export->bytes, OUT_BYTES);
    export->log_made = hal_file_open(&log, SCROBBLER_LOG_PATH);
    export->log_size = export->log_made ? log.size : 0;
    if (export->log_size > 0 && !hal_file_read(log, export->log_size - 1, &last, 1)) {
        last = '\n';
    }
    hal_file_close(&log);
    if (export->log_size == 0) {
        string_builder_append(&export->out, HEADER);
        if (version[0] != '\0') {
            string_builder_append_char(&export->out, ' ');
        }
        for (int index = 0; index < VERSION_LIMIT && version[index] != '\0'; index++) {
            string_builder_append_char(&export->out, version[index]);
        }
        string_builder_append_char(&export->out, '\n');
    } else if (last != '\n') {
        string_builder_append_char(&export->out, '\n');
    }
}

uint32_t scrobbler_log_write(ListenHistory *history, const char *version)
{
    Export export;
    if (!history->located || history->slots == 0 ||
        !hal_file_open(&export.history, LISTEN_HISTORY_PATH)) {
        return 0;
    }
    export.slots = history->slots;
    export.newest = history->sequence - 1;
    export.oldest = export.newest > export.slots ? export.newest - export.slots + 1 : 1;
    uint32_t exported = read_mark(export.newest);
    export.first = exported + 1 > export.oldest ? exported + 1 : export.oldest;
    export.processed = export.first - 1;
    export.covered = exported;
    export.lines = 0;
    export.buffered = 0;
    export.mark_made = false;
    export.failed = false;
    open_log(&export, version);
    run(&export, history->slot);
    hal_file_close(&export.history);
    if (export.covered > exported) {
        write_mark(export.covered);
    }
    return export.lines;
}
