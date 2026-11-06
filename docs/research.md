# Research notes

Open questions behind feature requests, with what the code and the other docs show so far. Anything not backed by code, a doc or a test here is marked unknown.

## Bluetooth PIN entry and numeric comparison (issue #32)

What is known:

- The Bluetooth module runs its own stack. EchoPod drives it over UART1 at 115,200 baud with short ASCII frames (`docs/hardware.md`, `platform/device/logic/bt_module.c`): `CS00` to scan, `CL06` + address to connect, `CH06` and `CT00` after timeouts, `CQ00` to poll the link, `CP00` to stop a scan, `CY0` + a level byte to start and stop the stream, and stock's `C 86`/`C 87` play notices.
- From the module EchoPod understands `CV` (ready), `CS` (scan results up to `CSEN`), `CQ` (link status), `CL`/`CH` (connect results), `CT` (disconnected), `CP`/`CC` (pause/play from the headphones) and `CA`/`CB` (previous/next). The parser also sizes a 4-byte `CK` frame but does nothing with it; what it means isn't known.
- Every frame from the module except `CS` scan results and repeated `CQ` answers, including frames EchoPod doesn't recognise (shown as `C` + the next two characters), goes into the 12-entry log on Diagnostics → Bluetooth, with the seconds between entries.
- Pairing has only been tried with headphones that pair without a PIN (OnePlus Buds 3). EchoPod sends nothing that answers a PIN or a passkey, and the code has no frame for one.

Unknown:

- Whether the module supports legacy PIN pairing or Secure Simple Pairing with numeric comparison or passkey entry at all, and if so whether it reports the request over the UART or answers it itself (for example with a fixed `0000`). Pairing this player with stock firmware to a device that asks for a PIN would show whether stock can do it at all.
- What to try next: pair such a device under EchoPod and photograph Diagnostics → Bluetooth right after the attempt. A frame EchoPod doesn't know shows up in the log. Better still, a capture of UART1's two lines (the module's TX and RX, 115,200 8N1) while stock pairs a PIN device would show the frames stock exchanges: the request the module sends, what stock answers, and in what format the PIN or the six-digit number travels. With that, EchoPod could show the number or a PIN prompt and send the answer.

## LDAC and higher SBC bitrates

What is known:

- The module takes the DAC's analogue output, not digital audio (`docs/architecture.md`, Bluetooth: Settings → Bluetooth level picks the DAC volume table for it; `docs/hardware.md`: GPIO2.0 and GPIO2.14 switch the analogue path). The module samples and encodes that signal itself.
- EchoPod's only control over the stream is `CY0` followed by one level byte (stock sends `1E` to start, `00` to stop). What the `1E` means is unknown; no other value has been tried.
- No frame in `bt_module.c` selects a codec, a bitrate or a sample rate, and none reports one.

Unknown:

- Which codecs the module supports and which one it negotiates with a given pair of headphones. A phone or computer acting as the sink (some show the codec in their Bluetooth developer settings) would show what the module sends.
- Whether the module has commands for codec or bitpool selection. If it does, they would only show up in the module's documentation or in a UART capture of stock using them; stock has no such setting that we know of.
- LDAC normally needs digital audio and a licensed encoder in the module. With the audio arriving as an analogue signal, LDAC would only be possible if the module itself supports it, which isn't known.

## DSD and DSF files

What is known:

- The DAC is a pair of Cirrus Logic CS43131 chips (one per channel) set up over I2C0 and fed by I2S1 (`docs/hardware.md`, `platform/device/logic/dac_sequence.c`). The register sequence EchoPod writes is copied from stock and only sets up the PCM path: the serial port, the PCM filter (`0x090000`), the PCM volume (`0x090001`/`0x090002`) and the PCM path controls. Nothing in the code configures a DSD input or mode.
- I2S1 runs at 44.1 to 192 kHz with 32-bit words (`platform/device/board/dac.c`); 176.4 kHz, the rate DSD64 over PCM (DoP) needs, is one of them.
- The audio engine passes 24-bit samples through unchanged on the device when the equalizer is off: the software gain stays at unity, and the equalizer is already off at 176.4 and 192 kHz (`docs/known-issues.md`). So the bits a DoP stream needs would reach the DAC intact.

Unknown:

- Whether the CS43131 recognises DoP on its PCM serial port and switches to DSD by itself, or needs DSD mode set up through registers and its separate DSD inputs, and whether those inputs are wired on this board. Neither the code nor these docs say. The CS43131 datasheet and a register dump of stock while it plays a DSF file (Diagnostics doesn't read the DAC's registers yet; a dump over I2C would be the check) would answer it.
- If DoP works, volume would have to come from the DAC's DSD path, since the PCM volume registers EchoPod uses wouldn't apply. Not tried.

Converting to PCM instead (no DAC changes needed):

- A DSF file holds 1-bit samples at 2.8224 MHz per channel for DSD64 (5.6 Mbit/s for stereo, about 706 KB/s from the card). Converting means a low-pass filter and decimation, by 32 to 88.2 kHz or by 16 to 176.4 kHz. A table-driven filter that looks up 8 input bits at a time needs a few kilobytes of tables and roughly one table lookup and add per 8 input bits per filter tap group; at 88.2 kHz output that is on the order of a few million lookups a second for stereo. That is an estimate, not a measurement: whether it fits on core 1 next to the reading, and whether the card keeps up with 706 KB/s while the screen draws, would have to be measured on the player.
- DSD128 doubles both figures. No DSF parser or converter exists in the code yet.
