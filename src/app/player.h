#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/audio.h"
#include "library/library.h"
#include "library/library_browse.h"
#include "playback/play_queue.h"

enum {
    PLAYER_DEFAULT_VOLUME = 30,
    PLAYER_RESTART_THRESHOLD_MS = 3000,
    PLAYER_SAVE_INTERVAL_MS = 15000,
    PLAYER_PATH_CAPACITY = 260
};

typedef enum PlayerState {
    PLAYER_IDLE,
    PLAYER_PLAYING,
    PLAYER_PAUSED
} PlayerState;

typedef struct PlayerStatus {
    PlayerState state;
    bool has_track;
    uint16_t track;
    uint32_t position_ms;
    uint32_t duration_ms;
    uint8_t volume;
    ShuffleMode shuffle;
    RepeatMode repeat;
} PlayerStatus;

typedef struct Player {
    const Library *library;
    uint32_t fingerprint;
    PlayQueue queue;
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
    uint32_t last_save_ms;
} Player;

void player_init(Player *player, const Library *library);
bool player_play_list(Player *player, BrowseQuery source, uint32_t position, uint32_t seed);
void player_toggle_pause(Player *player);
void player_skip(Player *player, int direction);
void player_change_volume(Player *player, int delta);
void player_set_shuffle(Player *player, ShuffleMode shuffle, uint32_t seed);
void player_set_repeat(Player *player, RepeatMode repeat);
void player_update(Player *player, uint32_t now_ms);
bool player_save(Player *player);
bool player_restore(Player *player);
PlayerStatus player_status(const Player *player);
