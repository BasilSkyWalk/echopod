#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/audio.h"
#include "playback/play_queue.h"

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
    PlayQueuePlace place;
    HalAudioFormat format;
    bool spoken;
} PlayerStatus;
