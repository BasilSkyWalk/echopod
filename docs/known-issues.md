# EchoPod known issues

EchoPod is early firmware made by one person and tested on one player. It comes with no warranty: use it at your own risk. Read this before you install, and keep a way back (see "If something goes wrong" in `docs/user-guide.md`).

## Safety

- **Volume:** EchoPod's top volume is close to FiiO's maximum, and sensitive earphones can be very loud. Turn the volume down before you put them in.
- **Battery:** EchoPod switches off when the battery runs low, but doesn't yet refuse to start on an almost empty battery. If it switches off by itself, charge it before using it again.
- **Bricking:** custom firmware has left Echo Mini players unable to start during development (all were recovered through maskrom). EchoPod's updater writes one firmware slot at a time and reads every chunk back, but a power cut during an install is still a risk: install with the battery charged.

## Installing

- **Tested on one player.** One Echo Mini 8 GB, starting from FiiO firmware 3.7.0, 3.9.0 and 4.0.0, installed from Windows 11. Other units, other FiiO versions and other computers haven't been tried.
- **Installing from a Mac is unproven.** Earlier attempts to copy an image from a Mac with FiiO's firmware running were refused (FiiO's firmware showed "Updating" for under a second and kept itself). The cause isn't known. Install from Windows if you can.
- **The 512 MB Echo Mini isn't supported.** EchoPod refuses its firmware, and its firmware ignores EchoPod's file.
- **Going back to FiiO's firmware** is checked with FiiO's 8 GB 3.7.0, 3.8.0, 3.9.0 and 4.0.0 images, and 0.3.0 installed FiiO 4.0.0 on the player and was installed back from it (EchoPod 0.2.0 refuses 4.0.0, see `docs/user-guide.md`). Another custom firmware can't be installed from EchoPod: go back to FiiO's first.
- **If the player doesn't start at all,** the only recovery known is the chip's maskrom mode (`docs/device-recovery.md`). It works, but it needs a patched tool and care.

## USB

- **The first connection to a new computer can fail.** Windows may report "Device Descriptor Request Failed", or the drives may drop off. Unplug and plug in again; the second try has always worked.
- **Eject both drives before you unplug.** Unplugging first can lose the last changes, and the player warns you ("Unplugged before ejecting"). Once, a file deleted from Windows and ejected properly was still on internal storage afterwards; it hasn't happened again.
- **The SD card can be marked as needing repair.** Twice a computer reported the card's exFAT volume as not cleanly closed. Nothing was lost. If Windows offers to scan and fix the card, let it (or run `chkdsk /f` on its drive letter; on a Mac, Disk Utility's First Aid).
- **Speed on a Mac hasn't been measured.** On Windows 11, copying runs at about 3.7 MB/s to the SD card and 5 MB/s to internal storage, and reading from the SD card at about 3.9 MB/s.
- **USB DAC mode is tested on one player with Windows 11** (44.1 and 48 kHz, 16 and 24-bit) **and an Android phone.** Linux and a Mac haven't been tried. If it doesn't appear as a sound card, crackles, or plays at the wrong speed, set Settings → USB mode back to Storage and report what you saw. Known limits: full speed only (44.1 or 48 kHz, 16 or 24-bit; 88.2 and 96 kHz don't fit this chip's USB buffer); about 20 ms of delay; the player stays in DAC mode until you unplug it or hold Back; Power doesn't turn the screen off during DAC mode. If the computer stops sending audio and starts again, the first 20 ms are silent.
- **macOS leaves `._` files** beside everything it copies. EchoPod ignores them, but they take space. `dot_clean` on the drive removes them.

## Music and library

- **Gapless MP3** needs the LAME tag that LAME and ffmpeg write. MP3s without one (some iTunes and older encoders) keep a short gap between tracks. After a seek, or when playback resumes part-way through a track, that track's silent end isn't trimmed, so the next track starts a few tens of milliseconds late.
- **Characters:** song, artist and album names show Latin letters with accents, Korean (the 2,350 common Hangul syllables), Simplified Chinese (GB 2312), Japanese kana and kanji (JIS X 0208), and CJK punctuation. Rarer Hangul syllables, Traditional-only Chinese characters, emoji and other scripts (Arabic, Hebrew, Thai, Cyrillic, Greek…) show as `?`. Chinese characters shared with Japanese are drawn in their Simplified Chinese style. Tested on the player with Korean, Chinese, Japanese and accented titles; the first time a screen shows a character, it's read from internal storage.
- **Formats:** FLAC, ALAC, AAC, Opus, MP3 and WAV play, at 44.1, 48, 88.2, 96, 176.4 and 192 kHz. APE and the other formats FiiO lists appear in the library but don't play; EchoPod says so. ALAC, AAC and Opus play cleanly on the player tested; like MP3, they are refused after an update until the player restarts, like MP3. ALAC files with more than two channels or 32-bit samples don't play. Only AAC-LC plays: HE-AAC and HE-AAC v2 files that say so in their header are refused, and ones that don't play their core layer, which sounds duller. AAC with more than two channels doesn't play. Opus (`.opus`) plays mono and stereo files; files with more channels, packets longer than 60 ms (rare: encoders default to 20 ms) and Ogg files with Vorbis audio don't play. The volume adjustment some tools store in an Opus header is ignored, and a file made of several chained streams stops at the end of the first. AAC and Opus run core 1 at 200 MHz, twice the clock the other formats use, which may cost some battery (not measured yet); if one of them stutters, the `c` figure on Diagnostics → Frame time shows how busy core 1 is. Right after an Opus seek the first tenth of a second differs very slightly from playing straight through, as Opus decoders settle.
- **Building the library takes time.** On the first start, after music changes over USB, and once after updating to 0.2.0 (the library format changed). A 1,563-track, 144-album card takes about 1 minute 45 seconds: 20 s for the tracks and 1.4 minutes for the covers, where decoding each album's picture takes about half a second. Later rescans keep the covers whose art didn't change and decode only new or changed albums; the first rescan after updating still decodes them all, and so does one after an echo-sync from before this change. Covers from a cover file on a card that isn't FAT32 or exFAT are always decoded again. The player is busy until it finishes: let it finish before you plug it in. echo-sync can build the library on a computer instead.
- **Library size:** tested on the player with 6,558 tracks, and with over 1,600 albums; on a computer with 20,000 tracks. Larger libraries may stop with an error when the library file (4 MB) is full.
- **Album art:** the covers file has room for 2,673 albums. Past that, the remaining albums show without art.
- **Cover Flow** orders albums by album artist, then year, not by album title like the Albums list.
- **Spoken word is decided by the top folder.** Only tracks inside an `Audiobooks`, `Audiobook`, `Podcasts` or `Podcast` folder at the top of the card count as audiobooks or podcasts; the same folder names deeper down are music. An author with music too appears under Artists with the music only.

## echo-sync

- **Tested on Windows 11 only.** The macOS (Apple silicon) and Linux files pass an automatic check on GitHub but haven't been run on a real card yet. On a Mac, the downloaded file needs `chmod +x` and its quarantine flag removed before it runs (see `docs/user-guide.md`).
- **FAT32 and exFAT only.** On other file systems the file times may not match the player's, and the player then rescans anyway.
- **Copies that never finished** (a file whose size on the card is larger than what was written) are read as the player sees them; recopy such files from the computer.

## Not there yet

- No EQ, ReplayGain or lyrics.
- Bluetooth has been tested with one pair of headphones (OnePlus Buds 3). While music goes to Bluetooth the player can't read the headphone jack: pause, plug wired headphones in and press Play to switch.
- The 4.4 mm balanced output works (reported by a tester). Settings → Output can't switch sound between the jacks: both are driven by the same DACs, so it only picks the volume table (see `docs/user-guide.md`).
- Audiobooks and podcasts: the shelves, covers and keeping them out of Music are checked on a player; bookmarks, in-order play and the 30 s / 15 s jumps so far only in the simulator. There's no playback speed control.
- The brightness, Favorites stutter, sound-after-USB and battery-while-charging fixes since 0.1.0 still need a tester's confirmation on a player.
