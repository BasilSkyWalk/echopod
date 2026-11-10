#include "core/resident.h"
#include "faces.h"

#define CHROME(top, bottom, line, text, off, backdrop)                                             \
    {.background = backdrop,                                                                       \
     .header_top = top,                                                                            \
     .header_background = bottom,                                                                  \
     .divider = line,                                                                              \
     .header_text = text,                                                                          \
     .text_tertiary = off,                                                                         \
     .success = COLOR_RGB(120, 200, 96),                                                           \
     .danger = COLOR_RGB(248, 96, 72)}

ECHO_NP_FACES_DATA static const ThemeColors CASSETTE_CHROME =
    CHROME(COLOR_RGB(72, 68, 72), COLOR_RGB(24, 20, 24), COLOR_RGB(96, 96, 104),
           COLOR_RGB(240, 228, 200), COLOR_RGB(184, 168, 136), COLOR_RGB(24, 20, 24));
ECHO_NP_FACES_DATA static const ThemeColors VU_CHROME =
    CHROME(COLOR_RGB(56, 52, 48), COLOR_RGB(24, 20, 16), COLOR_RGB(72, 64, 56),
           COLOR_RGB(240, 232, 216), COLOR_RGB(168, 152, 128), COLOR_RGB(32, 28, 24));
ECHO_NP_FACES_DATA static const ThemeColors TURNTABLE_CHROME =
    CHROME(COLOR_RGB(40, 20, 16), COLOR_RGB(24, 12, 16), COLOR_RGB(104, 104, 96),
           COLOR_RGB(248, 236, 208), COLOR_RGB(192, 160, 120), COLOR_RGB(72, 44, 24));
ECHO_NP_FACES_DATA static const ThemeColors LCD_CHROME =
    CHROME(COLOR_RGB(64, 64, 72), COLOR_RGB(24, 24, 32), COLOR_RGB(72, 72, 80),
           COLOR_RGB(176, 196, 104), COLOR_RGB(144, 168, 80), COLOR_RGB(32, 32, 40));

ECHO_NP_FACES_CODE static void draw(const NowPlayingFrame *frame, Canvas *canvas)
{
    switch (frame->face) {
    case NP_FACE_CASSETTE:
        face_cassette_draw(frame, canvas);
        break;
    case NP_FACE_VU_METERS:
        face_vu_draw(frame, canvas);
        break;
    case NP_FACE_TURNTABLE:
        face_turntable_draw(frame, canvas);
        break;
    case NP_FACE_LCD:
        face_lcd_draw(frame, canvas);
        break;
    default:
        break;
    }
}

ECHO_NP_FACES_ENTRY const NowPlayingFaces NOW_PLAYING_FACES = {
    .magic = NP_FACES_MAGIC,
    .draw = draw,
    .chrome = {[NP_FACE_CASSETTE] = &CASSETTE_CHROME,
               [NP_FACE_VU_METERS] = &VU_CHROME,
               [NP_FACE_TURNTABLE] = &TURNTABLE_CHROME,
               [NP_FACE_LCD] = &LCD_CHROME},
};
