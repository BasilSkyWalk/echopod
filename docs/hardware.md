# Echo Mini hardware

A short map of the hardware EchoPod drives, for reading the code in `platform/device`. Most of this was first worked out by the [ReChord](https://github.com/brnomt/ReChord) project and then checked on a player.

## Chip and memory

- Rockchip RKnano with two Cortex-M3 cores. Core 0 runs EchoPod; core 1 runs a decoder program (FLAC or MP3) loaded on demand.
- Core 0 data RAM: `0x03004000`–`0x0304C000`. Code runs from `0x03050000` (about 157 KB).
- High RAM: `0x0307A000`–`0x0309EFFF` (148 KB). The library rescan code is loaded here when it runs.
- Core 1 RAM: `0x01000000`–`0x0105FFFF` (384 KB). It also serves as scratch memory for the rescan while playback is stopped.
- The boot ROM starts the cores at 24 MHz. EchoPod switches the PLL to 96 or 200 MHz.

## Display and keys

- 320 × 170 RGB565 panel with its own image memory, so there is no framebuffer. Two panel variants exist, each with its own init sequence.
- The backlight runs from a 10 kHz PWM on GPIO2.4.
- ▶‖, ≡, ↩ and the volume rocker sit on a resistor ladder read through the ADC. Power is a separate input.

## Storage

- 8 GB eMMC on the DW-MMC controller. From the start: the ID block, firmware slot 1, firmware slot 2, a SAVE block, then the FAT32 user volume that appears as "ECHO MINI" over USB. EchoPod never writes the ID block, the SAVE block or the gap between them.
- Each firmware slot holds the first 5 MiB of a firmware image. On the unit tested, slot 1 is at rockusb sector 0 and slot 2 at rockusb sector 69632.
- A microSD slot on the same kind of controller, with card detect. EchoPod reads and writes FAT16, FAT32 and exFAT.

## Audio

- Cirrus Logic CS43131 DAC, set up over I2C0, fed by I2S1 through DMA from a ring in data RAM.
- 3.5 mm and 4.4 mm outputs. GPIO0.13 high means a 3.5 mm plug, GPIO2.8 low means 4.4 mm.
- An LPC accelerator next to core 1 speeds up FLAC decoding.

## USB

- DesignWare USB 2.0 OTG controller, high speed. EchoPod presents both drives as a mass-storage device.
- With the buttons held at power-up, the boot ROM's maskrom mode appears as `2207:262d` (`docs/device-recovery.md`).

## Other

- **Bluetooth:** an external module with its own stack, powered from GPIO2.7 and driven over UART1 at 115,200 baud with short ASCII commands.
- **Battery:** read through the ADC. EchoPod shuts down below 3450 mV, as FiiO's firmware does.

## Firmware image

- `HIFIEC90.IMG` is a 5 MiB `RKnanoFW` container: an outer header, a table of load rows, the code and data, a copy of the first 512 bytes at `0x44C000` and a CRC-32 trailer at `0x500000`. `tools/make_image.py` builds it, and `docs/architecture.md` describes the layout.
- FiiO's firmware installs the highest-numbered `HIFIECnn.IMG` it finds at the top level of internal storage. FiiO's own images are 32 MiB.
