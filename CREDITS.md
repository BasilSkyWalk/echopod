# Credits

EchoPod is licensed under GPL-3.0-or-later (`LICENSE`). It builds on the work of others:

- **[ReChord](https://github.com/brnomt/ReChord)** (GPL-3.0): the community research into the Echo Mini's hardware. Its notes, drivers and debrick guide are where most of the hardware knowledge in `docs/hardware.md`, and values such as the panel and DAC set-up, come from.
- **The livetrack.club Echo Mini debrick guide**, on which the maskrom recovery steps build.
- **[libmad](https://codeberg.org/tenacityteam/libmad)** 0.16.4, by Underbit Technologies and the Tenacity team (GPL-2.0-or-later): the MP3 decoder on the player (`third_party/libmad`).
- **[Apple Lossless Audio Codec](https://github.com/macosforge/alac)** by Apple (Apache-2.0): EchoPod's own ALAC decoder (`src/codec/alac.c`) follows its reference decoder's algorithm; no Apple code is included.
- **[FAAD2](https://github.com/knik0/faad2)** 2.11.4, by M. Bakker, Nero AG and the FAAD2 contributors (GPL-2.0-or-later): the AAC decoder on the player and in the simulator (`third_party/faad2`, built fixed-point and AAC-LC only).
- **[Opus](https://opus-codec.org)** 1.6.1, by the Xiph.Org Foundation, Skype, Octasic, Jean-Marc Valin, Timothy B. Terriberry, CSIRO, Gregory Maxwell, Mark Borgerding and Erik de Castro Lopo (BSD-3-Clause): the Opus decoder on the player and in the simulator (`third_party/opus`, built fixed-point, decoder only).
- **[dr_libs](https://github.com/mackron/dr_libs)** by David Reid (public domain or MIT-0): FLAC, MP3 and WAV decoding in the simulator (`third_party/dr_libs`).
- **[Inter](https://github.com/rsms/inter)** by Rasmus Andersson and the Inter project authors (SIL Open Font License 1.1): the UI font, pre-rendered into `src/fonts` (`assets/fonts/inter/OFL.txt`).
- **[rkdeveloptool](https://github.com/rockchip-linux/rkdeveloptool)** by Rockchip (GPL-2.0): used for maskrom recovery. It isn't included; `tools/patch_rkdeveloptool.py` patches a checkout for the Echo Mini.
- **[pid.codes](https://pid.codes)**: the USB vendor ID `1209`.

Snowsky and FiiO are trademarks of their owners. EchoPod isn't made, endorsed or supported by FiiO.
