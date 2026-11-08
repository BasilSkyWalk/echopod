# EchoPod release notes

## 0.11.1

Smoother and steadier: 192 kHz FLAC plays cleanly again, and menus, Cover Flow and card reads do less work. Tested on one Echo Mini 8 GB.

### Fixed

- **192 kHz FLAC plays cleanly again.** Since 0.11.0, FLAC at 176.4 and 192 kHz could crackle and play at the wrong speed: the second core waited for the main core before decoding the next block, so at those rates the audio ran out. It now decodes the next block straight away, and runs at full speed for FLAC and ALAC above 96 kHz.
- **The clock and battery percentage in landscape** no longer come and go as the minutes change (issue 57). The header title is a few pixels narrower to make room for the clock.
- **Long tracks on FAT32 storage** (internal storage, and microSD cards of 32 GB or less) no longer take more work to read the further they play. Covers and the library also load with fewer reads on FAT32.
- **If the audio buffer ever runs dry,** you hear a moment of silence instead of the last fraction of a second again.
- **Moving between FLAC or ALAC and MP3, AAC or Opus, or turning the equalizer on or off,** now gives a short clean gap at the next track instead of a possible crackle on hi-res tracks.
- **A key no longer sticks** when many presses arrive during a long rescan (a volume key could keep stepping), and taps made during a rescan aren't replayed afterwards.

### Smoother

- Slides start once the new screen has loaded, so the first frame doesn't jump.
- Cover Flow moves the same way at any frame rate, and a slow frame no longer snaps straight to the album.
- Going back to a list doesn't reload it. Opening a long playlist in Cover Flow, starting playback from a long list, the fast-scroll letter and the Home cover read far less from the card.
- The resume save every 15 s waits until a slide or fade has finished.

## 0.11.0

This release answers most of the requests from GitHub and Reddit: listen history, a clock, music on internal storage, grouped settings, and fixes for battery life, Bluetooth, USB and the SD card. Tested on one Echo Mini 8 GB.

### New

- **Listen history for Last.fm and ListenBrainz** (issue 38). Each time you connect to a computer, the player adds what you played to `/.scrobbler.log` on the card, the file Rockbox-style uploaders read (for example rb-scrobbler). `tools/echo_history.py` can also write it. A play counts when the track is over 30 s and you heard half of it or 4 minutes. Audiobooks and podcasts aren't logged. Settings → Listen history turns it off.
- **A clock** (issue 23). EchoPod reads the player's clock chip at start, sets it from your computer at the end of every echo-sync, and Settings → Date & time sets it by hand. Settings → Clock shows the time in the status bar. Listen history uses it for real play times; without a set clock, times are estimated by counting back from when you connect. A clock dated before this firmware was built counts as not set, and the first start after installing asks once to set it. Diagnostics → Clock shows what the chip holds.
- **USB DAC mode:** play/pause, next and previous reach the computer; volume keys no longer crackle; the DAC screen shows the battery; Swap volume buttons and portrait apply.
- **Ask when connected:** plugging into a computer asks Storage, DAC or Charge only (issue 5). Settings → Ask when connected turns it off.
- **Music on internal storage** (issue 36). Music on the player's own storage is in the library too, as an "Internal storage" folder; Settings → Music folders leaves its folders out. echo-sync finds the player's internal storage drive by itself. The library files stay on the microSD card.
- **Settings are grouped** into Sound, Display, Library, Buttons and System.
- **Settings → Music folders:** untick top-level folders to leave them out of the library.
- **Cover Flow for folders and playlists** (issue 17): Settings → Folders & playlists → Cover Flow. **Settings → Cover Flow** shows 5 or 3 covers, and the outer covers are smaller.
- **Lyrics:** a `.lrc` file next to a track shows on Now Playing. Settings → Lyrics turns them off.
- **Settings → Theme → Light.**
- **Album versions:** albums tagged with `ALBUMVERSION` show separately, for example "Abbey Road (2019 Remix)".
- **Settings → Animations:** Normal, Slower or Faster (issue 15).
- **Settings → Bluetooth level → Louder** for headphones that are too quiet (issue 28).
- **Settings → Portrait status bar → Large** and **Settings → Power on → Hold**.
- **Polish** (by micdudk, pull request 39) and **Bahasa Indonesia** (issue 30). The Indonesian text hasn't been checked by a native speaker yet.
- **Traditional Chinese** (Big5 level 1) and more punctuation in names (issue 30).
- **FLAC files with blocks up to 16,384 samples** play.
- **More than 2,673 album covers** on the player: covers.bin grows during a rescan.
- **Diagnostics:** Power, Savings, USB errors, Last update, Last stack and Stack.

### Fixed

- **No microSD card:** Home and Music now say so, and saving a setting without a card asks for one (issue 42).
- **SD card waits have a time limit**, so a card that stops answering gives an error instead of freezing the player (issue 43).
- **The first start after an update** shows the start-up screen and a lit "Checking for changes" instead of a black screen.
- **Battery:** the second core sleeps when it has nothing to decode; FLAC, WAV, ALAC and MP3 at 48 kHz or less play at 96 MHz with the screen off; after 30 s paused the audio hardware powers down; the screen sleeps when it's off; the percent moves 1 point at a time and holds still for 30 s after plugging in or out.
- **Bluetooth:** the stream starts once the headphones report they're ready instead of straight away; the opening of a track isn't lost; garbled replies from the Bluetooth chip no longer drop the link; Play wakes a module that switched itself off.
- **Clicks** at track changes between sample rates and at pause (issue 21), and the first part of a track no longer cut off.
- **Embedded art in Opus and Ogg files** (issue 29), and albums sharing a folder get their own covers.
- **USB storage:** short dips in USB power no longer end the session, card reads are retried, and a reseated card is found again.
- **Scrolling titles** move smoothly (issue 21).
- **The rescan code is checked against the running firmware** before it runs.

### When you update

- Your library and settings stay as they are. Going back to 0.10 or earlier resets settings to defaults.
- After an update installs, restart the player before connecting it to a computer again.
- Delete the update file from the player once it has installed, or it installs again.
- Set the clock after installing (Settings → Date & time) or sync with echo-sync. A clock dated before the firmware was built counts as not set, and the player asks once after the update.

## 0.10.1

### Fixed

- **Audiobooks and podcasts at 22 kHz play** (issue 37). Files at 8, 11.025, 12, 16, 22.05, 24 and 32 kHz used to be refused with "22.1 kHz isn't supported". The player now plays them at a rate it supports (2 to 6 times theirs), filling in the new samples smoothly between the real ones. Now Playing still shows the file's own rate.
- **Sample rates show exactly**, for example 22.05 kHz instead of 22.1 kHz.

### When you update

- Your library, settings and everything else stay as they are.
- Delete the update file from the player once it has installed, or it installs again.

## 0.10.0

This release answers most of the open GitHub issues.

### New

- **Play all and Shuffle all** at the top of every artist's, album artist's and genre's album list, and Shuffle all on All Songs and playlists (issues 9 and 22). The list still opens on the first album, so scroll up to reach them.
- **Settings → Volume buttons:** *Tap: volume, hold: skip* (as before), *Tap: skip, hold: volume*, or *Tap: volume, hold: seek* (issues 21 and 25). In every mode, holding the rocker right after tapping the volume keeps changing the volume (issue 5).
- **Settings → Swap volume buttons** turns + and − around (issue 25).
- **Settings → Menu button → Next track:** on Now Playing and with the screen off, ≡ skips to the next track; hold ≡ for Up Next (issue 26).
- **Hold ≡ anywhere outside Now Playing to open Settings** (issue 21).
- **Settings → Accent color** (Blue, Red, Orange, Green, Teal, Purple, Pink, Graphite) and **Highlight** (Gradient or Flat) (issue 20).
- **Settings → Battery percent** hides the number beside the battery icon (issue 24).
- **Settings → Cover Flow order → Album title** sorts Cover Flow by album name instead of album artist (issue 18).
- **Settings → Artist albums → Cover Flow** opens an artist, album artist or genre as a Cover Flow of only their albums (issue 27).
- **Start screen** can also be Album Artists, Genres, Folders or All Songs (issue 22).
- **Russian and Ukrainian** interface languages (issue 21). Not yet checked by a native speaker; corrections are welcome.
- **`tools/echo_bookmarks.py`** lists the audiobook and podcast bookmarks on a card, or with `--finished` prints the finished episodes one per line for sync scripts (issue 16).
- **Diagnostics → Panel** shows the screen's ID, which says which of the two screen start-up sequences the player uses (issue 15).

### Fixed

- **Turning off with the screen off now shows "Goodbye"** (issue 5). Before, the player switched off silently, so it looked like nothing happened. The sleep timer and auto off still turn off without lighting the screen.
- **Fast scroll no longer races through the alphabet** (issue 5): after 3 s of holding it moves one letter every 0.45 s instead of one every 0.12 s.
- **Holding the rocker in Cover Flow speeds up**, up to 4 albums a step (issue 5).
- **A "?" at the end of some titles** (issue 21): invisible characters such as a stray byte-order mark, zero-width spaces and control codes are dropped, and bytes 0x80–0x9F in Latin-1 tags are read as Windows-1252 (curly quotes, dashes, "…"). Rescan the library to apply it. echo-sync does the same.
- **The Bluetooth icon is a crisp 5 × 9 drawing** instead of thin diagonals that blurred at status-bar size (issue 21).
- **Going back to FiiO's firmware** (issue 1):
  - The search for the second firmware slot prefers the copy that matches the first slot, so an old leftover firmware header can no longer make the slot look too small.
  - "isn't a valid image" now ends with a code such as `(E6/32)`: the reason (1 size, 2 header, 3 header copy, 4 checksum, 5 wrong model, 6 doesn't fit, 7 can't boot) and the slot size in MB.
- **Portrait album lists drop the year** so album names have more room (issue 5).

### Not in this build

- FLAC files with blocks over 4,608 samples (ffmpeg's default at 96 kHz) still don't play (issue 5). Re-encode with `flac -b 4096` until the decoder's buffers can be reorganised.
- Lyrics (issues 3 and 14), Cover Flow for folders and playlists (issue 17), the 512 MB model (issue 19, waiting on the contributor's pull request), a clock (issue 23) and the USB mode prompt and long-press power-on (issue 5).

### When you update

- Your library, settings, equalizer, orientation, language and Bluetooth headphones stay as they are. The new settings start at their old behaviour.
- The settings file gains 8 bytes. Going back to 0.9.1 or earlier makes that firmware ignore the file and start from default settings.
- Rescan the library once (Settings → Rescan library) to clean up titles that showed "?".
- Delete the update file from the player once it has installed, or it installs again.

## 0.9.1

Most of this comes from a comparison with FiiO's 4.0.0 firmware that velppa's Claude Code made (issue 12). Tested on the one Echo Mini 8 GB.

### New

- **Settings → Filter and Balance.** Filter has five options, 1 to 5, that write FiiO's five DAC filter values; the names aren't known, so they're numbered. Balance runs from L12 to R12 and turns the louder side down by one dB a step (the step size is a guess, not FiiO's). Both are saved and apply at once.

### Fixed

- **The quietest volume steps are quieter.** Step 1 starts at FiiO's quietest level and the first three steps rise smoothly from it. Before, step 1 was about −60 dB, which was loud with sensitive in-ear monitors.
- **Bluetooth has its own volume curve**, 6.5 dB quieter than wired, as in FiiO's firmware.
- **The screen type is read from the panel** (READ ID) instead of assumed, so a unit with the other panel gets its own start-up sequence. Tested only on a panel that uses the one EchoPod already used.
- **The headphone-jack detect pins get their pull settings at start-up**, as in FiiO's firmware, not only when audio goes to Bluetooth.
- **Turning off while music plays powers the DAC down first**, which should avoid a pop.
- **The updater also installs `ECHOMINI.IMG`**, checked before `HIFIEC00.IMG` to `HIFIEC97.IMG`.
- **The second firmware slot is found by its header** when it isn't where EchoPod assumed it, so internal-storage writes are fenced to the real slots. On the tested unit nothing changes.

### When you update

- Your library, settings, equalizer, orientation, language and Bluetooth headphones stay as they are.
- The settings file gains a filter and balance byte. Going back to 0.9.0 or earlier makes that older firmware ignore the file and start from default settings.
- Delete the update file from the player once it has installed, or it installs again.

## 0.9.0

### New

- **Languages.** Settings → Language: English, Simplified Chinese, Japanese, Korean, German, French, Spanish, Italian, Portuguese, Dutch, Swedish, Danish, Norwegian and Finnish. The choice is saved and applies after you restart the player (the toast says so). Menus, settings, the equalizer, Bluetooth, USB, the updater and the diagnostics all follow it. The English text is no longer part of the firmware's code: every language, English included, is a small pack stored in the image and read when the player starts, which also leaves more free room for code than before.
- The translations were written by Claude and no native speaker has reviewed them; the Chinese starts from chuqiao's translation (pull request 11). Some long texts may wrap or be cut on the small screen. Corrections are welcome as changes to `assets/lang/<code>.txt`.

### Fixed

- **Cyrillic, Greek, Polish, Czech, Turkish, Romanian and Vietnamese letters in track, artist and album names show instead of `?`** (reported by Katsuki, issue 7). Latin Extended-A letters, Greek, Cyrillic and the Vietnamese letters are now in the fonts the player reads on demand.
- A long text that doesn't fit its box is no longer cut in the middle of a character.

### When you update

- Your library, settings, equalizer, orientation and Bluetooth headphones stay as they are. The language starts on English.
- The settings file gains a language byte. Going back to 0.8.1 or earlier makes that older firmware ignore the file and start from default settings.
- The Battery low notice shown before the player's storage starts, and the panic screen, stay in English.

## 0.8.1

### Fixed

- **176.4 kHz plays at the right speed.** It ran 0.28 % slow (about 5 cents flat) because of a clock setting inherited from FiiO's firmware; it now uses the same clock as 44.1 and 88.2 kHz. Found and measured by guiral lacotte, who is building another firmware for the Echo Mini.

### Changed

- **A steadier 200 MHz clock.** The main PLL ran its oscillator at the very bottom of its range; it now runs mid-range and gives the same 200 MHz.
- **Quicker menus.** Home, Settings and choice lists act when you press Play, not when you let go. Holding Play on a song or an album still opens its menu.
- **Less work when nothing moves.** A screen that is not changing is no longer redrawn, and the player sleeps between key presses, which should help the battery a little (not measured yet).
- **Pause and skip respond sooner**, because the resume file is written after the screen is drawn.
- **Albums, Favorites and Cover Flow open and scroll with fewer reads from the card.**
- The splash is shorter, 0.5 s instead of 1.1 s.

### When you update

- Your library, settings, equalizer, orientation and Bluetooth headphones stay as they are.

## 0.8.0

### New

- **Portrait mode.** Settings → Orientation → Portrait or Portrait, flipped turns the screen upright straight away, and the player starts that way next time. Home shows the album art under the menu, Now Playing puts the cover above the track, Cover Flow scrolls albums from top to bottom, the Custom EQ shows its seven bands as rows, and lists show 11 rows. The volume rocker turns with the screen, so the button on top moves up through lists; in Portrait (not flipped) it also turns the volume down.

### When you update

- Your library, settings, equalizer and Bluetooth headphones stay as they are. Orientation starts on Landscape.

## 0.7.1

### Fixed

- **The first start with a FAT32 card no longer looks frozen.** With a FAT32 microSD card full of music, the first start after installing could stay on a black screen for minutes while the player set up its files on the card, so it looked like it wouldn't boot. It now finds the free space on the card in one pass instead of one pass per file, and shows "Setting up the card..." while it works. exFAT cards weren't affected.

## 0.7.0

### New

- **Equalizer.** Settings → Equalizer, or hold ≡ on Now Playing. It has the presets from FiiO's firmware (Retro, Bass, Heavy, Pop, Jazz, Unique) and a 7-band Custom EQ (63 Hz to 16 kHz, ±12 dB): Vol± pick a band, ▶ grabs it and Vol± set its level. You hear each preset as you scroll to it, and the player remembers the last one you used. Boosts lower the overall level a little so loud music doesn't distort. The equalizer works for tracks up to 96 kHz, and through Bluetooth; it is off for 176.4 and 192 kHz tracks and in USB DAC mode. For FLAC and ALAC it runs on the second core, which then runs at full speed, so the screen stays smooth on hi-res tracks; that uses a little more battery while the equalizer is on.
- **ALAC (Apple Lossless) plays.** `.m4a` files in Apple Lossless now play, at 16, 20 or 24 bits, mono or stereo, at the same rates as FLAC (44.1 to 192 kHz), with seeking and gapless playback. Now Playing shows ALAC with the bit depth and rate.
- **AAC plays.** `.m4a` files in AAC (the kind iTunes and most phones make) now play, mono or stereo, with seeking and gapless playback (the encoder's lead-in is trimmed). Now Playing shows AAC with the bitrate. HE-AAC files (often used for podcasts and radio) still say they aren't supported. AAC and Opus run the second core at full speed, so they may use a little more battery than FLAC or MP3.
- **Opus plays.** `.opus` files now show up in the library (with their tags and length) and play, mono or stereo, with seeking that lands on the exact spot and gapless playback. Now Playing shows Opus with the bitrate. Rescan the library (Settings → Rescan library) or run echo-sync again to see Opus files you already have.

### Faster

- **Hi-res playback takes half as much of the main processor.** Audio now reaches the output in batches, with a shortcut for the normal volume path: a 96 kHz 24-bit FLAC went from 61 % to 29 % of the main processor, which leaves more time for the screen and the buttons.

## 0.6.0

### New

- **Albums stay together when some tracks feature another artist.** When files have no album-artist tag, a track by "The Weeknd & Justice" used to split off from the rest of "Hurry Up Tomorrow" by The Weeknd. Tracks without that tag that share a folder and album name are now one album, under the artist most of them share. Rescan the library (Settings → Rescan library) or run echo-sync again to see it.
- **Random album.** Cover Flow starts with a Random album tile (a shuffle mark as its cover), just before the first album. Press Play on it to hear a random album from its first track, in order.

### Fixed

- **Bluetooth connects within seconds.** "Connecting…" could last 20–30 seconds after the headphones had already announced the connection. The player now asks the Bluetooth module for its status every second while connecting, as stock does, so "Connected" shows within about a second of the link coming up, whether the player or the headphones started it.
- **Headphones put in pairing mode are found.** Headphones the player already knew didn't show up when put in pairing mode while it was searching, until Bluetooth was turned off and on. Like stock, a search that follows a connection attempt now restarts the module first (half a second, still shown as Searching…). Remembered headphones a search finds connect straight away; after Pair new device or a disconnect, their row shows "Nearby" instead. Devices found stay listed between searches.
- **Play no longer pauses again on Bluetooth.** A late "paused" report from the headphones could pause playback right after Play was pressed. Reports are now ignored for 3 seconds after Play or Pause on the player, and a pause the headphones cause shows "Paused by <name>".
- **Diagnostics → Bluetooth** keeps the last 12 module messages over two lines, with the commands the player sent and the seconds between them, and keeps them when Bluetooth is turned off and on.

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
