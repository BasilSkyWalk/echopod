#include "player.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/storage.h"
#include "playback/resume_state.h"

static const char RESUME_PATH[] = "/.echo/resume.bin";

static uint32_t current_position(const Player *player)
{
    return player->loaded ? hal_audio_status().position_ms : player->paused_position_ms;
}

static bool track_path(const Player *player, uint16_t track, char *buffer, size_t capacity)
{
    LibraryTrack record;
    char relative[PLAYER_PATH_CAPACITY];
    if (!library_read_track(player->library, track, &record) ||
        !library_read_string(player->library, record.path, relative, sizeof relative)) {
        return false;
    }
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append_char(&builder, '/');
    string_builder_append(&builder, relative);
    return !builder.overflowed;
}

static uint32_t new_token(Player *player)
{
    return ++player->token_counter;
}

static void become_idle(Player *player)
{
    player->state = PLAYER_IDLE;
    player->loaded = false;
    player->has_next = false;
    player->paused_position_ms = 0;
}

static void queue_upcoming(Player *player)
{
    char path[PLAYER_PATH_CAPACITY];
    PlayQueueCursor upcoming;
    uint16_t track = 0;
    player->has_next = play_queue_upcoming(&player->queue, player->repeat, &upcoming) &&
                       play_queue_track_at(&player->queue, upcoming, &track) &&
                       track_path(player, track, path, sizeof path);
    if (!player->has_next) {
        hal_audio_queue_next(NULL, 0);
        return;
    }
    player->next_cursor = upcoming;
    player->next_token = new_token(player);
    hal_audio_queue_next(path, player->next_token);
}

static bool start_current(Player *player, uint32_t start_ms)
{
    char path[PLAYER_PATH_CAPACITY];
    uint16_t track = 0;
    if (!play_queue_current(&player->queue, &track) ||
        !track_path(player, track, path, sizeof path)) {
        become_idle(player);
        return false;
    }
    player->track = track;
    player->has_track = true;
    player->current_token = new_token(player);
    player->loaded = hal_audio_play(path, start_ms, player->current_token);
    if (!player->loaded) {
        become_idle(player);
        return false;
    }
    player->state = PLAYER_PLAYING;
    queue_upcoming(player);
    return true;
}

static void restart_track(Player *player)
{
    if (player->loaded) {
        hal_audio_seek(0);
    } else {
        player->paused_position_ms = 0;
    }
}

static bool move_and_start(Player *player, int direction, bool wrap)
{
    PlayQueueCursor target;
    if (!play_queue_neighbor(&player->queue, direction, wrap, &target)) {
        return false;
    }
    play_queue_move_to(&player->queue, target);
    return start_current(player, 0);
}

void player_init(Player *player, const Library *library)
{
    player->library = library;
    player->fingerprint = library_fingerprint(library);
    player->queue.library = library;
    player->queue.count = 0;
    player->repeat = REPEAT_OFF;
    player->shuffle = SHUFFLE_OFF;
    player->volume = PLAYER_DEFAULT_VOLUME;
    player->has_track = false;
    player->track = 0;
    player->token_counter = 0;
    player->current_token = 0;
    player->last_save_ms = 0;
    become_idle(player);
    hal_audio_set_volume(player->volume);
}

bool player_play_list(Player *player, BrowseQuery source, uint32_t position, uint32_t seed)
{
    BrowseItem item;
    if (!browse_item(player->library, source, position, &item) || item.kind != BROWSE_ITEM_TRACK) {
        return false;
    }
    PlayQueuePlan plan = {
        .source = source, .anchor_track = item.index, .shuffle = player->shuffle, .seed = seed};
    if (!play_queue_build(&player->queue, player->library, plan) || !start_current(player, 0)) {
        return false;
    }
    player_save(player);
    return true;
}

void player_toggle_pause(Player *player)
{
    switch (player->state) {
    case PLAYER_PLAYING:
        hal_audio_pause();
        player->state = PLAYER_PAUSED;
        player_save(player);
        return;
    case PLAYER_PAUSED:
        if (player->loaded) {
            hal_audio_resume();
            player->state = PLAYER_PLAYING;
            return;
        }
        start_current(player, player->paused_position_ms);
        return;
    case PLAYER_IDLE:
        if (player->has_track) {
            start_current(player, 0);
        }
        return;
    }
}

void player_skip(Player *player, int direction)
{
    if (!player->has_track) {
        return;
    }
    if (direction < 0 && current_position(player) > PLAYER_RESTART_THRESHOLD_MS) {
        restart_track(player);
        return;
    }
    bool wrap = direction > 0 ? player->repeat != REPEAT_OFF : player->repeat == REPEAT_ALL;
    if (!move_and_start(player, direction, wrap) && direction < 0) {
        restart_track(player);
    }
}

void player_seek(Player *player, uint32_t position_ms)
{
    if (!player->has_track) {
        return;
    }
    if (player->loaded) {
        hal_audio_seek(position_ms);
    } else {
        player->paused_position_ms = position_ms;
    }
}

void player_change_volume(Player *player, int delta)
{
    int level = player->volume + delta;
    if (level < 0) {
        level = 0;
    }
    if (level > HAL_AUDIO_VOLUME_MAX) {
        level = HAL_AUDIO_VOLUME_MAX;
    }
    player->volume = (uint8_t)level;
    hal_audio_set_volume(player->volume);
}

void player_set_shuffle(Player *player, ShuffleMode shuffle, uint32_t seed)
{
    player->shuffle = shuffle;
    if (!player->has_track) {
        return;
    }
    PlayQueuePlan plan = player->queue.plan;
    plan.anchor_track = player->track;
    plan.shuffle = shuffle;
    plan.seed = seed;
    if (play_queue_build(&player->queue, player->library, plan) && player->loaded) {
        queue_upcoming(player);
    }
    player_save(player);
}

void player_set_repeat(Player *player, RepeatMode repeat)
{
    player->repeat = repeat;
    if (player->loaded) {
        queue_upcoming(player);
    }
}

static void advance_to_next(Player *player)
{
    play_queue_move_to(&player->queue, player->next_cursor);
    play_queue_current(&player->queue, &player->track);
    player->current_token = player->next_token;
    player->has_next = false;
    queue_upcoming(player);
    player_save(player);
}

static void finish_track(Player *player, bool failed)
{
    if (failed && move_and_start(player, 1, player->repeat != REPEAT_OFF)) {
        return;
    }
    become_idle(player);
    player_save(player);
}

static void save_periodically(Player *player, uint32_t now_ms)
{
    if (player->state == PLAYER_PLAYING &&
        now_ms - player->last_save_ms >= PLAYER_SAVE_INTERVAL_MS) {
        player_save(player);
        player->last_save_ms = now_ms;
    }
}

void player_update(Player *player, uint32_t now_ms)
{
    if (!player->loaded) {
        return;
    }
    HalAudioStatus status = hal_audio_status();
    if (player->has_next && status.token == player->next_token) {
        advance_to_next(player);
    } else if (status.state == HAL_AUDIO_STOPPED && status.token == player->current_token) {
        finish_track(player, status.failed);
    }
    save_periodically(player, now_ms);
}

bool player_save(Player *player)
{
    if (!player->has_track) {
        return false;
    }
    ResumeState state = {
        .library_fingerprint = player->fingerprint,
        .plan = player->queue.plan,
        .repeat = player->repeat,
        .volume = player->volume,
        .cursor = player->queue.cursor,
        .track = player->track,
        .position_ms = current_position(player),
    };
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&state, bytes);
    return hal_file_write(RESUME_PATH, bytes, sizeof bytes);
}

static bool read_resume_state(ResumeState *state)
{
    HalFile file;
    uint8_t bytes[RESUME_STATE_SIZE];
    if (!hal_file_open(&file, RESUME_PATH)) {
        return false;
    }
    bool read = file.size >= RESUME_STATE_SIZE && hal_file_read(file, 0, bytes, sizeof bytes);
    hal_file_close(&file);
    return read && resume_state_decode(bytes, sizeof bytes, state);
}

static bool rebuild_queue(Player *player, const ResumeState *state)
{
    uint16_t track = 0;
    if (!play_queue_build(&player->queue, player->library, state->plan)) {
        return false;
    }
    play_queue_move_to(&player->queue, state->cursor);
    return play_queue_current(&player->queue, &track) && track == state->track;
}

bool player_restore(Player *player)
{
    ResumeState state;
    if (!read_resume_state(&state) || state.library_fingerprint != player->fingerprint ||
        !rebuild_queue(player, &state)) {
        return false;
    }
    player->track = state.track;
    player->has_track = true;
    player->repeat = state.repeat;
    player->shuffle = state.plan.shuffle;
    player->state = PLAYER_PAUSED;
    player->paused_position_ms = state.position_ms;
    player_change_volume(player, (int)state.volume - (int)player->volume);
    return true;
}

static HalAudioFormat current_format(const Player *player)
{
    HalAudioFormat unknown = {.codec = HAL_AUDIO_CODEC_UNKNOWN};
    if (!player->loaded) {
        return unknown;
    }
    HalAudioStatus status = hal_audio_status();
    return status.token == player->current_token ? status.format : unknown;
}

PlayerStatus player_status(const Player *player)
{
    LibraryTrack record;
    bool known = player->has_track && library_read_track(player->library, player->track, &record);
    return (PlayerStatus){
        .state = player->state,
        .has_track = player->has_track,
        .track = player->track,
        .position_ms = current_position(player),
        .duration_ms = known ? record.duration_ms : 0,
        .volume = player->volume,
        .shuffle = player->shuffle,
        .repeat = player->repeat,
        .place = player->has_track ? play_queue_place(&player->queue) : (PlayQueuePlace){0, 0},
        .format = current_format(player),
    };
}
