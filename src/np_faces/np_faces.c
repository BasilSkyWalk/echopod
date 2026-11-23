#include "core/resident.h"
#include "np_faces/scene.h"

ECHO_NP_FACES_ENTRY const NowPlayingFaces NOW_PLAYING_FACES = {
    .magic = NP_FACES_MAGIC,
    .draw = scene_draw,
};
