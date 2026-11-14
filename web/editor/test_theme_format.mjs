import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { blankRows, paintedRows, resizedRows, rowsFromPixels } from "./icon_editor.js";
import { coveredCodes } from "./font_editor.js";
import { effectivePlaces, placeAt } from "./layout_editor.js";
import { dimmed, fittedSize, quantise } from "./wallpaper_editor.js";
import {
  ThemeError,
  assetMemory,
  checksum,
  contrast,
  defaultTheme,
  hexColor,
  lint,
  pack,
  rgb565,
  toToml,
  unpack,
} from "./theme_format.js";

const site = process.argv[2];
const schema = JSON.parse(readFileSync(join(site, "theme_schema.json"), "utf-8"));
const packages = readdirSync(join(site, "themes")).filter((name) => name.endsWith(".echotheme"));
const warnings = process.argv[3] ? JSON.parse(readFileSync(process.argv[3], "utf-8")) : null;

assert.ok(packages.length >= 2);
for (const name of packages) {
  const bytes = new Uint8Array(readFileSync(join(site, "themes", name)));
  const theme = unpack(bytes, schema);
  assert.deepEqual(Array.from(pack(theme, schema)), Array.from(bytes), `${name} repacks to itself`);
  if (warnings) {
    const expected = warnings[name.replace(".echotheme", "")];
    assert.deepEqual(lint(theme, schema), expected, `${name} gets the packer's warnings`);
  }
}

const dark = new Uint8Array(readFileSync(join(site, "themes", "dark.echotheme")));
const fresh = defaultTheme(schema);
fresh.name = "Dark";
assert.deepEqual(Array.from(pack(fresh, schema)), Array.from(dark), "the editor's starting theme is Dark");

const ember = unpack(new Uint8Array(readFileSync(join(site, "themes", "ember.echotheme"))), schema);
assert.equal(ember.name, "Ember");
assert.equal(ember.spacing.row_height, 28);
assert.equal(ember.spacing.scrollbar_width, null);
assert.equal(ember.boxes.toast.radius, 8);
assert.equal(ember.boxes.selection.border_edges, 4);
assert.equal(ember.colors.selection_top, "#ffa231");

assert.equal(checksum(new Uint8Array([])), 0x811c9dc5);
assert.equal(checksum(new TextEncoder().encode("a")), 0xe40c292c);
assert.equal(rgb565("#f2f2f5"), 0xf79e);
assert.equal(rgb565(hexColor(0x1082)), 0x1082);
assert.equal(contrast("#ffffff", "#000000"), 21);
assert.equal(contrast("#101114", "#101114"), 1);
assert.deepEqual(lint(fresh, schema), []);
const faint = { ...fresh, colors: { ...fresh.colors, text: fresh.colors.background } };
assert.deepEqual(lint(faint, schema).slice(0, 1), ["List text: contrast 1.0, below 4.5"]);

const edited = { ...fresh, name: "x".repeat(24) };
assert.throws(() => pack(edited, schema), ThemeError);
assert.throws(() => pack({ ...fresh, spacing: { ...fresh.spacing, row_height: 41 } }, schema), ThemeError);
assert.throws(() => unpack(dark.subarray(0, dark.length - 1), schema), ThemeError);
const flipped = dark.slice();
flipped[60] ^= 1;
assert.throws(() => unpack(flipped, schema), ThemeError);

assert.equal(fresh.icons.battery, null);
assert.equal(ember.icons.battery.length, 10);
assert.equal(ember.icons.battery[0].length, 22);
assert.equal(ember.icons.battery_level_large.length, 14);
const drawn = { ...fresh, icons: { ...fresh.icons, status_pause: ["f.8", ".1f"], chevron: ["0f"] } };
const drawnBytes = pack(drawn, schema);
assert.equal(drawnBytes[23], 7);
assert.equal(drawnBytes.length, dark.length + 7 * 4 + 5);
assert.deepEqual(Array.from(drawnBytes.subarray(dark.length + 4, dark.length + 8)), [3, 2, 0, 0]);
assert.deepEqual(Array.from(drawnBytes.subarray(-5)), [0xf0, 0x80, 0x01, 0xf0, 0x0f]);
const redrawn = unpack(drawnBytes, schema);
assert.deepEqual(redrawn.icons.status_pause, ["f.8", ".1f"]);
assert.deepEqual(redrawn.icons.chevron, [".f"]);
assert.equal(redrawn.icons.status_play, null);
const lone = { ...fresh, icons: { ...fresh.icons, battery: ["ff"] } };
assert.throws(() => pack(lone, schema), /go together/);
const uneven = { ...lone, icons: { ...lone.icons, battery_level: ["fff"] } };
assert.throws(() => pack(uneven, schema), /go together/);
assert.throws(() => pack({ ...fresh, icons: { ...fresh.icons, status_bolt: ["fffffff"] } }, schema), ThemeError);
assert.throws(() => pack({ ...fresh, icons: { ...fresh.icons, status_bolt: ["fg"] } }, schema), ThemeError);
assert.throws(() => pack({ ...fresh, icons: { ...fresh.icons, status_bolt: ["ff", "f"] } }, schema), ThemeError);
const drawnSource = toToml(drawn, schema).split(String.fromCharCode(10));
const iconsAt = drawnSource.indexOf("[icons]");
assert.deepEqual(drawnSource.slice(iconsAt + 1, iconsAt + 5), ["status_pause = [", '    "f.8",', '    ".1f",', "]"]);

assert.deepEqual(blankRows(3, 2), ["...", "..."]);
assert.deepEqual(resizedRows(["f8", "1."], 3, 1), ["f8."]);
assert.deepEqual(resizedRows(["f8"], 1, 2), ["f", "."]);
assert.deepEqual(paintedRows(["...", "..."], 1, 1, 15), ["...", ".f."]);
assert.deepEqual(paintedRows(["fff"], 2, 0, 0), ["ff."]);
const clearPixels = new Uint8Array([9, 9, 9, 0, 9, 9, 9, 255, 9, 9, 9, 136, 9, 9, 9, 17]);
assert.deepEqual(rowsFromPixels(clearPixels, 2, 2), [".f", "81"]);
const inkPixels = new Uint8Array([0, 0, 0, 255, 255, 255, 255, 255]);
assert.deepEqual(rowsFromPixels(inkPixels, 2, 1), ["f."]);

const tile = { palette: ["#000000", "#ffffff", "#ff0000"], rows: ["0120", "2100"] };
assert.equal(fresh.wallpapers.wallpaper, null);
const papered = pack({ ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: tile } }, schema);
const paperedView = new DataView(papered.buffer);
const checked = paperedView.getUint32(8, true);
assert.equal(paperedView.getUint16(6, true), 52);
assert.equal(checked, dark.length + 4 + 16);
assert.equal(papered.length, checked + 32 + 4);
assert.equal(papered[48], 1);
assert.equal(paperedView.getUint32(12, true), checksum(papered.subarray(16, checked)));
assert.equal(paperedView.getUint32(checked - 8, true), checked);
assert.equal(paperedView.getUint32(checked - 4, true), checksum(papered.subarray(checked)));
assert.deepEqual(Array.from(papered.subarray(-4)), [0x01, 0x20, 0x21, 0x00]);
assert.deepEqual(unpack(papered, schema).wallpapers.wallpaper, tile);
assert.deepEqual(unpack(papered, schema).colors, unpack(dark, schema).colors);
const both = { ...fresh, wallpapers: { wallpaper: tile, wallpaper_portrait: tile } };
assert.deepEqual(unpack(pack(both, schema), schema).wallpapers, both.wallpapers);
const spoilt = papered.slice();
spoilt[spoilt.length - 1] ^= 1;
assert.deepEqual(unpack(spoilt, schema).wallpapers.wallpaper.rows, ["0120", "2101"]);
assert.throws(() => unpack(papered.subarray(0, checked - 1), schema), ThemeError);
for (const rows of [["012"], ["01", "0120"], ["03"], ["0".repeat(322)], Array(171).fill("00")]) {
  const bad = { ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: { ...tile, rows } } };
  assert.throws(() => pack(bad, schema), ThemeError);
}
assert.equal(ember.wallpapers.wallpaper.rows.length, 16);
assert.equal(ember.wallpapers.wallpaper.palette.length, 3);
const paperSource = toToml({ ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: tile } }, schema);
const paperLines = paperSource.split(String.fromCharCode(10));
const paperAt = paperLines.indexOf("[wallpaper]");
assert.deepEqual(paperLines.slice(paperAt + 1, paperAt + 6), [
  'palette = ["#000000", "#ffffff", "#ff0000"]',
  "rows = [",
  '    "0120",',
  '    "2100",',
  "]",
]);
const pale = { palette: ["#101114", "#e8e8e8"], rows: Array(10).fill("0000000001") };
const paler = { ...pale, rows: Array(10).fill("0000000011") };
assert.deepEqual(lint({ ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: pale } }, schema), []);
assert.match(
  lint({ ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: paler } }, schema)[0],
  /^List text over the wallpaper: contrast 1\.\d, below 3\.0$/,
);

assert.deepEqual(fittedSize({ width: 1000, height: 400 }, schema.wallpapers[0], false), { width: 320, height: 170 });
assert.deepEqual(fittedSize({ width: 33, height: 20 }, schema.wallpapers[0], true), { width: 32, height: 20 });
assert.deepEqual(fittedSize({ width: 33, height: 20 }, schema.wallpapers[0], false), { width: 320, height: 170 });
assert.deepEqual(fittedSize({ width: 400, height: 20 }, schema.wallpapers[1], true), { width: 170, height: 320 });
assert.deepEqual(Array.from(dimmed(new Uint8ClampedArray([200, 100, 0, 7]), "#000000", 50)), [100, 50, 0, 255]);
assert.deepEqual(Array.from(dimmed(new Uint8ClampedArray([200, 100, 0, 255]), "#ffffff", 0)), [200, 100, 0, 255]);
const flat = new Uint8ClampedArray(8 * 4 * 4).fill(255);
assert.deepEqual(quantise(flat, 8, 4), { palette: ["#ffffff"], rows: Array(4).fill("00000000") });
const halves = new Uint8ClampedArray(8 * 2 * 4);
for (let at = 0; at < halves.length; at += 4) {
  halves.set((at / 4) % 8 < 4 ? [0, 0, 0, 255] : [255, 0, 0, 255], at);
}
const cut = quantise(halves, 8, 2);
assert.deepEqual(cut.palette.toSorted(), ["#000000", "#ff0000"]);
assert.equal(cut.rows[0].slice(0, 4), cut.rows[1].slice(0, 4));
assert.notEqual(cut.rows[0][0], cut.rows[0][7]);
const ramp = new Uint8ClampedArray(64 * 64 * 4);
for (let at = 0; at < ramp.length; at += 4) {
  ramp.set([(at / 4) % 64 * 4, Math.floor(at / 256) * 4, 128, 255], at);
}
const many = quantise(ramp, 64, 64);
assert.equal(many.palette.length, 16);
assert.ok(many.palette.every((color) => rgb565(color) === rgb565(hexColor(rgb565(color)))));
assert.equal(pack({ ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: many } }, schema).length, dark.length + 20 + 32 + 32 * 64);

const letters = {
  ascent: 3,
  descent: 1,
  glyphs: [
    { char: "B", left: 1, top: 3, advance: 4, rows: ["f8", "1.", "ff"] },
    { char: "A", left: -1, top: 2, advance: 5, rows: ["fff"] },
    { char: " ", left: 0, top: 0, advance: 2, rows: [] },
  ],
};
assert.equal(fresh.fonts.body, null);
const fonted = pack({ ...fresh, fonts: { ...fresh.fonts, body: letters } }, schema);
const fontedView = new DataView(fonted.buffer);
const fontCore = fontedView.getUint32(8, true);
assert.equal(fonted[48], 1);
assert.deepEqual(Array.from(fonted.subarray(fontCore - 16, fontCore - 8)), [3, 3, 3, 0, 5, 0, 1, 0]);
assert.equal(fontedView.getUint32(fontCore - 8, true), fontCore);
assert.equal(fontedView.getUint32(fontCore - 4, true), checksum(fonted.subarray(fontCore)));
assert.deepEqual(Array.from(fonted.subarray(fontCore, fontCore + 10)), [32, 0, 0, 0, 0, 0, 0, 0, 2, 0]);
assert.deepEqual(Array.from(fonted.subarray(fontCore + 10, fontCore + 20)), [65, 0, 0, 0, 3, 1, 255, 2, 5, 0]);
assert.deepEqual(Array.from(fonted.subarray(-5)), [0xff, 0xf0, 0xf8, 0x10, 0xff]);
const unpacked = unpack(fonted, schema).fonts.body;
assert.deepEqual(unpacked, { ...letters, glyphs: [letters.glyphs[2], letters.glyphs[1], letters.glyphs[0]] });
assert.equal(assetMemory({ ...fresh, fonts: { ...fresh.fonts, body: letters } }, schema), 32 + 8);
const tooTall = { ...letters, ascent: 17, descent: 4 };
assert.throws(() => pack({ ...fresh, fonts: { ...fresh.fonts, body: tooTall } }, schema), ThemeError);
assert.throws(() => pack({ ...fresh, fonts: { ...fresh.fonts, small: { ...letters, ascent: 12, descent: 4 } } }, schema), ThemeError);
const twice = { ...letters, glyphs: [...letters.glyphs, letters.glyphs[0]] };
assert.throws(() => pack({ ...fresh, fonts: { ...fresh.fonts, body: twice } }, schema), ThemeError);
const heavy = { ...letters, glyphs: Array.from({ length: 9 }, (_, index) => ({ char: String.fromCharCode(33 + index), left: 0, top: 0, advance: 1, rows: Array(40).fill("f".repeat(40)) })) };
const screen = { palette: ["#000000"], rows: Array(170).fill("0".repeat(320)) };
pack({ ...fresh, fonts: { ...fresh.fonts, body: heavy } }, schema);
assert.throws(
  () => pack({ ...fresh, fonts: { ...fresh.fonts, body: heavy }, wallpapers: { ...fresh.wallpapers, wallpaper: screen } }, schema),
  /28,224/,
);
const fontLines = toToml({ ...fresh, fonts: { ...fresh.fonts, small: letters } }, schema).split(String.fromCharCode(10));
const fontAt = fontLines.indexOf("[fonts.small]");
assert.deepEqual(fontLines.slice(fontAt + 1, fontAt + 5), [
  "ascent = 3",
  "descent = 1",
  "glyphs = [",
  '    { char = "B", left = 1, top = 3, advance = 4, rows = ["f8", "1.", "ff"] },',
]);
const inter = readFileSync(new URL("../../assets/fonts/inter/Inter-SemiBold.ttf", import.meta.url));
const covered = coveredCodes(inter.buffer.slice(inter.byteOffset, inter.byteOffset + inter.byteLength), schema.font_ranges);
assert.equal(covered.length, 108);
assert.ok(covered.includes(65) && covered.includes(0x2026) && !covered.includes(0x4e00));

assert.equal(fresh.layouts.now_playing.cover, null);
const arranged = {
  ...fresh,
  layouts: {
    now_playing: { ...fresh.layouts.now_playing, cover: { x: -4, y: 30, size: 64 }, title: { x: 10, y: 96, width: 300, height: 22, align: "right" } },
    now_playing_portrait: { ...fresh.layouts.now_playing_portrait, progress: { hide: true } },
  },
};
const arrangedBytes = pack(arranged, schema);
const arrangedView = new DataView(arrangedBytes.buffer);
assert.equal(arrangedView.getUint16(6, true), 52);
assert.deepEqual(Array.from(arrangedBytes.subarray(48, 51)), [0, 10, 10]);
assert.equal(arrangedBytes.length, dark.length + 4 + 2 * 10 * 8);
const placesAt = 52 + 2 * schema.colors.length + 11 * schema.boxes.length + schema.spacing.length;
assert.equal(arrangedView.getInt16(placesAt, true), -4);
assert.deepEqual(Array.from(arrangedBytes.subarray(placesAt + 4, placesAt + 8)), [64, 0, 0, 0]);
assert.deepEqual(Array.from(arrangedBytes.subarray(placesAt + 14, placesAt + 16)), [22, 2]);
assert.equal(arrangedBytes[placesAt + 8 * 2 + 7], 255);
const rearranged = unpack(arrangedBytes, schema);
assert.deepEqual(rearranged.layouts, arranged.layouts);
assert.deepEqual(Array.from(pack(rearranged, schema)), Array.from(arrangedBytes));
assert.throws(() => pack({ ...fresh, layouts: { ...fresh.layouts, now_playing: { ...fresh.layouts.now_playing, cover: { x: 0, y: 0, size: 171 } } } }, schema), ThemeError);
assert.throws(() => pack({ ...fresh, layouts: { ...fresh.layouts, now_playing: { ...fresh.layouts.now_playing, title: { x: 0, y: 0, width: 10, align: "middle" } } } }, schema), ThemeError);
const placeLines = toToml(arranged, schema).split(String.fromCharCode(10));
const titleAt = placeLines.indexOf("[now_playing.title]");
assert.deepEqual(placeLines.slice(titleAt + 1, titleAt + 6), ["x = 10", "y = 96", "width = 300", "height = 22", 'align = "right"']);
assert.deepEqual(placeLines.slice(placeLines.indexOf("[now_playing.cover]") + 1, placeLines.indexOf("[now_playing.cover]") + 4), ["x = -4", "y = 30", "size = 64"]);
assert.equal(placeLines[placeLines.indexOf("[now_playing_portrait.progress]") + 1], "hide = true");
const builtin = schema.layout.map((entry, index) => ({ x: index * 10, y: 0, width: 5, height: 5, align: 1 }));
const effective = effectivePlaces(builtin, arranged.layouts.now_playing, schema);
assert.deepEqual(effective[0], { entry: schema.layout[0], given: true, x: -4, y: 30, size: 64 });
assert.deepEqual(effective[2], { entry: schema.layout[2], given: false, x: 20, y: 0, width: 5, height: 5, align: "center" });
assert.equal(placeAt(effective, 0, 40).entry.name, "cover");
assert.equal(placeAt(effective, 22, 2).entry.name, "artist");
assert.equal(placeAt(effective, 400, 400), null);

const source = toToml(ember, schema);
assert.match(source, /^name = "Ember"$/m);
assert.match(source, /^row_height = 28$/m);
assert.match(source, /^\[boxes\.toast\]$/m);
assert.match(source, /^radius = 8$/m);
console.log(`ok: ${packages.length} themes repack to themselves`);
