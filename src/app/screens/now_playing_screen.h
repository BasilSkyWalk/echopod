#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/key_tracker.h"
#include "cover_cache.h"
#include "library/library.h"
#include "np_faces/now_playing_face.h"
#include "playback/lyrics.h"
#include "player.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    NOW_PLAYING_TEXT_CAPACITY = 128
};

typedef enum NowPlayingCommand {
    NOW_PLAYING_NONE,
    NOW_PLAYING_TOGGLE_PAUSE,
    NOW_PLAYING_NEXT,
    NOW_PLAYING_PREVIOUS,
    NOW_PLAYING_VOLUME_UP,
    NOW_PLAYING_VOLUME_DOWN,
    NOW_PLAYING_LEAVE,
    NOW_PLAYING_HOME,
    NOW_PLAYING_SCRUB_START,
    NOW_PLAYING_SCRUB_FORWARD,
    NOW_PLAYING_SCRUB_FORWARD_FAST,
    NOW_PLAYING_SCRUB_BACK,
    NOW_PLAYING_SCRUB_BACK_FAST,
    NOW_PLAYING_SCRUB_COMMIT,
    NOW_PLAYING_SCRUB_CANCEL,
    NOW_PLAYING_UP_NEXT,
    NOW_PLAYING_EQUALIZER,
    NOW_PLAYING_SEEK_FORWARD,
    NOW_PLAYING_SEEK_BACK
} NowPlayingCommand;

typedef struct NowPlayingControls {
    uint8_t rocker;
    bool menu_skips;
    bool volume_shown;
} NowPlayingControls;

enum {
    NOW_PLAYING_SCRUB_STEP_MS = 5000,
    NOW_PLAYING_SCRUB_FAST_STEP_MS = 15000,
    NOW_PLAYING_VOLUME_FEEDBACK_MS = 1500,
    NOW_PLAYING_VOLUME_HOLD_MS = 2500,
    NOW_PLAYING_BADGE_MS = 700
};

typedef struct NowPlayingConfig {
    Player *player;
    const Library *library;
    const Theme *theme;
    CoverCache *covers;
    Screen *up_next;
    Screen *equalizer;
    const uint8_t *options;
} NowPlayingConfig;

typedef struct NowPlayingLyrics {
    Lyrics lyrics;
    bool ready;
    int current;
    uint32_t current_ms;
} NowPlayingLyrics;

enum {
    NOW_PLAYING_LYRICS_CHUNK = 512,
    NOW_PLAYING_LYRICS_READ_LIMIT = 64 * 1024
};

typedef struct NowPlayingScreen {
    Screen screen;
    NowPlayingConfig config;
    PlayerStatus status;
    bool scrubbing;
    uint32_t scrub_position_ms;
    bool has_text;
    uint16_t text_track;
    uint16_t album_index;
    uint32_t text_changed_ms;
    uint32_t now_ms;
    NowPlayingFeedback feedback;
    uint32_t feedback_started_ms;
    char title[NOW_PLAYING_TEXT_CAPACITY];
    char artist[NOW_PLAYING_TEXT_CAPACITY];
    char album[NOW_PLAYING_TEXT_CAPACITY];
    NowPlayingLyrics lyrics;
} NowPlayingScreen;

NowPlayingCommand now_playing_command_for(KeyEvent event, bool scrubbing);
NowPlayingCommand now_playing_command_with(KeyEvent event, bool scrubbing,
                                           NowPlayingControls controls);
NowPlayingControls now_playing_controls_of(const uint8_t *options);
uint32_t now_playing_scrub_target(uint32_t position_ms, uint32_t duration_ms, int32_t delta_ms);
const char *now_playing_codec_name(HalAudioCodec codec);
void now_playing_stream_text(HalAudioFormat format, char *buffer, size_t capacity);
void now_playing_detail_text(const PlayerStatus *status, char *buffer, size_t capacity);
int now_playing_progress_width(uint32_t position_ms, uint32_t duration_ms, int width);
NowPlayingFeedback now_playing_feedback_for(NowPlayingCommand command, PlayerState after);
bool now_playing_feedback_visible(NowPlayingFeedback feedback, uint32_t started_ms,
                                  uint32_t now_ms);
int now_playing_volume_width(uint8_t volume, int width);
const char *now_playing_lyric(const NowPlayingScreen *now_playing);
const ThemeColors *now_playing_face_colors(const NowPlayingScreen *now_playing);
Screen *now_playing_screen_init(NowPlayingScreen *now_playing, NowPlayingConfig config);
