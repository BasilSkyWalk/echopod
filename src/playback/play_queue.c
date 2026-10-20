#include "playback/play_queue.h"

#include "core/random.h"

static bool shuffles_albums(const PlayQueue *queue)
{
    return queue->plan.shuffle == SHUFFLE_ALBUMS;
}

static void swap_entries(PlayQueue *queue, uint16_t first, uint16_t second)
{
    uint16_t value = queue->entries[first];
    queue->entries[first] = queue->entries[second];
    queue->entries[second] = value;
}

enum {
    FEWEST_TO_SHUFFLE = 3
};

static void shuffle_after_first(PlayQueue *queue)
{
    if (queue->count < FEWEST_TO_SHUFFLE) {
        return;
    }
    Random random = random_seeded(queue->plan.seed);
    for (uint16_t index = (uint16_t)(queue->count - 1); index > 1; index--) {
        uint16_t other = (uint16_t)(1 + random_below(&random, index));
        swap_entries(queue, index, other);
    }
}

static void move_to_front(PlayQueue *queue, uint16_t position)
{
    swap_entries(queue, 0, position);
    queue->cursor.position = 0;
}

static void collect_source_tracks(PlayQueue *queue)
{
    uint32_t available = browse_count(queue->library, queue->plan.source);
    for (uint32_t position = 0; position < available && queue->count < PLAY_QUEUE_CAPACITY;
         position++) {
        BrowseItem item;
        if (browse_item(queue->library, queue->plan.source, position, &item) &&
            item.kind == BROWSE_ITEM_TRACK) {
            queue->entries[queue->count++] = item.index;
        }
    }
}

static bool find_entry(const PlayQueue *queue, uint16_t value, uint16_t *position)
{
    for (uint16_t index = 0; index < queue->count; index++) {
        if (queue->entries[index] == value) {
            *position = index;
            return true;
        }
    }
    return false;
}

static void insert_at_front(PlayQueue *queue, uint16_t value)
{
    uint16_t kept = queue->count < PLAY_QUEUE_CAPACITY ? queue->count : PLAY_QUEUE_CAPACITY - 1;
    for (uint16_t index = kept; index > 0; index--) {
        queue->entries[index] = queue->entries[index - 1];
    }
    queue->entries[0] = value;
    queue->count = (uint16_t)(kept + 1);
}

static void place_anchor(PlayQueue *queue)
{
    uint16_t position = 0;
    if (find_entry(queue, queue->plan.anchor_track, &position)) {
        queue->cursor.position = position;
        return;
    }
    insert_at_front(queue, queue->plan.anchor_track);
    queue->cursor.position = 0;
}

static bool build_track_queue(PlayQueue *queue)
{
    collect_source_tracks(queue);
    place_anchor(queue);
    if (queue->plan.shuffle == SHUFFLE_SONGS) {
        move_to_front(queue, queue->cursor.position);
        shuffle_after_first(queue);
    }
    return true;
}

static bool build_album_queue(PlayQueue *queue)
{
    LibraryTrack anchor;
    LibraryAlbum album;
    uint32_t albums = library_count(queue->library, LIBRARY_ALBUMS);
    if (!library_read_track(queue->library, queue->plan.anchor_track, &anchor) ||
        !library_read_album(queue->library, anchor.album, &album)) {
        return false;
    }
    for (uint32_t index = 0; index < albums && index < PLAY_QUEUE_CAPACITY; index++) {
        queue->entries[queue->count++] = (uint16_t)index;
    }
    move_to_front(queue, anchor.album);
    shuffle_after_first(queue);
    queue->cursor.album_track = (uint16_t)(queue->plan.anchor_track - album.first_track);
    return true;
}

bool play_queue_build(PlayQueue *queue, const Library *library, PlayQueuePlan plan)
{
    queue->library = library;
    queue->plan = plan;
    queue->count = 0;
    queue->cursor = (PlayQueueCursor){0, 0};
    return shuffles_albums(queue) ? build_album_queue(queue) : build_track_queue(queue);
}

static bool album_at(const PlayQueue *queue, uint16_t position, LibraryAlbum *album)
{
    return position < queue->count &&
           library_read_album(queue->library, queue->entries[position], album);
}

bool play_queue_track_at(const PlayQueue *queue, PlayQueueCursor cursor, uint16_t *track)
{
    if (!shuffles_albums(queue)) {
        if (cursor.position >= queue->count) {
            return false;
        }
        *track = queue->entries[cursor.position];
        return true;
    }
    LibraryAlbum album;
    if (!album_at(queue, cursor.position, &album) || cursor.album_track >= album.track_count) {
        return false;
    }
    *track = (uint16_t)(album.first_track + cursor.album_track);
    return true;
}

bool play_queue_current(const PlayQueue *queue, uint16_t *track)
{
    return play_queue_track_at(queue, queue->cursor, track);
}

static bool neighbor_position(const PlayQueue *queue, uint16_t from, int direction, bool wrap,
                              uint16_t *result)
{
    int target = from + direction;
    if (target >= 0 && target < queue->count) {
        *result = (uint16_t)target;
        return true;
    }
    if (!wrap || queue->count == 0) {
        return false;
    }
    *result = (uint16_t)(target < 0 ? queue->count - 1 : 0);
    return true;
}

static bool album_neighbor(const PlayQueue *queue, PlayQueueCursor from, int direction, bool wrap,
                           PlayQueueCursor *result)
{
    LibraryAlbum album;
    if (!album_at(queue, from.position, &album)) {
        return false;
    }
    int track = from.album_track + direction;
    if (track >= 0 && track < album.track_count) {
        *result = (PlayQueueCursor){from.position, (uint16_t)track};
        return true;
    }
    uint16_t position = 0;
    if (!neighbor_position(queue, from.position, direction, wrap, &position) ||
        !album_at(queue, position, &album)) {
        return false;
    }
    uint16_t last_track = album.track_count > 0 ? (uint16_t)(album.track_count - 1) : 0;
    *result = (PlayQueueCursor){position, direction > 0 ? 0 : last_track};
    return true;
}

bool play_queue_step(const PlayQueue *queue, PlayQueueCursor from, int direction, bool wrap,
                     PlayQueueCursor *result)
{
    if (shuffles_albums(queue)) {
        return album_neighbor(queue, from, direction, wrap, result);
    }
    uint16_t position = 0;
    if (!neighbor_position(queue, from.position, direction, wrap, &position)) {
        return false;
    }
    *result = (PlayQueueCursor){position, 0};
    return true;
}

bool play_queue_neighbor(const PlayQueue *queue, int direction, bool wrap, PlayQueueCursor *result)
{
    return play_queue_step(queue, queue->cursor, direction, wrap, result);
}

bool play_queue_upcoming(const PlayQueue *queue, RepeatMode repeat, PlayQueueCursor *result)
{
    if (repeat == REPEAT_ONE) {
        *result = queue->cursor;
        return queue->count > 0;
    }
    return play_queue_neighbor(queue, 1, repeat == REPEAT_ALL, result);
}

void play_queue_move_to(PlayQueue *queue, PlayQueueCursor cursor)
{
    queue->cursor = cursor;
}

PlayQueuePlace play_queue_place(const PlayQueue *queue)
{
    if (!shuffles_albums(queue)) {
        if (queue->cursor.position >= queue->count) {
            return (PlayQueuePlace){0, 0};
        }
        return (PlayQueuePlace){(uint16_t)(queue->cursor.position + 1), queue->count};
    }
    LibraryAlbum album;
    if (!album_at(queue, queue->cursor.position, &album)) {
        return (PlayQueuePlace){0, 0};
    }
    return (PlayQueuePlace){(uint16_t)(queue->cursor.album_track + 1), album.track_count};
}
