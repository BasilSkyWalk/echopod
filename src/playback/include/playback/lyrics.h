#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    LYRICS_TEXT_CAPACITY = 3072,
    LYRICS_LINE_CAPACITY = 160,
    LYRICS_PENDING_CAPACITY = 240,
    LYRICS_TIMES_PER_LINE = 16,
    LYRICS_NONE = -1
};

typedef struct LyricsLine {
    uint32_t time_ms;
    uint16_t text;
    bool timed;
} LyricsLine;

typedef struct Lyrics {
    LyricsLine lines[LYRICS_LINE_CAPACITY];
    char text[LYRICS_TEXT_CAPACITY];
    char pending[LYRICS_PENDING_CAPACITY];
    uint16_t count;
    uint16_t text_used;
    uint16_t pending_length;
    int32_t offset_ms;
    bool synced;
    bool cut;
    bool first_line;
} Lyrics;

void lyrics_start(Lyrics *lyrics);
void lyrics_feed(Lyrics *lyrics, const char *bytes, uint32_t length);
void lyrics_finish(Lyrics *lyrics);
bool lyrics_full(const Lyrics *lyrics);
int lyrics_current(const Lyrics *lyrics, uint32_t position_ms, uint32_t duration_ms);
const char *lyrics_line(const Lyrics *lyrics, int index);
