#pragma once

#include "np_faces/now_playing_face.h"

void face_cassette_draw(const NowPlayingFrame *frame, Canvas *canvas);
void face_vu_draw(const NowPlayingFrame *frame, Canvas *canvas);
void face_turntable_draw(const NowPlayingFrame *frame, Canvas *canvas);
void face_lcd_draw(const NowPlayingFrame *frame, Canvas *canvas);
void face_minimal_draw(const NowPlayingFrame *frame, Canvas *canvas);
