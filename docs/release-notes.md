# EchoPod release notes

## Unreleased

### New

- **Random album.** Cover Flow starts with a Random album tile (a shuffle mark as its cover), just before the first album. Press Play on it to hear a random album from its first track, in order.

### Fixed

- **Pairing remembered headphones again.** Headphones the player already knew didn't show up when put in pairing mode, and connecting could take a long time unless Bluetooth was turned off and on. A search that finds remembered headphones now connects to them straight away; after Pair new device or a disconnect, their row shows "Nearby" instead.

## 0.5.0

### New

- **Bluetooth headphones.** Settings → Bluetooth turns Bluetooth on, pairs headphones (Pair new device) and lists up to six you've used, most recent first. With Bluetooth on, the player connects to your last headphones by itself at start-up and when they come back, and resets and retries a connection that stalls. Press Play on the connected headphones to disconnect them, on others to switch. Music goes to the headphones whenever nothing is in either jack; their pause, play, next, previous and in-ear detection work, and a dropped connection pauses playback. A Bluetooth mark next to the play icon shows Bluetooth on (dim) or connected (bright). With nothing connected for 5 minutes, Bluetooth switches itself off until you open its screen or restart. Tested on the player with OnePlus Buds 3.
- **Charge screen.** Switching off with a cable plugged in, or plugging a charger into a switched-off player, now shows the battery level and "Charging" or "Charged" (the screen goes dark after 5 seconds; any button lights it). Hold Power to turn the player on, or unplug to switch it off.

### Fixed

- **Switching off on a cable.** Holding Power with a charger or computer plugged in left the screen black and the player unresponsive until the reset button: the cable kept it powered. It now shows the charge screen.
- **Battery percent** ([#4](https://github.com/BasilSkyWalk/echopod/issues/4)). The level dropped a few points whenever playback or the screen briefly loaded the battery, and could show 98 % on the charger when the battery held less, which then looked like drain while off. It now averages about a minute of readings, never shows more than 99 % while charging, and settles to the true level within a minute of unplugging.
- **Start-up.** The screen stays dark until EchoPod's first frame, so the panel's noise no longer flashes in the corner.
- **Dark screen during USB.** A message shown with the screen off (such as "Computer found") could stay almost black for the whole USB session; it now lights fully.

### Known limits

- To switch from Bluetooth to wired headphones, pause, plug them in and press Play: while music goes to Bluetooth the player can't read the jack.

## 0.4.0

### New

- **Chinese, Japanese and Korean names.** Song, artist and album names in Korean, Simplified Chinese and Japanese no longer show as `?`: the player now has the 2,350 common Hangul syllables, the 6,763 hanzi of GB 2312, the kanji of JIS X 0208, kana and CJK punctuation, at both text sizes (Noto Sans CJK SC). The characters are stored in the firmware image and read as a screen needs them. Traditional-only Chinese characters, emoji and other scripts still show as `?`.

### Changed

- **Accented letters** (é, ü, ñ…) are now read the same way instead of being built into the firmware's code. They look exactly as before, and the space they free leaves room for future features.

## 0.3.0

### Fixed

- **Going back to FiiO's firmware 4.0.0.** EchoPod refused FiiO's newest 8 GB firmware (`HIFIEC40.IMG`) as "isn't a valid image for this player": it told the 8 GB and 512 MB models apart by a size field that FiiO changed in 4.0.0. It now looks for the update file name each FiiO image is built to look for (`HIFIECnn.IMG` on the 8 GB model, `MINIVnnn.IMG` on the 512 MB one), checked against FiiO's 3.7.0 to 4.0.0 images, and still refuses the 512 MB model's firmware. Tested on the player: 0.3.0 installed FiiO 4.0.0, and FiiO 4.0.0 installed 0.3.0 again. On 0.2.0, install FiiO 3.9.0 first and update to 4.0.0 from there.
- **USB DAC plays on more computers and phones.** Android always picked 96 kHz, and some computers pick 88.2 or 96 kHz too, but this chip can't take packets that large, so those hosts showed the format and played nothing. DAC mode now offers 44.1 and 48 kHz (16 or 24-bit), and no longer depends on the order in which the computer sets the format and the rate. All four formats played on the player from Windows 11, and it played from an Android phone; Linux hasn't been tried yet. If you set your computer to 88.2 or 96 kHz before, pick 48 kHz.
- **Gapless MP3.** MP3s made with LAME or ffmpeg now play without a gap between tracks: the player reads the encoder delay and padding from the file's LAME tag and drops the silence the encoder added at each end.

### Faster

- **Rescans keep the covers.** A rescan no longer decodes every album's picture again: covers whose art is unchanged are kept, and only new albums and changed art are decoded (about half a second each). The first rescan after updating still decodes them all, once. The covers file now has room for 2,673 albums instead of 2,674.
- **echo-sync's covers are kept too.** echo-sync now writes `covers.bin` in the player's layout with the same art stamps, so when the player does rescan (say after you add an album), it keeps the covers the computer made and decodes only what's new. Download echo-sync again to get this.
- JPEG covers decode slightly faster (about 5–8 % on a PC).

## 0.2.0

### New

- **Audiobooks and podcasts.** Put books in an `Audiobooks` folder and shows in a `Podcasts` folder at the top of the card. Home gets an Audiobooks and a Podcasts shelf. Each book and episode remembers where it stopped, books always play in order (even with shuffle on), and holding vol± jumps 30 s forward or 15 s back instead of skipping. Books and episodes stay out of Music: they never show up in Cover Flow, Albums, Artists, Genres, Songs or shuffle.
- **USB DAC mode.** Settings → USB mode → DAC turns the player into a USB sound card (USB Audio Class 1.0, 16 or 24-bit up to 96 kHz). It has its own volume, remembered between sessions (vol± changes it, hold to repeat), and the screen shows the format and volume.
- **echo-sync: build the library on a computer.** One file for Windows, macOS (Apple silicon) or Linux, nothing to install. It reads the music on the player's card (in Storage mode or a card reader) and writes the library, covers and scan fingerprint, so the player starts without scanning. 1,563 tracks take about 2 minutes. See "Build the library on a computer" in `docs/user-guide.md`.

### Faster

- **USB storage reads:** about 3.9 MB/s instead of 1.3 MB/s on Windows (one multi-block card read per request).
- **Library rebuild on the player:** writes of whole sectors go to the card in one command without reading them first, folder listings are read 4 KB at a time, and opening the tracks of one album no longer walks the card from the top for each one. A 1,563-track, 144-album card now rebuilds in about 1 minute 45 seconds, covers included; the faster file lookups alone took its track scan from 61 s to 19 s.
- **echo-sync** skips embedded pictures and padding when it reads tags: 2 minutes instead of 22 for the same card.

### Fixed

- Settings → Brightness: every level now changes the backlight, from dimmest at 1 to full at 10.
- Settings → Output explains what it does: both jacks are driven by the same DACs, so it picks the volume table, and Auto shows the plugged jack.
- Favorites no longer makes the music repeat a beat while the list opens.
- Sound comes back after using USB, without a restart.
- The battery level no longer drops by about 10 % when the charger is plugged in, and doesn't jump when it's unplugged.
- After echo-sync, the player no longer rescans exFAT cards: both sides now compare file times in UTC.

### Good to know when updating

- **The library format changed (version 3).** After updating, the player rebuilds its library once, even if the music didn't change. echo-sync files from before 0.2.0 write the old format: download echo-sync again.
- **Rescan timings.** Each rebuild saves where its time went to `/.echo/rescan_times.bin` on the card. If a rebuild seems slow, send that file with your report.
- Everything else that's missing or unproven is in `docs/known-issues.md`.

## 0.1.0

The first public build: FLAC, MP3 and WAV playback, library and covers built on the player, Cover Flow, browsing, playlists, favorites, resume, USB storage, and updates (and the way back to FiiO's firmware) from a file copy.
