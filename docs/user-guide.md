# EchoPod user guide

EchoPod is a replacement firmware for the Snowsky (FiiO) Echo Mini music player. It has a Cover Flow album browser, a music library built on the player, favorites, playlists and an Up Next queue. It is not made or supported by FiiO.

Read `docs/known-issues.md` before you install. EchoPod has been tested on one player so far.

## Which players

- **Echo Mini 8 GB: supported.** Its FiiO update file is called `HIFIECnn.IMG`.
- **Echo Mini 512 MB: not supported.** Its FiiO update file is called `MINIVnnn.IMG`. Its firmware ignores EchoPod's file, and EchoPod refuses its firmware.

Tested starting points: FiiO firmware 3.7.0 and 3.9.0 (Settings → About on the player shows yours).

## Before you install

1. Copy anything you want to keep off the player's internal storage.
2. Charge the battery.
3. Download `HIFIEC90.IMG` and `SHA256SUMS.txt` from the EchoPod release, and check the file:
   - Windows (PowerShell): `Get-FileHash HIFIEC90.IMG`
   - macOS or Linux: `shasum -a 256 HIFIEC90.IMG`

   The result must match the line for `HIFIEC90.IMG` in `SHA256SUMS.txt`. If it doesn't, download it again.

## Install

1. Connect the player to your computer. The internal storage appears as a drive called ECHO MINI.
2. Copy `HIFIEC90.IMG` to the top level of that drive, not into a folder. Delete any other `HIFIECnn.IMG` or `ECHOMINI.IMG` file there first: the player installs the one with the highest number.
3. Eject the drive (Windows: Safely Remove; macOS: Eject), then unplug.
4. Take the microSD card out.
5. Switch the player off and on again. It shows "Updating, Don't power off" for about a minute or two. Leave it alone until EchoPod starts.
6. Put the microSD card back. On the first start with a card, EchoPod builds its library and album covers, which takes about 2 minutes per 1,400 tracks.

If "Updating" flashes for under a second and FiiO's firmware comes back, the install didn't happen. See "If something goes wrong" below.

## Your music

- Put music on a microSD card formatted FAT32 or exFAT, in any folder layout.
- EchoPod plays FLAC, MP3 and WAV, up to 192 kHz. It lists the other formats FiiO lists, and says when it can't play one.
- Album covers come from art inside each album's first track, or from a `cover`, `folder`, `front` or `albumart` JPEG or PNG next to the tracks.
- `.m3u` and `.m3u8` playlists anywhere on the card appear under Music → Playlists.
- EchoPod keeps its own files in a hidden `/.echo` folder on the card. Don't delete it unless you want the library rebuilt.

## Copying music over USB

1. Connect the player. Two drives appear: internal storage and the SD card.
2. Copy music to the SD card.
3. **Eject both drives before you unplug.** If you unplug first, recent changes may be lost, and the player warns you.
4. When music changed, the player rebuilds its library by itself. Copying other files doesn't trigger a rebuild. Settings → Rescan library rebuilds it on demand.

## Buttons

| Button | In lists | On Now Playing |
|---|---|---|
| Volume up / down | Next / previous item; hold to scroll faster | Volume; hold Vol+ for the next track, Vol− for the previous one |
| Play | Select; a song plays the list from that song. Hold on an album for Favorites, hold on a song for Play Next / Add to Up Next | Play / pause; hold to seek |
| Menu | | Opens Up Next |
| Back | Back; hold to go home | Back; hold to go home |
| Power | Lock or unlock; hold to turn off | Same |

## Updating EchoPod

1. Copy the new `HIFIEC90.IMG` to the top level of internal storage.
2. Eject both drives. The player checks the file and shows "Installing update", then "Update installed".
3. Unplug, then hold Power to turn the player off, and switch it on again.

## Going back to FiiO's firmware

1. Download FiiO's firmware for the **8 GB** Echo Mini from FiiO's website and unzip it.
2. Delete `HIFIEC90.IMG` from internal storage if it's still there.
3. Copy FiiO's `HIFIECnn.IMG` (for example `HIFIEC39.IMG`) to the top level of internal storage, unchanged.
4. Eject both drives. The player checks the file (this takes a while) and shows "Installing FiiO firmware", then "FiiO firmware installed".
5. Unplug, take the microSD card out, hold Power to turn the player off, and switch it on. FiiO's firmware starts, shows "Updating" once more for a minute or two while it installs the same file, and deletes it.

## If something goes wrong

- **The install doesn't take** (FiiO's firmware comes back after a short "Updating"): copy the file again, check its hash on the player's drive, and make sure it's the only `HIFIECnn.IMG` there. If it keeps failing, the only other way in is maskrom (below).
- **The player doesn't start at all:** every case so far was recovered through the chip's maskrom mode, which writes FiiO's firmware back from a computer over USB. It needs a patched `rkdeveloptool` and some care; the steps are in `docs/device-recovery.md`.
- **Reporting a problem:** include what you did, and the Diagnostics → Firmware, Last start, Last step and Last USB lines.
