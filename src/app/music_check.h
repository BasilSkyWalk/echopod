#pragma once

#include <stdbool.h>

#include "library/music_fingerprint.h"

extern const char MUSIC_CHECK_PATH[];

MusicFingerprint music_check_fingerprint(void);
bool music_check_unchanged(void);
void music_check_remember(MusicFingerprint print);
bool music_check_firmware_is_new(void);
void music_check_note_firmware(void);
