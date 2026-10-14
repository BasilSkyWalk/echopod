# Installing our firmware (Phase 3a)

Only do this after the recovery write test in `docs/device-recovery.md` has passed on the player. It passed on the test unit on 2026-10-14. If anything goes wrong, that same procedure puts stock 3.7.0 back.

## First install: through the stock updater

This is the route ReChord used (`docs/device-facts.md` 2.1). It needs the player on stock 3.7.0.

1. Build the image: `cmake --preset device && cmake --build --preset device` writes `build/device/HIFIEC90.IMG`. The `device_image_passes_the_updater_check` test confirms it's well formed.
2. Remove the microSD card from the player.
3. Plug the player in normally. The ECHO MINI drive should appear on the Mac. If it doesn't, check that the player's USB mode is set to storage, not DAC or charging only.
4. Copy the image to the root of the drive, without macOS's extra `._` file, then eject:

   ```sh
   cp -X build/device/HIFIEC90.IMG "/Volumes/ECHO MINI/"
   dot_clean -m "/Volumes/ECHO MINI"
   ls -la "/Volumes/ECHO MINI"
   diskutil eject "/Volumes/ECHO MINI"
   ```

   The listing should show `HIFIEC90.IMG` (5,242,884 bytes) and no other `.IMG` file.
5. Unplug and restart the player. The stock updater runs at start-up and installs the image.
6. If the stock screen comes back afterwards, or the screen stays black, turn the player off and on once more. A cold start runs the new image.

## What to check

- **Home screen:** "Echo Mini" with Music, Now Playing, Settings and Diagnostics. Without a card, the music lists say "No library".
- **Keys:** open Diagnostics → Key test and press every key. Note which end of the volume rocker shows as up and which as down.
- **Navigation:** the volume rocker moves, ▶‖ selects, ↩ goes back, and holding ↩ goes home.
- **Power:** a short press turns the screen off and on, and a long press turns the player off.
- **USB:** plug into the Mac. The player shows a "USB" notice, and an "Internal storage" drive appears. Ejecting that drive returns the player to the menu.

## If something goes wrong

| Symptom | Meaning | Next step |
|---|---|---|
| The screen stays black, but the Internal storage drive appears over USB | Our firmware runs; the panel is probably variant A | Tell me. The next image goes in over USB, with no maskrom |
| The screen stays black and nothing appears over USB | It didn't boot | Maskrom recovery (`docs/device-recovery.md`, "Recovery write test"), then tell me |
| The screen shows garbage or wrong colours | The panel init or window is off | Note what you see, then recover as above |
| Keys do nothing | The ADC or key ladder differs on this unit | Plug in USB to check that it's alive, then recover |

## Later updates: our own updater

1. Plug in. The player shows "USB", and the Internal storage drive appears.
2. Copy the new `HIFIEC90.IMG` to its root with `cp -X` as above, then eject Internal storage.
3. The player checks the file, installs it (showing a percentage) and asks you to unplug and turn it off and on.

An image identical to the installed one is skipped. Stock images are refused, so getting back to stock is always the maskrom procedure.
