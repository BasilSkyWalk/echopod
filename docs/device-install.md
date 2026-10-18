# Installing our firmware

Recovery has been confirmed on the test unit (`docs/device-recovery.md`): maskrom can write the stock 3.7.0 image back. Every route below falls back to that.

## First install: a direct maskrom write

On the test unit the stock 3.7.0 updater shows "Updating" for under a second, deletes the file and keeps stock. It did this for our image, for RECHO 0.9.2, and even for FiiO's own image. So the first install writes our image straight into both firmware slots. Each slot holds the image's first `0x500000` bytes (`docs/device-facts.md` 2.2). On this unit rockusb LBA = raw − `0x2000`, so slot 1 is rockusb 0 and slot 2 is rockusb `0x11000` = 69632 (2.3.1).

1. Build: `cmake --preset device && cmake --build --preset device`. Then cut the slot contents:

   ```sh
   head -c 5242880 build/device/HIFIEC90.IMG > build/device/slot.bin
   ```

2. Enter maskrom: unplug, press RST, hold every button except Power, plug in, then let go after 2–3 s.
3. Write slot 2, then slot 1, and read both back:

   ```sh
   cd external/rkdeveloptool
   ./rkdeveloptool ld && ./rkdeveloptool td
   ./rkdeveloptool wl 69632 ../../build/device/slot.bin
   ./rkdeveloptool wl 0 ../../build/device/slot.bin
   ./rkdeveloptool rl 0 1 /tmp/s1.bin && ./rkdeveloptool rl 69632 1 /tmp/s2.bin
   ```

   Both read-backs must equal the first 512 bytes of `slot.bin`.
4. `./rkdeveloptool rd`. The player starts our firmware.

## Later updates: our own updater

1. Plug the player into the Mac. It shows "USB", and the internal-storage drive ("ECHO MINI") mounts.
2. Copy the new image to the root without macOS's side file, and check it:

   ```sh
   cp -X build/device/HIFIEC90.IMG "/Volumes/ECHO MINI/"
   rm -f "/Volumes/ECHO MINI/._HIFIEC90.IMG"
   shasum -a 256 build/device/HIFIEC90.IMG "/Volumes/ECHO MINI/HIFIEC90.IMG"
   ```

   5 MiB takes about 30 s, because writes run at about 180 KB/s.
3. Eject the drive and leave the cable in. The player shows "Checking HIFIEC90.IMG", then "Installing update" with a percentage, then "Update installed".
4. Press a key, unplug, hold Power to switch off, then switch on. The new build starts.

The updater checks the image's shape, rows, entry vector, header copy and CRC before writing. It skips an image that is already installed and refuses stock images. It writes slot 2 first, then slot 1, and reads back every chunk.

## What to check after an install

- **Home screen:** "Echo Mini" with Music, Now Playing, Settings and Diagnostics. Without a card, Music says "No library".
- **Keys:** Diagnostics → Key test. The rocker's up end should register as up.
- **Power:** a short press turns the screen off and on. A long press switches the player off, but only with the cable unplugged; with USB connected the player stays powered.
- **USB:** the "USB" notice appears and the drive mounts within about 10 s.
- **Saving:** the card needs `/.echo/settings.bin`, `resume.bin` and `favorites.bin`, made once with `tools/save_files.py --echo /Volumes/<card>/.echo`. Without them nothing is saved, and adding a favorite says "Could not save favorites".

## If something goes wrong

| Symptom | Next step |
|---|---|
| Black screen, but the drive appears over USB | Copy a fixed image with the updater |
| Black screen and nothing over USB | Maskrom recovery, then a direct maskrom write of a fixed image |
| "Update not installed" | The file isn't a valid image for this player; rebuild it and copy it again |
| "Update failed" | Copy the file again and retry before turning the player off; one slot is always complete |
| USB shows the "USB 1/4"–"4/4" screens | The USB controller failed to start; note the values for diagnosis |
