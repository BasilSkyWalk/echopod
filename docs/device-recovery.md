# Device recovery dry run (Phase 2)

Before any of our code goes near the player, we prove that its recovery path works on the test unit. If something we install later fails to boot, maskrom recovery is how the player comes back.

This run changes nothing on the device. It only checks that the Mac can see the player in maskrom and talk to it. There are no write commands here, and none should be added until this run has passed.

Based on ReChord's `docs/DEBRICK-LINUX.md` (verified on Ubuntu, 2026-08-24) and the livetrack.club debrick guide. On macOS the tool and the procedure are untested; that's what this run finds out.

## 1. Before you start

1. On the stock firmware, note the **firmware version** and the **storage size** (8 GB or 512 MB model). FiiO publishes separate images for each, and the recovery image must match.
2. Connect the player over USB and **copy everything off its internal storage**. Some upgrades format it.
3. Download the matching stock firmware zip from FiiO's official Echo Mini firmware page and keep it somewhere safe. Don't install it; it's the restore image for later.

## 2. Build the patched `rkdeveloptool` (once)

The upstream tool needs three Echo Mini fixes: a finite USB timeout, 32-sector transfers, and maskrom detection for USB ID `2207:262d`. ReChord's script applies them.

```sh
brew install libusb automake autoconf pkgconf
cd ~/echo-mini-fw/external
git clone https://github.com/rockchip-linux/rkdeveloptool.git
cd rkdeveloptool
bash ../ReChord/tools/patch-rkdeveloptool-echo-mini.sh
./autogen.sh && ./configure && make CXXFLAGS="-g -O2 -Wno-vla-cxx-extension"
./rkdeveloptool -v
```

Apple clang 17 treats two run-time-sized arrays in upstream `main.cpp` as errors, which is why the `CXXFLAGS` are needed. The linker warns that libusb was built for a newer macOS; that's harmless. Verified on 2026-10-14: it prints `rkdeveloptool ver 1.32`.

The script should print `ok: Echo Mini patches applied`. Never use any other `rkdeveloptool` binary (for example `community/rkbin/tools/rkdeveloptool` in ReChord): it doesn't know this device.

## 3. Enter maskrom

1. Unplug USB. Press the **RST** hole with a paperclip so the player is fully off. Don't use the Power button.
2. Hold **every button except Power**: ▶‖, ≡, ↩ and the volume rocker. ReChord and livetrack only say "all buttons except power", so it isn't known which key actually matters. If the rocker can't be pressed at both ends at once, hold one end, and note which one you held.
3. Still holding, plug the USB cable into the Mac. Let go after a couple of seconds.
4. The screen stays completely black. That's expected.

## 4. Check, and stop

```sh
system_profiler SPUSBDataType | grep -i -B2 -A8 0x262d
cd ~/echo-mini-fw/external/rkdeveloptool
./rkdeveloptool ld
./rkdeveloptool td
```

| Command | Expected |
|---|---|
| `system_profiler` | a device with Product ID `0x262d`, Vendor ID `0x2207` |
| `ld` | `DevNo=1 Vid=0x2207,Pid=0x262d,... Maskrom` |
| `td` | `Test Device OK.` |

**Stop there.** Don't run any other command: not `db` (it fails on this boot ROM and can wedge USB), not `wl`, `ef`, `ul`, `cs` or anything else.

## 5. Leave maskrom

Unplug USB, press RST with the paperclip, then power on as usual. The player should start the stock firmware exactly as before, with your files intact.

## 6. Report

Send back:
- the firmware version and storage size from step 1
- the output of the three commands in step 4
- whether the player started normally afterwards

## If it goes wrong

| Symptom | Try |
|---|---|
| Nothing with `0x262d` appears | Repeat step 3. Use a data cable (not charge-only) and a port directly on the Mac, not a hub. |
| `ld` says `Loader`, not `Maskrom` | The patch didn't apply. Re-run the script and `make`. |
| `ld` lists nothing, but `system_profiler` shows the device | Note it and stop. It may be a macOS USB permission or driver issue. |
| `td` hangs or times out | Unplug, press RST, and stop. ReChord's Linux guide disables USB autosuspend, which has no macOS switch; a Linux machine or live USB may be needed. |
| The player won't start normally after step 5 | Press RST, wait 10 s, try Power again. If it still won't start, stop and tell me before doing anything else. |

# Recovery write test (Phase 2b)

The dry run showed the Mac can talk to the player in maskrom. This part proves the actual recovery: writing FiiO's stock 3.7.0 firmware back through maskrom. That's the same firmware the player runs now, so a success leaves it as it was. Do this before any of our images is installed, because getting back from ours depends on it.

The sequence (`ld`, `td`, `wl 0`, `rd`) is the one ReChord used for every recovery, on Ubuntu (`docs/device-facts.md` 2.3). Verified on macOS with the test unit on 2026-10-14: `wl` prints about 2,049 progress lines up to 100%, and `rd` prints `Reset Device OK.`

## 1. Before you start

1. Internal storage may be wiped. Copy anything you want to keep off the player first (it was empty on 2026-10-14).
2. Remove the microSD card.
3. Check the stock image. The hash must match exactly:

   ```sh
   shasum -a 256 ~/echo-mini-blobs/stock-3.7.0/"ECHO MINI V3.7.0"/HIFIEC37.IMG
   ```

   Expected: `eba715acbc55875fd3be4b9b1f5f58c777bca130049320043ffc09bc2866b228`. If it differs, stop.
4. Use the same cable and Mac port as the dry run, connected directly, not through a hub. Keep the Mac awake and plugged in.

## 2. Enter maskrom and check

Same as in the dry run:
1. Press RST about 1 s and release.
2. Hold every button except Power, plug in USB, and release after 2–3 s.

Then run:

```sh
cd ~/echo-mini-fw/external/rkdeveloptool
./rkdeveloptool ld
./rkdeveloptool td
```

Continue only if `ld` says `Maskrom` and `td` says `Test Device OK.`

## 3. Write the stock image

```sh
./rkdeveloptool wl 0 ~/echo-mini-blobs/stock-3.7.0/"ECHO MINI V3.7.0"/HIFIEC37.IMG
```

- It writes 32 MiB in small transfers, so it may take a while. Don't unplug, don't let the Mac sleep, and don't run anything else until it finishes.
- Save the full output, the last lines especially.

If it succeeded:

```sh
./rkdeveloptool rd
```

## 4. Check

The player should restart into stock firmware. If it stays black after `rd`, unplug USB, press RST, then press Power. Then:
- Settings shows version 3.7.0.
- Over USB, the ECHO MINI drive appears on the Mac.

## Rules

- Never run `db`. Never write anything except the whole stock image (no `ul`, `ef`, `wlx`, unpacked parts or RKAF tools).
- If any command fails or hangs (more than about a minute with no progress, or an error), the session is wedged:
  1. Press Ctrl+C, unplug, and press RST.
  2. Enter maskrom again and start over from `ld`.
  3. If `wl` fails twice the same way, stop and send me the output. The fallback is the same commands from a Linux machine or live USB, which is what ReChord's recoveries used.
- A failed `wl` can leave the player unable to start until a later `wl` succeeds. That's expected, and it's why this is tested now, with the known-good stock image, rather than after one of ours.

## Report

- the output of `ld`, `td`, `wl` and `rd`
- whether the player started stock 3.7.0 afterwards, and whether the drive appears over USB

# Maskrom from Windows

Verified on a Windows 11 laptop on 2026-10-22: `ld` reported Maskrom, `td` said `Test Device OK.`, and `rl` read the first sector of both firmware slots (rockusb 0 and 69632), each holding the running image's header.

## 1. Build the patched `rkdeveloptool` (once)

Needs MSYS2 with the CLANG64 packages from `NOTES.md` (Build facts) plus `mingw-w64-clang-x86_64-autotools`, `mingw-w64-clang-x86_64-libusb` and `git`. In an MSYS2 CLANG64 shell:

```sh
cd /d/Projects/echo-mini-fw/external
git clone https://github.com/rockchip-linux/rkdeveloptool.git
cd rkdeveloptool
git checkout 304f073752fd25c854e1bcf05d8e7f925b1f4e14
python ../../tools/patch_rkdeveloptool.py .
autoreconf -fi
mkdir -p build-clang64 && cd build-clang64
../configure --prefix=/clang64
make AM_CPPFLAGS='-Wall -Wextra -Wreturn-type -fno-strict-aliasing -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE $(LIBUSB1_CFLAGS)'
./rkdeveloptool -v
```

`tools/patch_rkdeveloptool.py` makes the three changes in `docs/device-facts.md` 2.3: a 5000 ms command timeout, 32 sectors per transfer, and `2207:262d` listed as an RKnano device and classified by bcdDevice (`0x0100` in maskrom). It prints `ok: Echo Mini changes applied`, and it refuses a source it doesn't recognise or one that is already patched. The `make` line drops upstream's `-Werror`, as MSYS2's own package does. The tool prints `rkdeveloptool ver 1.32` and needs `C:\msys64\clang64\bin` on the PATH at run time (libusb and libc++).

## 2. Bind maskrom to WinUSB (once per PC)

1. Enter maskrom (section 3 above). The screen stays black, and Windows lists `USB\VID_2207&PID_262D` with no driver.
2. Run Zadig 2.9 (from https://github.com/pbatard/libwdi/releases, signed by Akeo Consulting) and allow the admin prompt.
3. Options → List All Devices. Pick the entry whose USB ID reads `2207` `262D` (it shows as "Unknown Device #1"). Check both numbers: replacing another device's driver breaks that device.
4. The target must be WinUSB. Click Install Driver.

Only maskrom mode uses this ID, so the player's normal USB drives are unaffected.

## 3. Check

```sh
./rkdeveloptool ld
./rkdeveloptool td
./rkdeveloptool rl 0 1 slot1.bin
./rkdeveloptool rl 69632 1 slot2.bin
```

Expect `Maskrom`, `Test Device OK.` and two 512-byte files whose bytes `0x1F8`–`0x1FF` read `RKnanoFW`. The same rules as on the Mac apply: never `db`.
