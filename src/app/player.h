#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal/audio.h"
#include "library/library.h"
#include "library/library_browse.h"
#include "playback/play_queue.h"
#include "playback/up_next.h"

enum {
    PLAYER_DEFAULT_VOLUME = 12,
    PLAYER_RESTART_THRESHOLD_MS = 3000,
    PLAYER_SAVE_INTERVAL_MS = 15000,
    PLAYER_PATH_CAPACITY = 260
};

typedef enum PlayerState {
    PLAYER_IDLE,
    PLAYER_PLAYING,
    PLAYER_PAUSED
} PlayerState;

typedef enum PlayerUpNextResult {
    PLAYER_UP_NEXT_ADDED,
    PLAYER_UP_NEXT_STARTED,
    PLAYER_UP_NEXT_FULL,
    PLAYER_UP_NEXT_FAILED
} PlayerUpNextResult;

typedef enum PlayerNextSource {
    PLAYER_NEXT_FROM_QUEUE,
    PLAYER_NEXT_FROM_UP_NEXT,
    PLAYER_NEXT_SAME_SONG
} PlayerNextSource;

typedef struct PlayerStatus {
    PlayerState state;
    bool has_track;
    uint16_t track;
    uint32_t position_ms;
    uint32_t duration_ms;
    uint8_t volume;
    ShuffleMode shuffle;
    RepeatMode repeat;
    PlayQueuePlace place;
    HalAudioFormat format;
    bool spoken;
} PlayerStatus;

typedef struct PlayerResume {
    uint32_t (*position_for)(void *context, uint16_t track);
    void *context;
} PlayerResume;

typedef struct PlayerListStart {
    BrowseQuery source;
    uint32_t position;
    uint32_t seed;
    bool in_order;
} PlayerListStart;

typedef struct Player {
    const Library *library;
    uint32_t fingerprint;
    PlayQueue queue;
    UpNext up_next;
    bool from_up_next;
    RepeatMode repeat;
    ShuffleMode shuffle;
    uint8_t volume;
    PlayerState state;
    bool has_track;
    bool loaded;
    uint16_t track;
    uint32_t paused_position_ms;
    uint32_t token_counter;
    uint32_t current_token;
    bool has_next;
    uint32_t next_token;
    PlayQueueCursor next_cursor;
    PlayerNextSource next_source;
    uint32_t last_save_ms;
    HalAudioJack jack;
    uint32_t refusal_count;
    HalAudioRefusal refusal;
    bool spoken;
    PlayerResume resume;
} Player;

extern const char PLAYER_RESUME_PATH[];
extern const char PLAYER_UP_NEXT_PATH[];

void player_init(Player *player, const Library *library);
bool player_play_list(Player *player, BrowseQuery source, uint32_t position, uint32_t seed);
bool player_play_from(Player *player, PlayerListStart start);
void player_set_resume(Player *player, PlayerResume resume);
void player_jump(Player *player, int32_t delta_ms);
void player_toggle_pause(Player *player);
void player_skip(Player *player, int direction);
void player_seek(Player *player, uint32_t position_ms);
void player_change_volume(Player *player, int delta);
void player_set_shuffle(Player *player, ShuffleMode shuffle, uint32_t seed);
void player_set_repeat(Player *player, RepeatMode repeat);
PlayerUpNextResult player_play_next(Player *player, uint16_t track);
PlayerUpNextResult player_add_to_up_next(Player *player, uint16_t track);
bool player_jump_to_up_next(Player *player, uint16_t index);
bool player_jump_to_queue(Player *player, PlayQueueCursor cursor);
void player_update(Player *player, uint32_t now_ms);
void player_refusal_text(HalAudioRefusal refusal, char *text, size_t capacity);
bool player_save(Player *player);
void player_release_output(Player *player);
bool player_restore(Player *player);
PlayerStatus player_status(const Player *player);
