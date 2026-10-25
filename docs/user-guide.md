# EchoPod user guide

EchoPod is a replacement firmware for the Snowsky (FiiO) Echo Mini music player. It has a Cover Flow album browser, a music library built on the player, favorites, playlists and an Up Next queue. It is not made or supported by FiiO.

Read `docs/known-issues.md` before you install. EchoPod has been tested on one player so far.

**Use it at your own risk.** EchoPod comes with no warranty (see `LICENSE`). Replacing a player's firmware can leave it unable to start; during development that happened, and the player was brought back through maskrom (`docs/device-recovery.md`). Only install it if you're prepared to do that.

## Which players

- **Echo Mini 8 GB: supported.** Its FiiO update file is called `HIFIECnn.IMG`.
- **Echo Mini 512 MB: not supported.** Its FiiO update file is called `MINIVnnn.IMG`. Its firmware ignores EchoPod's file, and EchoPod refuses its firmware.

Tested starting points: FiiO firmware 3.7.0 and 3.9.0 (Settings → About on the player shows yours).

## Before you install

1. Copy anything you want to keep off the player's internal storage.
2. Charge the battery.
3. Download `HIFIEC90.IMG` and `SHA256SUMS.txt` from the [EchoPod releases](https://github.com/BasilSkyWalk/echopod/releases), and check the file:
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
- Libraries of many thousands of tracks work; `docs/known-issues.md` lists the sizes tested so far.
- EchoPod keeps its own files in a hidden `/.echo` folder on the card, about 110 MB of them. Don't delete it unless you want the library rebuilt.

## Copying music over USB

1. Connect the player. Two drives appear: internal storage and the SD card.
2. Copy music to the SD card.
3. **Eject both drives before you unplug.** If you unplug first, recent changes may be lost, and the player warns you.
4. When music changed, the player rebuilds its library by itself. Copying other files doesn't trigger a rebuild. Settings → Rescan library rebuilds it on demand.

## Using the player as a USB DAC

**Untested on a player so far** (see `docs/known-issues.md`). In this mode the player is a sound card for your computer or phone: their audio plays through the player's headphone outputs.

1. On the player, Settings → USB mode → DAC.
2. Plug in the USB cable. The player shows "USB DAC". No driver is needed on Windows 10 and later, macOS, Linux, Android or iOS (phones need a USB-C or Lightning adapter that passes data).
3. On the computer, choose EchoPod as the sound output (Windows: the speaker icon in the taskbar; macOS: System Settings → Sound → Output). The player shows the format, for example "Playing from computer · 48 kHz 24-bit".
4. Use the computer's volume, the player's volume keys, or both. Start with both low.
5. Unplug when you're done, or hold Back for a second and let go to go back to the player's own music (it keeps charging).

The player accepts 16 or 24-bit at 44.1, 48, 88.2 and 96 kHz; the computer picks one (on Windows: Sound settings → EchoPod → Properties → Advanced). Music on the player stops while it's a DAC, and your drives don't appear: set USB mode back to Storage to copy music. The setting takes effect the next time you plug in. A charger works the same in both modes.

## Build the library on a computer

Rebuilding on the player takes about 2 minutes per 1,400 tracks. A computer can build the same library and album covers instead, straight onto the card, and tell the player that the card has already been scanned.

The tool is called **echo-sync**. Download it from the release page, under Assets; it's one file, with nothing to install:

| Computer | File |
|---|---|
| Windows | `echo-sync-windows-x64.exe` |
| Mac with Apple silicon (M1 and later) | `echo-sync-macos-arm64` |
| Linux (x86-64) | `echo-sync-linux-x64` |

**Windows**

1. Copy your music to the card, in the player over USB or in a card reader.
2. Double-click `echo-sync-windows-x64.exe`. Windows may say it "protected your PC" because the file isn't signed: click **More info**, then **Run anyway**.
3. It finds a card the player has used and asks "Card to sync [E:\]": press Enter. For another card, type its drive letter (or drag the drive onto the window) and press Enter.
4. When it says "Done", press Enter to close it, eject the card (in the player: both drives), and put it back in the player or unplug.

From a command prompt, `echo-sync-windows-x64.exe E:` does the same without asking.

**Mac**

1. Copy your music to the card.
2. In Terminal, allow the downloaded file to run once: `chmod +x ~/Downloads/echo-sync-macos-arm64 && xattr -d com.apple.quarantine ~/Downloads/echo-sync-macos-arm64`.
3. Run `~/Downloads/echo-sync-macos-arm64` and press Enter to use the card it finds, or drag the card from the desktop into the window first. You can also give it straight away: `~/Downloads/echo-sync-macos-arm64 /Volumes/CARD`.
4. Eject the card.

**Linux**

`chmod +x echo-sync-linux-x64`, then `./echo-sync-linux-x64 /media/you/CARD` (the card's mount point), then eject the card.

**Intel Macs, other computers, or from the source.** Install Python 3, then the tools' packages once with `python3 -m pip install -r tools/requirements.txt`, and run `python3 tools/echo_sync.py /Volumes/CARD`. On Windows, `tools\echo_sync.bat E:` finds Python for you.

It reads the whole card, as the player does, and writes `library.db`, `covers.bin` and `scan.bin` into the card's `/.echo` folder. If the player has never used the card, it asks before creating `/.echo` (`--yes` creates it without asking). Files the player made are overwritten in place and keep their size; missing ones are made at the player's sizes (4 MB, 80 MB and 4 KB). It then prints what it found, for example "1,402 tracks, 120 albums, 3 playlists" and "115 covers, 5 without art".

**Why the player doesn't rescan.** After a USB session the player checks for changes: it compares the names, sizes and modification times of the music, playlists and cover images on the card with a fingerprint it keeps in `scan.bin`. The computer writes the same fingerprint, so the check finds nothing new and the player keeps the computer's library. With a card reader there's no USB session, so the player uses the new library straight away.

**When the player rescans anyway.** If the fingerprints differ, the player rescans as it would without the computer, and nothing is lost. That happens when:

- the music changed after the computer ran;
- the card is exFAT (most cards over 32 GB) and its music was copied by a computer in another time zone, by Linux, or by a phone or camera. exFAT keeps a time-zone offset with each file time that computers don't show, so the computer assumes the music was copied by a computer set to its own time zone. FAT32 cards on Windows don't have this problem;
- the computer is a Mac or runs Linux: how these read the card's file times hasn't been checked against the player yet;
- the card has folders more than 31 levels deep or paths longer than 1,023 bytes (the player can't fingerprint those);
- a name was saved decomposed (for example by Linux) and read on a Mac, or only as an old DOS short name with accented letters.

To see whether it works on your card, let the player scan it once, then run echo-sync with `--check` without changing anything (for example `echo-sync-windows-x64.exe E: --check`). "They match" means the player won't rescan after the next sync.

**After a firmware update** the player checks the music once when it first starts ("Checking for changes"), and rescans only if the music changed. `scan.bin` remembers the firmware version; the computer keeps it. A `scan.bin` the computer had to create has no version yet, so the player checks once on its next start too.

| Option | Effect |
|---|---|
| `--dry-run` | Read everything and say what would be written, but write nothing |
| `--no-covers` | Build the library only; `covers.bin` stays as it is, so new albums show without covers until the next rescan on the player |
| `--covers-only` | Rebuild `covers.bin` from the card's `library.db` |
| `--no-fingerprint` | Leave `scan.bin` alone, so the player compares the music with its own last scan |
| `--check` | Only compare the computer's fingerprint with the player's |
| `--yes` | Create `/.echo` without asking |
| `--verbose` | List the albums without art |

The computer picks each album's cover the way the player does: the picture inside the album's first track (FLAC, MP3, WAV, DSF/DFF, `.m4a`, `.ape` and `.wma`; a front cover wins), otherwise a `cover`, `folder`, `front` or `albumart` JPEG or PNG next to the tracks. Ogg pictures are skipped, as on the player. The covers can look a little different from the player's own, because the computer resizes them with a different filter.

## Buttons

| Button | In lists | On Now Playing |
|---|---|---|
| Volume up / down | Next / previous item; hold to scroll faster | Volume; hold Vol+ for the next track, Vol− for the previous one (in audiobooks and podcasts: 30 s forward, 15 s back) |
| Play | Select; a song plays the list from that song. Hold on an album for Favorites, hold on a song for Play Next / Add to Up Next | Play / pause; hold to seek |
| Menu | | Opens Up Next |
| Back | Back; hold to go home | Back; hold to go home |
| Power | Screen off or on; hold 2 s to turn off | Same |

With the screen off, ▶‖ still pauses and resumes, a tap of vol± changes the volume and holding vol± skips tracks (or jumps 30 s / 15 s in audiobooks and podcasts).

## Audiobooks and podcasts

Put audiobooks in a folder called `Audiobooks` and podcasts in a folder called `Podcasts`, at the top of the card (any capitalisation; `Audiobook` and `Podcast` work too). Home then shows **Audiobooks** and **Podcasts**; they stay hidden while those folders don't exist.

```
Audiobooks/
  The Long Road/          one folder per book
    01 - Departure.mp3    chapters play in file-name order
    02 - The River.mp3
  Short Story.mp3         a book in one file works too
Podcasts/
  Tech Talk/              one folder per show
    2026-10-01 - Episode 41.mp3
```

- **Audiobooks** lists your books, each with "3 chapters", "Chapter 2 of 12" or "Finished". ▶ on a book carries on where you stopped (or starts a finished book again). Hold ▶ on a book to see its chapters.
- **Podcasts** lists your shows; open one to see its episodes, each with "23:10 left" or "Played".
- Every book and every episode remembers its own place, even when you play music in between or turn the player off. The places are saved in `/.echo/bookmarks.bin` (the last 128 books and episodes).
- Books and shows always play in order: shuffle doesn't apply to them, and your shuffle setting for music stays as it was.
- On Now Playing, holding Vol+ jumps 30 s forward and holding Vol− jumps 15 s back. Jumping past the end of a chapter goes to the next one. To pick another chapter, use the book's chapter list.
- An episode or book counts as finished once it played to its last tenth (at most its last 30 s).
- These folders still appear under Music (Albums, Artists, Folders and so on), as all tracks on the card do. Give them a genre such as "Audiobook" if you want to find them there.
- Playback speed can't be changed yet.

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
- **Reporting a problem:** include what you did, your FiiO version before installing, and the Firmware, Last start, Last step and Last USB lines from Home → Diagnostics.
