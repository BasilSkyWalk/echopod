# EchoPod known issues

EchoPod is early firmware made by one person and tested on one player. It comes with no warranty: use it at your own risk. Read this before you install, and keep a way back (see "If something goes wrong" in `docs/user-guide.md`).

## Safety

- **Volume:** EchoPod's top volume is close to FiiO's maximum, and sensitive earphones can be very loud. Turn the volume down before you put them in.
- **Battery:** EchoPod switches off when the battery runs low, but doesn't yet refuse to start on an almost empty battery. If it switches off by itself, charge it before using it again.
- **Bricking:** custom firmware has left Echo Mini players unable to start during development (all were recovered through maskrom). EchoPod's updater writes one firmware slot at a time and reads every chunk back, but a power cut during an install is still a risk: install with the battery charged.

## Installing

- **Tested on one player.** One Echo Mini 8 GB, starting from FiiO firmware 3.7.0 and 3.9.0, installed from Windows 11. Other units, other FiiO versions and other computers haven't been tried.
- **Installing from a Mac is unproven.** Earlier attempts to copy an image from a Mac with FiiO's firmware running were refused (FiiO's firmware showed "Updating" for under a second and kept itself). The cause isn't known. Install from Windows if you can.
- **The 512 MB Echo Mini isn't supported.** EchoPod refuses its firmware, and its firmware ignores EchoPod's file.
- **If the player doesn't start at all,** the only recovery known is the chip's maskrom mode (`docs/device-recovery.md`). It works, but it needs a patched tool and care.

## USB

- **The first connection to a new computer can fail.** Windows may report "Device Descriptor Request Failed", or the drives may drop off. Unplug and plug in again; the second try has always worked.
- **Eject both drives before you unplug.** Unplugging first can lose the last changes, and the player warns you ("Unplugged before ejecting"). Once, a file deleted from Windows and ejected properly was still on internal storage afterwards; it hasn't happened again.
- **The SD card can be marked as needing repair.** Twice a computer reported the card's exFAT volume as not cleanly closed. Nothing was lost. If Windows offers to scan and fix the card, let it (or run `chkdsk /f` on its drive letter; on a Mac, Disk Utility's First Aid).
- **Speed on a Mac hasn't been measured.** On Windows 11, copying runs at about 3.7 MB/s to the SD card and 5 MB/s to internal storage.
- **USB DAC mode hasn't been tested on a player yet.** It was written and checked on a computer only. If it doesn't appear as a sound card, crackles, or plays at the wrong speed, set Settings → USB mode back to Storage and report what you saw. Known limits: full speed only (up to 96 kHz, 16 or 24-bit); about 20 ms of delay; the player stays in DAC mode until you unplug it or hold Back; the volume keys change the volume one step per press and the screen doesn't show it; Power doesn't turn the screen off during DAC mode. If the computer stops sending audio and starts again, the first 20 ms are silent.
- **macOS leaves `._` files** beside everything it copies. EchoPod ignores them, but they take space. `dot_clean` on the drive removes them.

## Music and library

- **Formats:** FLAC, MP3 and WAV play, at 44.1, 48, 88.2, 96, 176.4 and 192 kHz. ALAC, APE and the other formats FiiO lists appear in the library but don't play; EchoPod says so.
- **Building the library takes time.** About 2 minutes per 1,400 tracks on the first start, and again after music changes over USB. The player is busy until it finishes.
- **Library size:** tested on the player with 6,558 tracks, and with over 1,600 albums; on a computer with 20,000 tracks. Larger libraries may stop with an error when the library file (4 MB) is full.
- **Album art:** the covers file has room for 2,674 albums. Past that, the remaining albums show without art.
- **Cover Flow** orders albums by album artist, then year, not by album title like the Albums list.

## Not there yet

- No Bluetooth audio, EQ, ReplayGain or lyrics.
- The 4.4 mm balanced output works (reported by a tester). Settings → Output can't switch sound between the jacks: both are driven by the same DACs, so it only picks the volume table (see `docs/user-guide.md`).
- Audiobooks and podcasts (bookmarks, in-order play, 30 s / 15 s jumps) are new and have only been tested in the simulator. There's no playback speed control.
- The brightness, Favorites stutter, sound-after-USB and battery-while-charging fixes since 0.1.0 are tested on a computer, not yet on a player.
