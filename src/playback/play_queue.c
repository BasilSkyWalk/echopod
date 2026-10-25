#include "playback/play_queue.h"

enum {
    MOST_ENTRIES = UINT16_MAX
};

static bool shuffles_albums(const PlayQueue *queue)
{
    return queue->plan.shuffle == SHUFFLE_ALBUMS;
}

static bool anchor_in_list(const PlayQueue *queue)
{
    return queue->anchor_base < queue->base_count;
}

static bool source_track(const PlayQueue *queue, uint32_t position, uint16_t *track)
{
    BrowseItem item;
    if (!browse_item(queue->library, queue->plan.source, position, &item) ||
        item.kind != BROWSE_ITEM_TRACK) {
        return false;
    }
    *track = item.index;
    return true;
}

static bool base_value(const PlayQueue *queue, uint32_t base, uint16_t *value)
{
    if (base >= queue->base_count) {
        return false;
    }
    if (shuffles_albums(queue)) {
        *value = (uint16_t)base;
        return true;
    }
    return source_track(queue, queue->first_track + base, value);
}

static uint32_t first_track_position(const PlayQueue *queue, uint32_t available)
{
    uint16_t track = 0;
    uint32_t position = 0;
    while (position < available && !source_track(queue, position, &track)) {
        position++;
    }
    return position;
}

static uint16_t find_anchor(const PlayQueue *queue)
{
    uint16_t track = 0;
    for (uint16_t base = 0; base < queue->base_count; base++) {
        if (base_value(queue, base, &track) && track == queue->anchor_value) {
            return base;
        }
    }
    return queue->base_count;
}

static void finish_layout(PlayQueue *queue)
{
    bool shuffled = queue->plan.shuffle != SHUFFLE_OFF;
    if (!anchor_in_list(queue) && queue->base_count == MOST_ENTRIES) {
        queue->base_count--;
        queue->anchor_base = queue->base_count;
    }
    queue->count = anchor_in_list(queue) ? queue->base_count : (uint16_t)(queue->base_count + 1);
    queue->cursor.position = !shuffled && anchor_in_list(queue) ? queue->anchor_base : 0;
    queue->order = permutation_make(shuffled ? (uint32_t)queue->count - 1 : 0, queue->plan.seed);
}

static bool build_track_queue(PlayQueue *queue)
{
    uint32_t available = browse_count(queue->library, queue->plan.source);
    queue->first_track = first_track_position(queue, available);
    uint32_t tracks = available - queue->first_track;
    queue->base_count = (uint16_t)(tracks < MOST_ENTRIES ? tracks : MOST_ENTRIES);
    queue->anchor_value = queue->plan.anchor_track;
    queue->anchor_base = find_anchor(queue);
    finish_layout(queue);
    return true;
}

static bool build_album_queue(PlayQueue *queue)
{
    LibraryTrack anchor;
    LibraryAlbum album;
    uint32_t albums = library_music_count(queue->library, LIBRARY_ALBUMS);
    if (!library_read_track(queue->library, queue->plan.anchor_track, &anchor) ||
        !library_read_album(queue->library, anchor.album, &album) || anchor.album >= albums) {
        return false;
    }
    queue->base_count = (uint16_t)(albums < MOST_ENTRIES ? albums : MOST_ENTRIES);
    queue->anchor_value = anchor.album;
    queue->anchor_base = anchor.album;
    finish_layout(queue);
    queue->cursor.album_track = (uint16_t)(queue->plan.anchor_track - album.first_track);
    return true;
}

bool play_queue_build(PlayQueue *queue, const Library *library, PlayQueuePlan plan)
{
    *queue = (PlayQueue){.library = library, .plan = plan};
    return shuffles_albums(queue) ? build_album_queue(queue) : build_track_queue(queue);
}

static bool entry_at(const PlayQueue *queue, uint16_t position, uint16_t *value)
{
    if (position >= queue->count) {
        return false;
    }
    bool shuffled = queue->plan.shuffle != SHUFFLE_OFF;
    if (!shuffled && anchor_in_list(queue)) {
        return base_value(queue, position, value);
    }
    if (position == 0) {
        *value = queue->anchor_value;
        return true;
    }
    uint32_t base = (uint32_t)position - 1;
    if (shuffled) {
        base = permutation_at(&queue->order, base);
    }
    if (anchor_in_list(queue) && base >= queue->anchor_base) {
        base++;
    }
    return base_value(queue, base, value);
}

static bool album_at(const PlayQueue *queue, uint16_t position, LibraryAlbum *album)
{
    uint16_t index = 0;
    return entry_at(queue, position, &index) && library_read_album(queue->library, index, album);
}

bool play_queue_track_at(const PlayQueue *queue, PlayQueueCursor cursor, uint16_t *track)
{
    if (!shuffles_albums(queue)) {
        return entry_at(queue, cursor.position, track);
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
