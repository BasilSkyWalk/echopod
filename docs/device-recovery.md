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
./autogen.sh && ./configure && make
./rkdeveloptool -v
```

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
