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

- DesignWare USB 2.0 OTG controller, high speed. EchoPod presents both drives as a mass-storage device, or, in USB DAC mode, a full-speed USB Audio Class 1.0 sound card (untested on hardware). The core's FIFO RAM is small: at reset, RX 147 words and TX FIFO 0 128 words.
- With the buttons held at power-up, the boot ROM's maskrom mode appears as `2207:262d` (`docs/device-recovery.md`).

## Other

- **Bluetooth:** an external module with its own stack, powered from GPIO2.7 and driven over UART1 at 115,200 baud with short ASCII commands. Stock streams to OnePlus Buds 3 on this unit; EchoPod pairs, connects and, since 2026-10-29, streams to them too (see the end of this entry). Found on the player (2026-10-28, nothing in either jack, music playing): GPIO2.0 high with GPIO2.14 low (stock's connect step), both low, and GPIO2.0 low with GPIO2.14 high are all silent in the headphones; with GPIO2.14 low and GPIO2.0 low a 3.5 mm headphone still plays, so GPIO2.14 doesn't cut the wired output, and only GPIO2.0 high mutes it; and switching GPIO2.0/2.14 moves the jack-detect readings enough to fake an unplug, so `board/jack.c` ignores the jack pins while audio is routed to Bluetooth and for 500 ms after each routing change; readings taken while routed fake a plugged headphone. Also silent: stock's play-state rules for the `C 86`/`C 87` notices, pausing `CQ00` after the module's `CP`/`CC` reports and answering them with `CY0 00`/`CY0 1E`, driving GPIO0.14 high as stock's boot leaves it, and restarting the DAC path with the routing pins already set (the DAC starts normally with GPIO2.14 held low). The `CP`/`CC` flipping seen with the Buds out of the ears stops with them in. Also silent with the module powered and streaming before the DAC path starts (stock's order), and with Gain on High. Register dumps taken while streaming match stock's predicted state on every audio-path register: I2S1, the ACODEC words, the GRF pin groups, GPIO1.0–1.4, GPIO2.0/2.2/2.14 and the audio clock bits, and on both CS43131s every register except headphone output control `080000` (Gain) and PCM volume. The remaining differences are boot housekeeping (CRU gate and reset images, display, UART2 and pull settings). **Bluetooth audio played on 2026-10-29** once EchoPod, on routing to the module, also applied the stock start-up settings it had skipped: GRF `+0xC4` = `0x00010000` (bit 0 cleared), CRU `+0x84` = `0x00920092` and `+0xA0` = `0x00020002` (gates `0x11`, `0x14`, `0x17` and `0x81` off), CRU `+0xC0` = `0x00680068` (I2S0 and SPI0/1 held in reset), GPIO2.9 driven low, the GPIO0.13 and GPIO2.8 pulls set as stock sets them (GRF `+0x14`, `+0x54`), and `CY0 00` on connect before `CY0 1E`. Which of these is needed hasn't been narrowed down yet.
- **Battery:** read through the ADC. EchoPod shuts down below 3450 mV, as FiiO's firmware does.

## Firmware image

- `HIFIEC90.IMG` is a 5 MiB `RKnanoFW` container: an outer header, a table of load rows, the code and data, a copy of the first 512 bytes at `0x44C000` and a CRC-32 trailer at `0x500000`. `tools/make_image.py` builds it, and `docs/architecture.md` describes the layout.
- FiiO's firmware installs the highest-numbered `HIFIECnn.IMG` it finds at the top level of internal storage. FiiO's own images are 32 MiB.
