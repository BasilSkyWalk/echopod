# Noto Sans CJK SC

The Chinese, Japanese and Korean glyph packs (`src/fonts/packs/cjk_16.bin` and `cjk_12.bin`) are rendered from Noto Sans CJK SC Regular, version 2.004, by `tools/glyph_pack.py`. The font is licensed under the SIL Open Font License 1.1 (`OFL.txt`), and so are the packs made from it.

The font file is 16 MB, so it isn't in the repository: the packs are. To rebuild them, download it here first:

- URL: https://raw.githubusercontent.com/notofonts/noto-cjk/Sans2.004/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf
- Size: 16,437,364 bytes
- SHA-256: `2c76254f6fc379fdddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`

```sh
curl -L -o assets/fonts/noto-cjk/NotoSansCJKsc-Regular.otf \
  https://raw.githubusercontent.com/notofonts/noto-cjk/Sans2.004/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf
.venv/bin/python tools/glyph_pack.py --config assets/fonts/glyph_pack.json --output src/fonts/packs
```

Without the font, `glyph_pack.py` skips the two CJK faces and leaves the committed packs as they are.
