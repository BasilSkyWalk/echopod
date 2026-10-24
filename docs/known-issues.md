# EchoPod known issues

EchoPod is early firmware made by one person and tested on one player. Read this before you install, and keep a way back (see "If something goes wrong" in `docs/user-guide.md`).

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
- **macOS leaves `._` files** beside everything it copies. EchoPod ignores them, but they take space. `dot_clean` on the drive removes them.

## Music and library

- **Formats:** FLAC, MP3 and WAV play, up to 192 kHz. ALAC, APE and the other formats FiiO lists appear in the library but don't play; EchoPod says so. 176.4 kHz files haven't been checked.
- **Building the library takes time.** About 2 minutes per 1,400 tracks on the first start, and again after music changes over USB. The player is busy until it finishes.
- **Library size:** tested on the player with 6,558 tracks and with 1,643 albums, and on a computer with 20,000 tracks. Larger libraries may stop with an error when the library file (4 MB) is full.
- **Album art:** the covers file has room for 2,674 albums. Past that, the remaining albums show without art.
- **Cover Flow** orders albums by album artist, then year, not by album title like the Albums list.

## Not there yet

- No Bluetooth audio, EQ, ReplayGain, lyrics or USB DAC mode.
- The 4.4 mm balanced output hasn't been checked on hardware.
- A short hold of Power (0.6 s) turns the player off.
