# Recovering a player through maskrom

If the player doesn't start at all, its chip's maskrom mode can still write FiiO's firmware back from a computer over USB. This is the last resort: the normal way back to FiiO's firmware is a file copy (`docs/user-guide.md`, "Going back to FiiO's firmware").

Maskrom writes go straight to internal storage, below the file system. A wrong command can leave the player unable to start until a later write succeeds. Follow the steps exactly, and don't run any command not listed here.

The procedure follows ReChord's Linux debrick guide and the livetrack.club debrick guide. It has been used on an Echo Mini 8 GB from macOS and from Windows 11.

## What you need

- FiiO's firmware for your model, from FiiO's official Echo Mini firmware page. Only the **8 GB** model is covered here.
- A data USB cable (not charge-only), plugged into a port on the computer itself, not a hub.
- A paperclip for the RST hole.
- The patched `rkdeveloptool` (below). Never use any other `rkdeveloptool` binary: the upstream tool doesn't know this device.

Internal storage may be wiped. If the player still shows its drive over USB, copy off anything you want to keep first.

## 1. Build the patched `rkdeveloptool` (once)

The upstream tool needs three changes for the Echo Mini: a 5000 ms command timeout, 32 sectors per transfer, and maskrom detection for USB ID `2207:262d`. `tools/patch_rkdeveloptool.py` makes them. It prints `ok: Echo Mini changes applied`, and refuses a source it doesn't recognise or one that is already patched.

### Windows

Install [MSYS2](https://www.msys2.org) with the CLANG64 packages from the README, plus `mingw-w64-clang-x86_64-autotools`, `mingw-w64-clang-x86_64-libusb` and `git`. Then, in an MSYS2 CLANG64 shell, from the repository folder:

```sh
mkdir -p external && cd external
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

The `make` line drops upstream's `-Werror`, as MSYS2's own package does. The tool prints `rkdeveloptool ver 1.32`, and needs `C:\msys64\clang64\bin` on the PATH when it runs.

### macOS

```sh
brew install libusb automake autoconf pkgconf
mkdir -p external && cd external
git clone https://github.com/rockchip-linux/rkdeveloptool.git
cd rkdeveloptool
git checkout 304f073752fd25c854e1bcf05d8e7f925b1f4e14
python3 ../../tools/patch_rkdeveloptool.py .
./autogen.sh && ./configure && make CXXFLAGS="-g -O2 -Wno-vla-cxx-extension"
./rkdeveloptool -v
```

Apple clang treats two run-time-sized arrays in upstream `main.cpp` as errors, which is why the `CXXFLAGS` are needed. On macOS the procedure was proven with ReChord's patch script, which makes the same three changes.

## 2. Enter maskrom

1. Unplug USB. Press the **RST** hole with the paperclip so the player is fully off. Don't use the Power button.
2. Hold **every button except Power**: ▶‖, ≡, ↩ and the volume rocker.
3. Still holding, plug the USB cable in. Let go after 2–3 seconds.
4. The screen stays completely black. That's expected.

### Windows only: bind maskrom to WinUSB (once per computer)

1. With the player in maskrom, Windows lists `USB\VID_2207&PID_262D` with no driver.
2. Run [Zadig](https://github.com/pbatard/libwdi/releases) (signed by Akeo Consulting) and allow the admin prompt.
3. Options → List All Devices. Pick the entry whose USB ID reads `2207` `262D` (it shows as "Unknown Device #1"). Check both numbers: replacing another device's driver breaks that device.
4. Set the target to WinUSB and click Install Driver.

Only maskrom mode uses this ID, so the player's normal USB drives are unaffected.

## 3. Check the connection

```sh
./rkdeveloptool ld
./rkdeveloptool td
```

Continue only if `ld` shows `Maskrom` and `td` says `Test Device OK.`

## 4. Write FiiO's firmware

1. Check the image. For FiiO 3.7.0 (`HIFIEC37.IMG`), `sha256` must be `eba715acbc55875fd3be4b9b1f5f58c777bca130049320043ffc09bc2866b228`.
2. Write it, then restart the player:

   ```sh
   ./rkdeveloptool wl 0 HIFIEC37.IMG
   ./rkdeveloptool rd
   ```

   `wl` writes 32 MiB in small transfers and prints about 2,000 progress lines up to 100%. Don't unplug, and don't let the computer sleep, until it finishes. `rd` prints `Reset Device OK.`

3. The player starts FiiO's firmware. If it stays black, unplug USB, press RST, then press Power. Settings → About should show the version you wrote.

Before you write, make sure no `HIFIECnn.IMG` or `ECHOMINI.IMG` file is left at the top level of internal storage if you can still see the drive. FiiO's firmware installs such a file at its first start, and that would put the old image straight back.

## Rules

- Never run `db`: it fails on this boot ROM and can wedge USB. Never write anything except the whole FiiO image: no `ul`, `ef`, `wlx`, unpacked parts or RKAF tools.
- If a command fails or hangs (more than about a minute with no progress), the session is wedged. Press Ctrl+C, unplug, press RST, enter maskrom again and start over from `ld`.
- If `wl` fails twice the same way, try another USB port, cable or computer. ReChord's recoveries were done from Linux, which is the fallback.
- A failed `wl` can leave the player unable to start until a later `wl` succeeds.

## If it goes wrong

| Symptom | Try |
|---|---|
| Nothing with `2207:262d` appears | Repeat step 2. Use a data cable and a port directly on the computer. |
| `ld` says `Loader`, not `Maskrom` | The patch didn't apply. Rebuild the tool. |
| `ld` lists nothing on Windows | Bind the device to WinUSB with Zadig (step 2). |
| `td` hangs or times out | Unplug, press RST, and start again. On Linux, disable USB autosuspend first. |
| The player won't start after `rd` | Press RST, wait 10 s, then press Power. |
