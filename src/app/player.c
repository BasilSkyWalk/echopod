#include "player.h"

#include <stddef.h>

#include "core/format.h"
#include "core/string_builder.h"
#include "hal/storage.h"
#include "playback/resume_state.h"
#include "playback/spoken.h"

const char PLAYER_RESUME_PATH[] = "/.echo/resume.bin";
const char PLAYER_UP_NEXT_PATH[] = "/.echo/upnext.bin";

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

static void save_up_next(const Player *player)
{
    uint8_t bytes[UP_NEXT_STATE_SIZE];
    up_next_encode(&player->up_next, player->fingerprint, bytes);
    (void)hal_file_write(PLAYER_UP_NEXT_PATH, bytes, sizeof bytes);
}

static bool current_track(const Player *player, uint16_t *track)
{
    if (player->from_up_next) {
        *track = player->track;
        return true;
    }
    return play_queue_current(&player->queue, track);
}

static bool upcoming_track(Player *player, PlayQueueCursor *cursor, uint16_t *track)
{
    if (player->repeat == REPEAT_ONE && player->from_up_next) {
        player->next_source = PLAYER_NEXT_SAME_SONG;
        *track = player->track;
        return true;
    }
    if (player->repeat != REPEAT_ONE && up_next_first(&player->up_next, track)) {
        player->next_source = PLAYER_NEXT_FROM_UP_NEXT;
        return true;
    }
    player->next_source = PLAYER_NEXT_FROM_QUEUE;
    return play_queue_upcoming(&player->queue, player->repeat, cursor) &&
           play_queue_track_at(&player->queue, *cursor, track);
}

static void queue_upcoming(Player *player)
{
    char path[PLAYER_PATH_CAPACITY];
    PlayQueueCursor upcoming = player->queue.cursor;
    uint16_t track = 0;
    player->has_next =
        upcoming_track(player, &upcoming, &track) && track_path(player, track, path, sizeof path);
    if (!player->has_next) {
        hal_audio_queue_next(NULL, 0);
        return;
    }
    player->next_cursor = upcoming;
    player->next_token = new_token(player);
    hal_audio_queue_next(path, player->next_token);
}

static void note_spoken(Player *player, uint16_t track)
{
    char path[PLAYER_PATH_CAPACITY];
    player->spoken =
        track_path(player, track, path, sizeof path) && spoken_kind_of(path) != SPOKEN_NONE;
}

static uint32_t resume_point(const Player *player, uint16_t track, uint32_t start_ms)
{
    if (start_ms != 0 || !player->spoken || player->resume.position_for == NULL) {
        return start_ms;
    }
    return player->resume.position_for(player->resume.context, track);
}

static bool start_current(Player *player, uint32_t start_ms)
{
    char path[PLAYER_PATH_CAPACITY];
    uint16_t track = 0;
    if (!current_track(player, &track) || !track_path(player, track, path, sizeof path)) {
        become_idle(player);
        return false;
    }
    player->spoken = spoken_kind_of(path) != SPOKEN_NONE;
    start_ms = resume_point(player, track, start_ms);
    player->track = track;
    player->has_track = true;
    player->current_token = new_token(player);
    player->loaded = hal_audio_play(path, start_ms, player->current_token);
    if (!player->loaded) {
        player->refusal = hal_audio_refusal();
        player->refusal_count++;
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

static bool start_playable(Player *player, uint32_t start_ms, int direction, bool wrap)
{
    if (start_current(player, start_ms)) {
        return true;
    }
    for (uint16_t tries = 1; tries < player->queue.count; tries++) {
        PlayQueueCursor target;
        if (!play_queue_neighbor(&player->queue, direction, wrap, &target)) {
            break;
        }
        play_queue_move_to(&player->queue, target);
        if (start_current(player, 0)) {
            return true;
        }
    }
    become_idle(player);
    return false;
}

static bool start_up_next(Player *player)
{
    uint16_t track = 0;
    bool started = false;
    while (!started && up_next_first(&player->up_next, &track)) {
        up_next_drop(&player->up_next, 1);
        player->from_up_next = true;
        player->track = track;
        started = start_current(player, 0);
    }
    if (!started) {
        player->from_up_next = false;
    }
    save_up_next(player);
    return started;
}

static bool move_and_start(Player *player, int direction, bool wrap)
{
    if (direction > 0 && player->up_next.count > 0 && start_up_next(player)) {
        return true;
    }
    if (direction < 0 && player->from_up_next) {
        player->from_up_next = false;
        return start_playable(player, 0, direction, wrap);
    }
    PlayQueueCursor target;
    if (!play_queue_neighbor(&player->queue, direction, wrap, &target)) {
        return false;
    }
    player->from_up_next = false;
    play_queue_move_to(&player->queue, target);
    return start_playable(player, 0, direction, wrap);
}

void player_init(Player *player, const Library *library)
{
    player->library = library;
    player->fingerprint = library_fingerprint(library);
    player->queue.library = library;
    player->queue.count = 0;
    up_next_clear(&player->up_next);
    player->from_up_next = false;
    player->next_source = PLAYER_NEXT_FROM_QUEUE;
    player->repeat = REPEAT_OFF;
    player->shuffle = SHUFFLE_OFF;
    player->volume = PLAYER_DEFAULT_VOLUME;
    player->has_track = false;
    player->track = 0;
    player->token_counter = 0;
    player->current_token = 0;
    player->last_save_ms = 0;
    player->jack = hal_audio_jack();
    player->refusal_count = 0;
    player->refusal = (HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_NONE};
    player->spoken = false;
    player->resume = (PlayerResume){.position_for = NULL, .context = NULL};
    become_idle(player);
    hal_audio_set_volume(player->volume);
}

void player_set_resume(Player *player, PlayerResume resume)
{
    player->resume = resume;
}

bool player_play_list(Player *player, BrowseQuery source, uint32_t position, uint32_t seed)
{
    PlayerListStart start = {
        .source = source, .position = position, .seed = seed, .in_order = false};
    return player_play_from(player, start);
}

bool player_play_from(Player *player, PlayerListStart start)
{
    BrowseItem item;
    if (!browse_item(player->library, start.source, start.position, &item) ||
        item.kind != BROWSE_ITEM_TRACK) {
        return false;
    }
    player->from_up_next = false;
    PlayQueuePlan plan = {.source = start.source,
                          .anchor_track = item.index,
                          .shuffle = start.in_order ? SHUFFLE_OFF : player->shuffle,
                          .seed = start.seed};
    if (!play_queue_build(&player->queue, player->library, plan) ||
        !start_playable(player, 0, 1, player->repeat == REPEAT_ALL)) {
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

static uint32_t track_duration(const Player *player)
{
    LibraryTrack record;
    return library_read_track(player->library, player->track, &record) ? record.duration_ms : 0;
}

void player_jump(Player *player, int32_t delta_ms)
{
    if (!player->has_track) {
        return;
    }
    uint32_t position = current_position(player);
    uint32_t duration = track_duration(player);
    if (delta_ms < 0) {
        uint32_t back = (uint32_t)-delta_ms;
        player_seek(player, position > back ? position - back : 0);
        return;
    }
    uint32_t target = position + (uint32_t)delta_ms;
    if (duration > 0 && target >= duration) {
        move_and_start(player, 1, player->repeat != REPEAT_OFF);
        return;
    }
    player_seek(player, target);
}

void player_skip(Player *player, int direction)
{
    if (!player->has_track) {
        return;
    }
    if (player->spoken) {
        player_jump(player, direction > 0 ? SPOKEN_JUMP_FORWARD_MS : -SPOKEN_JUMP_BACK_MS);
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
    if (!play_queue_current(&player->queue, &plan.anchor_track)) {
        plan.anchor_track = player->track;
    }
    plan.shuffle = player->spoken ? SHUFFLE_OFF : shuffle;
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

static void take_next(Player *player)
{
    switch (player->next_source) {
    case PLAYER_NEXT_FROM_UP_NEXT:
        up_next_first(&player->up_next, &player->track);
        up_next_drop(&player->up_next, 1);
        player->from_up_next = true;
        save_up_next(player);
        return;
    case PLAYER_NEXT_SAME_SONG:
        return;
    case PLAYER_NEXT_FROM_QUEUE:
        break;
    }
    play_queue_move_to(&player->queue, player->next_cursor);
    play_queue_current(&player->queue, &player->track);
    player->from_up_next = false;
}

static void take_next_and_note(Player *player)
{
    take_next(player);
    note_spoken(player, player->track);
}

static void advance_to_next(Player *player)
{
    take_next_and_note(player);
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

static PlayerUpNextResult enqueue(Player *player, uint16_t track, bool next)
{
    if (!player->has_track) {
        player->from_up_next = true;
        player->track = track;
        return start_current(player, 0) ? PLAYER_UP_NEXT_STARTED : PLAYER_UP_NEXT_FAILED;
    }
    bool added =
        next ? up_next_play_next(&player->up_next, track) : up_next_add(&player->up_next, track);
    if (!added) {
        return PLAYER_UP_NEXT_FULL;
    }
    save_up_next(player);
    if (player->loaded) {
        queue_upcoming(player);
    }
    return PLAYER_UP_NEXT_ADDED;
}

PlayerUpNextResult player_play_next(Player *player, uint16_t track)
{
    return enqueue(player, track, true);
}

PlayerUpNextResult player_add_to_up_next(Player *player, uint16_t track)
{
    return enqueue(player, track, false);
}

bool player_jump_to_up_next(Player *player, uint16_t index)
{
    if (index >= player->up_next.count) {
        return false;
    }
    up_next_drop(&player->up_next, index);
    bool started = start_up_next(player);
    player_save(player);
    return started;
}

bool player_jump_to_queue(Player *player, PlayQueueCursor cursor)
{
    up_next_clear(&player->up_next);
    save_up_next(player);
    player->from_up_next = false;
    play_queue_move_to(&player->queue, cursor);
    bool started = start_playable(player, 0, 1, player->repeat == REPEAT_ALL);
    player_save(player);
    return started;
}

void player_release_output(Player *player)
{
    if (!player->loaded) {
        return;
    }
    player->paused_position_ms = hal_audio_status().position_ms;
    player_save(player);
    player->loaded = false;
    player->has_next = false;
    if (player->state == PLAYER_PLAYING) {
        player->state = PLAYER_PAUSED;
    }
}

static void save_periodically(Player *player, uint32_t now_ms)
{
    if (player->state == PLAYER_PLAYING &&
        now_ms - player->last_save_ms >= PLAYER_SAVE_INTERVAL_MS) {
        player_save(player);
        player->last_save_ms = now_ms;
    }
}

static void follow_jack(Player *player)
{
    HalAudioJack jack = hal_audio_jack();
    bool unplugged = player->jack != HAL_AUDIO_JACK_NONE && jack == HAL_AUDIO_JACK_NONE;
    player->jack = jack;
    if (unplugged && player->state == PLAYER_PLAYING) {
        player_toggle_pause(player);
    }
}

void player_update(Player *player, uint32_t now_ms)
{
    follow_jack(player);
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
        .from_up_next = player->from_up_next,
    };
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&state, bytes);
    return hal_file_write(PLAYER_RESUME_PATH, bytes, sizeof bytes);
}

static bool read_resume_state(ResumeState *state)
{
    HalFile file;
    uint8_t bytes[RESUME_STATE_SIZE];
    if (!hal_file_open(&file, PLAYER_RESUME_PATH)) {
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
    return play_queue_current(&player->queue, &track) &&
           (state->from_up_next || track == state->track);
}

static void load_up_next(Player *player)
{
    HalFile file;
    uint8_t bytes[UP_NEXT_STATE_SIZE];
    if (!hal_file_open(&file, PLAYER_UP_NEXT_PATH)) {
        return;
    }
    bool read = file.size >= UP_NEXT_STATE_SIZE && hal_file_read(file, 0, bytes, sizeof bytes);
    hal_file_close(&file);
    if (!read || !up_next_decode(bytes, sizeof bytes, player->fingerprint, &player->up_next)) {
        up_next_clear(&player->up_next);
    }
}

bool player_restore(Player *player)
{
    ResumeState state;
    load_up_next(player);
    if (!read_resume_state(&state) || state.library_fingerprint != player->fingerprint ||
        !rebuild_queue(player, &state)) {
        return false;
    }
    player->track = state.track;
    player->from_up_next = state.from_up_next;
    player->has_track = true;
    player->repeat = state.repeat;
    player->shuffle = state.plan.shuffle;
    player->state = PLAYER_PAUSED;
    player->paused_position_ms = state.position_ms;
    note_spoken(player, player->track);
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
        .spoken = player->has_track && player->spoken,
    };
}

void player_refusal_text(HalAudioRefusal refusal, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    switch (refusal.problem) {
    case HAL_AUDIO_PROBLEM_RATE: {
        char rate[16];
        format_sample_rate(refusal.sample_rate, rate, sizeof rate);
        string_builder_append(&builder, rate);
        string_builder_append(&builder, " isn't supported yet");
        return;
    }
    case HAL_AUDIO_PROBLEM_FORMAT:
        string_builder_append(&builder, "This file type isn't supported yet");
        return;
    case HAL_AUDIO_PROBLEM_UNREADABLE:
    case HAL_AUDIO_PROBLEM_NONE:
        break;
    }
    string_builder_append(&builder, "Couldn't read the track");
}
