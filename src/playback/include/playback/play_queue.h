#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/permutation.h"
#include "library/library.h"
#include "library/library_browse.h"

typedef enum ShuffleMode {
    SHUFFLE_OFF,
    SHUFFLE_SONGS,
    SHUFFLE_ALBUMS
} ShuffleMode;

typedef enum RepeatMode {
    REPEAT_OFF,
    REPEAT_ONE,
    REPEAT_ALL
} RepeatMode;

enum {
    SHUFFLE_MODE_COUNT = SHUFFLE_ALBUMS + 1,
    REPEAT_MODE_COUNT = REPEAT_ALL + 1
};

typedef struct PlayQueuePlan {
    BrowseQuery source;
    uint16_t anchor_track;
    ShuffleMode shuffle;
    uint32_t seed;
} PlayQueuePlan;

typedef struct PlayQueueCursor {
    uint16_t position;
    uint16_t album_track;
} PlayQueueCursor;

typedef struct PlayQueuePlace {
    uint16_t number;
    uint16_t total;
} PlayQueuePlace;

typedef struct PlayQueue {
    const Library *library;
    PlayQueuePlan plan;
    uint32_t first_track;
    uint16_t base_count;
    uint16_t anchor_base;
    uint16_t anchor_value;
    Permutation order;
    uint16_t count;
    PlayQueueCursor cursor;
} PlayQueue;

bool play_queue_build(PlayQueue *queue, const Library *library, PlayQueuePlan plan);
bool play_queue_track_at(const PlayQueue *queue, PlayQueueCursor cursor, uint16_t *track);
bool play_queue_current(const PlayQueue *queue, uint16_t *track);
bool play_queue_step(const PlayQueue *queue, PlayQueueCursor from, int direction, bool wrap,
                     PlayQueueCursor *result);
bool play_queue_neighbor(const PlayQueue *queue, int direction, bool wrap, PlayQueueCursor *result);
bool play_queue_upcoming(const PlayQueue *queue, RepeatMode repeat, PlayQueueCursor *result);
void play_queue_move_to(PlayQueue *queue, PlayQueueCursor cursor);
PlayQueuePlace play_queue_place(const PlayQueue *queue);
