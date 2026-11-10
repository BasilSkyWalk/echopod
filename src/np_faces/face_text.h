#pragma once

#include <stddef.h>

#include "gfx/font.h"
#include "np_faces/now_playing_face.h"
#include "ui/marquee.h"

enum {
    FACE_TIME_CAPACITY = 12,
    FACE_FEEDBACK_CAPACITY = 48,
    FACE_CODEC_PADDING = 8,
    FACE_FORMAT_GAP = 7
};

typedef enum FaceAlign {
    FACE_ALIGN_LEFT,
    FACE_ALIGN_CENTRE,
    FACE_ALIGN_RIGHT
} FaceAlign;

typedef enum FaceIcon {
    FACE_ICON_NONE,
    FACE_ICON_PLAY,
    FACE_ICON_PAUSE,
    FACE_ICON_NEXT,
    FACE_ICON_PREVIOUS
} FaceIcon;

typedef struct FaceLine {
    const Font *font;
    Rect area;
    Color emphasis;
    Color color;
    FaceAlign align;
} FaceLine;

typedef struct FaceTimes {
    char elapsed[FACE_TIME_CAPACITY];
    char remaining[FACE_TIME_CAPACITY];
} FaceTimes;

typedef struct FaceFill {
    int width;
    bool hot;
} FaceFill;

typedef struct FaceBar {
    Rect area;
    Color track;
    Color normal;
    Color hot;
} FaceBar;

int face_scroll_offset(MarqueeTiming timing, int text_width, int view_width, uint32_t elapsed_ms);
Rect face_fitted_area(const Font *font, Rect area, const char *text, FaceAlign align);
void face_line(Canvas *canvas, const NowPlayingFrame *frame, const FaceLine *line,
               const char *text);
const char *face_feedback_text(const NowPlayingFrame *frame, char *buffer, size_t capacity);
void face_feedback_line(Canvas *canvas, const NowPlayingFrame *frame, const FaceLine *line,
                        const char *text);
void face_times_text(const NowPlayingFrame *frame, FaceTimes *times);
void face_times_draw(Canvas *canvas, const NowPlayingFrame *frame, const FaceLine *line);
int face_progress_width(uint32_t position_ms, uint32_t duration_ms, int width);
int face_volume_width(uint8_t volume, int width);
FaceFill face_bar_fill(const NowPlayingFrame *frame, int width);
void face_bar_draw(Canvas *canvas, const NowPlayingFrame *frame, const FaceBar *bar);
FaceIcon face_badge_kind(const NowPlayingFrame *frame);
void face_icon_draw(Canvas *canvas, FaceIcon icon, Rect box, Color color);
void face_badge_draw(Canvas *canvas, const NowPlayingFrame *frame, Rect box, Color color);
void face_outline(Canvas *canvas, Rect box, Color color);
int face_codec_width(const Font *font, const char *codec);
void face_codec_draw(Canvas *canvas, const FaceLine *line, const char *codec);
void face_format_row(Canvas *canvas, const NowPlayingFrame *frame, const FaceLine *line);
