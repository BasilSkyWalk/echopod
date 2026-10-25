# EchoPod release notes

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
