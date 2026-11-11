#include "spoken_store.h"

#include "core/format.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "hal/storage.h"

const char SPOKEN_STORE_PATH[] = "/.echo/bookmarks.bin";

enum {
    TIME_CAPACITY = 16
};

static bool track_path(const Library *library, uint16_t track, LibraryTrack *record, char *path)
{
    return library_read_track(library, track, record) &&
           library_read_string(library, record->path, path, PLAYER_PATH_CAPACITY);
}

static bool read_bookmarks(uint8_t *bytes, uint32_t *length)
{
    HalFile file;
    if (!hal_file_open(&file, SPOKEN_STORE_PATH)) {
        return false;
    }
    *length = file.size < BOOKMARKS_FILE_CAPACITY ? file.size : BOOKMARKS_FILE_CAPACITY;
    bool read = hal_file_read(file, 0, bytes, *length);
    hal_file_close(&file);
    return read;
}

void spoken_store_load(SpokenStore *store, const Library *library)
{
    uint8_t bytes[BOOKMARKS_FILE_CAPACITY];
    uint32_t length = 0;
    store->library = library;
    store->follow = (SpokenFollow){.known = false, .active = false};
    store->last_state = PLAYER_IDLE;
    store->dirty = false;
    store->last_put_ms = 0;
    store->last_save_ms = 0;
    if (!read_bookmarks(bytes, &length) || !bookmarks_decode(bytes, length, &store->bookmarks)) {
        bookmarks_clear(&store->bookmarks);
    }
}

static void save(SpokenStore *store, uint32_t now_ms)
{
    uint8_t bytes[BOOKMARKS_FILE_CAPACITY];
    uint32_t length = bookmarks_encode(&store->bookmarks, bytes);
    (void)hal_file_write(SPOKEN_STORE_PATH, bytes, length);
    store->dirty = false;
    store->last_save_ms = now_ms;
}

static void put(SpokenStore *store, Bookmark bookmark, uint32_t now_ms)
{
    bookmarks_put(&store->bookmarks, bookmark);
    store->dirty = true;
    store->last_put_ms = now_ms;
}

static void leave(SpokenStore *store, uint32_t now_ms)
{
    Bookmark bookmark = store->follow.bookmark;
    if (spoken_near_end(bookmark.position_ms, store->follow.duration_ms)) {
        bookmark.flags = BOOKMARK_FINISHED;
        bookmark.position_ms = 0;
    }
    put(store, bookmark, now_ms);
    store->follow.active = false;
}

static bool single_file_book(const SpokenStore *store, const LibraryTrack *record, const char *path)
{
    LibraryFolder folder;
    if (spoken_bookmark_key(path) == spoken_track_key(path)) {
        return true;
    }
    return library_read_folder(store->library, record->folder, &folder) && folder.track_count == 1;
}

static void begin(SpokenStore *store, uint16_t track)
{
    LibraryTrack record;
    char path[PLAYER_PATH_CAPACITY];
    uint16_t chapter = 0;
    store->follow = (SpokenFollow){.known = true, .active = false, .track = track};
    if (!track_path(store->library, track, &record, path) || spoken_kind_of(path) == SPOKEN_NONE) {
        return;
    }
    spoken_chapter_of(store->library, track, &chapter);
    store->follow.embedded = record.chapters > 0 && single_file_book(store, &record, path);
    store->follow.active = true;
    store->follow.duration_ms = record.duration_ms;
    store->follow.bookmark = (Bookmark){
        .key = spoken_bookmark_key(path), .track_key = spoken_track_key(path), .chapter = chapter};
}

static void record(SpokenStore *store, PlayerStatus status, uint32_t now_ms)
{
    store->follow.bookmark.position_ms = status.position_ms;
    store->follow.bookmark.flags = 0;
    if (store->follow.embedded) {
        store->follow.bookmark.chapter = status.chapter;
    }
    bool state_changed = status.state != store->last_state;
    if (state_changed || now_ms - store->last_put_ms >= SPOKEN_PUT_INTERVAL_MS) {
        put(store, store->follow.bookmark, now_ms);
    }
}

static void save_when_due(SpokenStore *store, PlayerState state, uint32_t now_ms)
{
    bool stopped_playing = state != PLAYER_PLAYING && store->last_state == PLAYER_PLAYING;
    bool due = now_ms - store->last_save_ms >= SPOKEN_SAVE_INTERVAL_MS;
    if (store->dirty && (stopped_playing || !store->follow.active || due)) {
        save(store, now_ms);
    }
}

static bool listening(PlayerStatus status)
{
    return status.has_track && status.state != PLAYER_IDLE && status.spoken;
}

void spoken_store_follow(SpokenStore *store, PlayerStatus status, uint32_t now_ms)
{
    bool moved = !store->follow.known || status.track != store->follow.track;
    if (store->follow.active && (!listening(status) || moved)) {
        leave(store, now_ms);
    }
    if (!listening(status)) {
        store->follow.known = false;
    } else if (moved) {
        begin(store, status.track);
        store->last_put_ms = now_ms - SPOKEN_PUT_INTERVAL_MS;
    }
    if (store->follow.active) {
        record(store, status, now_ms);
    }
    save_when_due(store, status.state, now_ms);
    store->last_state = status.state;
}

void spoken_store_flush(SpokenStore *store, PlayerStatus status, uint32_t now_ms)
{
    spoken_store_follow(store, status, now_ms);
    if (store->follow.active) {
        put(store, store->follow.bookmark, now_ms);
    }
    if (store->dirty) {
        save(store, now_ms);
    }
}

bool spoken_store_track(const SpokenStore *store, uint16_t track, Bookmark *bookmark)
{
    LibraryTrack record;
    char path[PLAYER_PATH_CAPACITY];
    return track_path(store->library, track, &record, path) &&
           bookmarks_find(&store->bookmarks, spoken_bookmark_key(path), bookmark) &&
           bookmark->track_key == spoken_track_key(path);
}

static uint32_t resume_position(void *context, uint16_t track)
{
    Bookmark bookmark;
    if (!spoken_store_track(context, track, &bookmark) || bookmark_finished(bookmark)) {
        return 0;
    }
    return bookmark.position_ms;
}

PlayerResume spoken_store_resume(SpokenStore *store)
{
    return (PlayerResume){.position_for = resume_position, .context = store};
}

bool spoken_store_book(const SpokenStore *store, uint16_t folder, Bookmark *bookmark)
{
    uint32_t key = 0;
    return spoken_folder_key(store->library, folder, &key) &&
           bookmarks_find(&store->bookmarks, key, bookmark);
}

static bool chapter_matches(const SpokenStore *store, uint16_t folder, uint16_t chapter,
                            uint32_t track_key)
{
    uint16_t track = 0;
    LibraryTrack record;
    char path[PLAYER_PATH_CAPACITY];
    return spoken_chapter_track(store->library, folder, chapter, &track) &&
           track_path(store->library, track, &record, path) && spoken_track_key(path) == track_key;
}

uint16_t spoken_store_resume_chapter(const SpokenStore *store, uint16_t folder)
{
    Bookmark bookmark;
    LibraryFolder record;
    if (!spoken_store_book(store, folder, &bookmark) || bookmark_finished(bookmark) ||
        !library_read_folder(store->library, folder, &record)) {
        return 0;
    }
    if (chapter_matches(store, folder, bookmark.chapter, bookmark.track_key)) {
        return bookmark.chapter;
    }
    for (uint16_t chapter = 0; chapter < record.track_count; chapter++) {
        if (chapter_matches(store, folder, chapter, bookmark.track_key)) {
            return chapter;
        }
    }
    return 0;
}

static void append_count(StringBuilder *builder, uint32_t count, StringId one, StringId many)
{
    char number[TIME_CAPACITY];
    StringBuilder digits = string_builder_make(number, sizeof number);
    string_builder_append_unsigned(&digits, count);
    const char *arguments[] = {number};
    i18n_format(builder, count == 1 ? one : many, arguments, 1);
}

static void append_progress(StringBuilder *builder, uint32_t chapter, uint32_t total)
{
    char chapter_text[TIME_CAPACITY];
    char total_text[TIME_CAPACITY];
    StringBuilder chapter_digits = string_builder_make(chapter_text, sizeof chapter_text);
    StringBuilder total_digits = string_builder_make(total_text, sizeof total_text);
    string_builder_append_unsigned(&chapter_digits, chapter + 1u);
    string_builder_append_unsigned(&total_digits, total);
    const char *arguments[] = {chapter_text, total_text};
    i18n_format(builder, STR_MENU_CHAPTER_PROGRESS, arguments, 2);
}

static void describe_book(StringBuilder *builder, uint32_t count, const Bookmark *bookmark)
{
    if (bookmark == NULL) {
        append_count(builder, count, STR_MENU_CHAPTER_ONE, STR_MENU_CHAPTER_MANY);
    } else if (bookmark_finished(*bookmark)) {
        string_builder_append(builder, i18n_text(STR_MENU_FINISHED));
    } else {
        append_progress(builder, bookmark->chapter, count);
    }
}

static uint16_t embedded_chapters(const SpokenStore *store, uint16_t folder)
{
    uint16_t track = 0;
    LibraryTrack record;
    return spoken_chapter_track(store->library, folder, 0, &track) &&
                   library_read_track(store->library, track, &record)
               ? record.chapters
               : 0;
}

static bool describe_folder(const SpokenStore *store, SpokenRow row, StringBuilder *builder)
{
    LibraryFolder folder;
    Bookmark bookmark;
    if (!library_read_folder(store->library, row.item.index, &folder) || folder.track_count == 0) {
        return false;
    }
    if (row.shelf == SPOKEN_PODCAST) {
        append_count(builder, folder.track_count, STR_MENU_EPISODE_ONE, STR_MENU_EPISODE_MANY);
        return true;
    }
    uint16_t embedded = folder.track_count == 1 ? embedded_chapters(store, row.item.index) : 0;
    bool started =
        row.shelf == SPOKEN_AUDIOBOOK && spoken_store_book(store, row.item.index, &bookmark);
    describe_book(builder, embedded > 0 ? embedded : folder.track_count,
                  started ? &bookmark : NULL);
    return true;
}

static bool describe_track(const SpokenStore *store, SpokenRow row, StringBuilder *builder)
{
    Bookmark bookmark;
    LibraryTrack record;
    char path[PLAYER_PATH_CAPACITY];
    if (!track_path(store->library, row.item.index, &record, path)) {
        return false;
    }
    bool started = spoken_store_track(store, row.item.index, &bookmark);
    if (row.shelf == SPOKEN_AUDIOBOOK && record.chapters > 0 &&
        spoken_bookmark_key(path) == spoken_track_key(path)) {
        describe_book(builder, record.chapters, started ? &bookmark : NULL);
        return true;
    }
    if (!started) {
        return false;
    }
    if (bookmark_finished(bookmark)) {
        string_builder_append(builder, i18n_text(STR_DIAG_TONE_PLAYED));
        return true;
    }
    if (bookmark.position_ms == 0 || bookmark.position_ms >= record.duration_ms) {
        return false;
    }
    char left[TIME_CAPACITY];
    format_duration(record.duration_ms - bookmark.position_ms, left, sizeof left);
    const char *arguments[] = {left};
    i18n_format(builder, STR_MENU_TIME_LEFT, arguments, 1);
    return true;
}

bool spoken_store_describe(const SpokenStore *store, SpokenRow row)
{
    char detail[TIME_CAPACITY * 2];
    StringBuilder builder = string_builder_make(detail, sizeof detail);
    bool described = row.item.kind == BROWSE_ITEM_FOLDER  ? describe_folder(store, row, &builder)
                     : row.item.kind == BROWSE_ITEM_TRACK ? describe_track(store, row, &builder)
                                                          : false;
    if (!described) {
        return false;
    }
    StringBuilder out = string_builder_make(row.detail, row.capacity);
    string_builder_append(&out, detail);
    return true;
}
