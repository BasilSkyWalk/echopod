# Installing development builds

This covers installing builds you made yourself. Release users should follow `docs/user-guide.md`. Before trying a build of your own, make sure you can recover the player (`docs/device-recovery.md`).

## First install

The first install goes through FiiO's own updater, as in the user guide: copy `HIFIEC90.IMG` (from `build/device/` or `build/release/`) to the top level of internal storage while FiiO's firmware runs, eject, unplug, take the microSD card out and restart. It has worked from FiiO 3.7.0 and 3.9.0 with the file copied from Windows.

### Fallback: a direct maskrom write

If FiiO's updater won't take the file, maskrom can write the image straight into both firmware slots. Each slot holds the image's first `0x500000` bytes. On the unit tested, slot 1 is rockusb sector 0 and slot 2 is rockusb sector 69632 (`0x11000`); check yours by reading both headers first (`rl 0 1` and `rl 69632 1` must both show `RKnanoFW` at `0x1F8`).

1. Build and cut the slot contents:

   ```sh
   cmake --preset device && cmake --build --preset device
   head -c 5242880 build/device/HIFIEC90.IMG > build/device/slot.bin
   ```

2. Enter maskrom (`docs/device-recovery.md`, step 2).
3. Write slot 2, then slot 1, and read both back:

   ```sh
   cd external/rkdeveloptool
   ./rkdeveloptool ld && ./rkdeveloptool td
   ./rkdeveloptool wl 69632 ../../build/device/slot.bin
   ./rkdeveloptool wl 0 ../../build/device/slot.bin
   ./rkdeveloptool rl 0 1 s1.bin && ./rkdeveloptool rl 69632 1 s2.bin
   ```

   Both read-backs must equal the first 512 bytes of `slot.bin`.
4. `./rkdeveloptool rd`. The player starts EchoPod.

## Later updates: EchoPod's own updater

1. Connect the player. It shows "Computer found", and two drives appear: internal storage ("ECHO MINI") and the SD card.
2. Copy the new image to the top level of internal storage and check it:
   - Windows (PowerShell): `Copy-Item build\device\HIFIEC90.IMG E:\` then `Get-FileHash E:\HIFIEC90.IMG`, with E: the internal drive.
   - macOS: `cp -X build/device/HIFIEC90.IMG "/Volumes/ECHO MINI/"`, then `rm -f "/Volumes/ECHO MINI/._HIFIEC90.IMG"` and `shasum -a 256` on both copies.
3. Eject both drives. The player shows "Checking HIFIEC90.IMG", then "Installing update" with a percentage, then "Update installed".
4. Unplug, hold Power to switch off, then switch on. The new build starts.

The updater takes the highest-numbered `HIFIECnn.IMG`. It checks the image's shape, rows, entry vector, header copy and CRC before writing, skips an image that is already installed, and writes slot 2 first, then slot 1, reading back every chunk. It also accepts FiiO's own 8 GB image (see "Going back to FiiO's firmware" in the user guide) and refuses the 512 MB model's.

## What to check after an install

- **Home screen:** "EchoPod" with Music, Now Playing, Settings and Diagnostics. Without a card, Music says "No library".
- **Diagnostics → Firmware:** the commit you built.
- **Keys:** Diagnostics → Key test.
- **Power:** a tap turns the screen off and on. Holding it for 2 s switches the player off; with a cable in, it shows the charge screen instead, and holding Power again turns the player back on.
- **USB:** "Computer found" appears and the drives mount within about 10 s.
- **Card:** on the first start with a card, the player creates `/.echo` with `settings.bin`, `resume.bin`, `favorites.bin`, `scan.bin`, `upnext.bin`, `bluetooth.bin`, `library.db` (4 MB), `covers.bin` (80 MB) and `build.tmp` (24 MB), then scans the card and builds the covers. If the card has no room it says "The card is full", and nothing is saved.

## If something goes wrong

| Symptom | Next step |
|---|---|
| Black screen, but the drives appear over USB | Copy a fixed image with the updater |
| Black screen and nothing over USB | Maskrom recovery (`docs/device-recovery.md`), then a direct maskrom write of a fixed image |
| "Update not installed" | The file isn't a valid image for this player; rebuild it and copy it again |
| "Update failed" | Copy the file again and retry before turning the player off |
| A STALL or FAULT screen | Note the values shown, then hold Power for 2 s |
