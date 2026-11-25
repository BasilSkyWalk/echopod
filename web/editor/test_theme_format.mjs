import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { batteryRows, blankRows, paintedRows, resizedRows, rowsFromPixels } from "./icon_editor.js";
import { coveredCodes } from "./font_editor.js";
import { KINDS, canMoveParts, canStretch, drawingScript, element, fitStretched, moveParts, newElement, pathOf, readDrawing } from "./drawing.js";
import { arrangeUpright, arrangedBoxes, uprightMatches } from "./arrange.js";
import { LETTERS, LETTER_STYLES, letterFont, letterHeight, sheetFont, sheetLayout, sheetOrder, sheetTemplate } from "./pixel_letters.js";
import { ACCENT_FAMILY, COLOUR_SETS, mix, paletteOf, readableShade, schemeFrom, schemeOf } from "./colour_tools.js";
import { LIBRARY, VERSION_EVERY_MS, VERSION_LIMIT, addVersion, copyTheme, keep, readLibrary, removeTheme, versionDue, writeLibrary } from "./library.js";
import { READY, SHELF, SHELF_ART, SHELF_MORE, STARTS, drawingFrom, fitTiles, fromDesign, isTiles, plainPicture, startDrawing, tileRows, tilesFor } from "./drawings.js";
import { TILE_ICONS, addTileIcons, starterRows } from "./tile_icons.js";
import { compileScene } from "./scene_compile.js";
import { dimmed, fittedSize, quantise } from "./wallpaper_editor.js";
import {
  ThemeError,
  assetMemory,
  memoryParts,
  checksum,
  contrast,
  defaultTheme,
  hexColor,
  lint,
  opacityPercent,
  pack,
  rgb565,
  seeThroughOf,
  textReadsInCoverFlow,
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

const clear = defaultTheme(schema);
clear.boxes.selection.see_through = seeThroughOf(60);
clear.boxes.header.see_through = seeThroughOf(0);
const reopened = unpack(pack(clear, schema), schema);
assert.equal(reopened.boxes.selection.see_through, 6);
assert.equal(reopened.boxes.header.see_through, 15);
assert.equal(reopened.boxes.toast.see_through, 0);
assert.equal(opacityPercent(6), 60);
assert.match(toToml(clear, schema), /opacity = 60/);
assert.doesNotMatch(toToml(defaultTheme(schema), schema), /opacity/);

const bare = { ...defaultTheme(schema), home_art: false };
const bareBytes = pack(bare, schema);
assert.equal(bareBytes[52] & 2, 2);
assert.equal(unpack(bareBytes, schema).home_art, false);
assert.equal(unpack(dark, schema).home_art, true);
assert.match(toToml(bare, schema), /home_art = false/);
const credited = { ...defaultTheme(schema), author: "Ann \"Lee\"", description: "  Blue and calm " };
const creditedToml = toToml(credited, schema);
assert.match(creditedToml, /^author = "Ann \\"Lee\\""$/m);
assert.match(creditedToml, /^description = "Blue and calm"$/m);
assert.doesNotMatch(toToml(defaultTheme(schema), schema), /^(author|description) =/m);

const homeTheme = defaultTheme(schema);
const SCENE_ROOM = 6144;
const sceneSize = (drawing) => {
  const built = compileScene(drawingScript(drawing, homeTheme), schema.scene);
  return built.code.length + built.strings.length;
};
for (const screen of ["home", "playing"]) {
  for (const [name] of STARTS[screen]) {
    const drawing = startDrawing(name, screen);
    const script = drawingScript(drawing, homeTheme);
    compileScene(script, schema.scene);
    assert.ok(sceneSize(drawing) < SCENE_ROOM - 1200, `${screen} ${name} leaves room to add to it`);
    const key = screen === "home" ? "home_scene" : "now_playing_scene";
    const worn = { ...homeTheme, [key]: { script, chrome: null, design: JSON.stringify(drawing) } };
    const bytes = pack(worn, schema);
    const back = unpack(bytes, schema);
    assert.equal(back[key].script, script);
    assert.equal(readDrawing(back[key].design).elements.length, drawing.elements.length);
    assert.deepEqual(Array.from(pack(back, schema)), Array.from(bytes), `${screen} ${name} repacks`);
  }
}
for (const screen of ["home", "playing"]) {
  for (const [key, [label, make, only]] of Object.entries(READY)) {
    if (only && only !== screen) {
      continue;
    }
    const drawing = startDrawing("standard", screen);
    drawing.elements.push(make());
    const script = drawingScript(drawing, homeTheme);
    compileScene(script, schema.scene);
    assert.ok(sceneSize(drawing) < SCENE_ROOM, `the ready-made ${label} fits on ${screen}`);
    for (const shape of ["wide", "tall"]) {
      assert.ok(drawing.elements.at(-1).at[shape], `${key} is placed ${shape}`);
    }
  }
}
const picture = new Uint8ClampedArray(80 * 80 * 4);
for (let index = 0; index < 80 * 80; index++) {
  const shade = index < 3800 ? [18, 28, 58] : index < 5400 ? [232, 120, 40] : [240, 228, 200];
  picture.set([...shade, 255], index * 4);
}
const palette = paletteOf(picture, 6);
assert.equal(palette[0].color, "#121c3a", "the most common colour comes first");
assert.ok(palette.some((entry) => entry.color === "#e87828"));
assert.ok(Math.abs(palette.reduce((sum, entry) => sum + entry.share, 0) - 1) < 1e-9);
for (const dark of [true, false]) {
  const coloured = defaultTheme(schema);
  Object.assign(coloured.colors, schemeFrom(palette, dark));
  assert.deepEqual(lint(coloured, schema), [], `colours from a picture read well when ${dark ? "dark" : "light"}`);
  assert.equal(coloured.colors.selection_background, "#e87828", "the liveliest colour fills the selected row");
}
assert.ok(contrast(readableShade("#777777", "#808080", 4.5), "#808080") >= 4.5, "a hard-to-read colour is made readable");
assert.equal(readableShade("#ffffff", "#000000", 4.5), "#ffffff", "a readable colour is left alone");
assert.equal(mix("#000000", "#ffffff", 0.5), "#808080");
function shelf(room) {
  const kept = new Map();
  return {
    getItem: (key) => kept.get(key) ?? null,
    setItem: (key, value) => {
      if (value.length > room) {
        throw new Error("full");
      }
      kept.set(key, value);
    },
    kept,
  };
}
const stored = (name, colour) => JSON.stringify({ theme: { name, colors: { background: colour } }, designs: {} });
const library = readLibrary(shelf(1e9));
assert.deepEqual(library, { current: null, themes: [] }, "an empty browser has no themes");
keep(library, "a", stored("Night", "#000000"), "Night", 1000);
assert.equal(library.current, "a");
assert.ok(versionDue(library, "a", 1000), "a new theme keeps its first version straight away");
assert.ok(addVersion(library, "a", 1000));
assert.equal(addVersion(library, "a", 2000), false, "an unchanged theme isn't kept twice");
keep(library, "a", stored("Night", "#111111"), "Night", 2000);
assert.equal(versionDue(library, "a", 2000), false);
assert.ok(versionDue(library, "a", 1000 + VERSION_EVERY_MS), "another version is due ten minutes later");
for (let step = 0; step < VERSION_LIMIT + 5; step++) {
  keep(library, "a", stored("Night", `#0000${String(step).padStart(2, "0")}`), "Night", 3000 + step);
  addVersion(library, "a", 3000 + step);
}
assert.equal(library.themes[0].versions.length, VERSION_LIMIT, "only the latest versions are kept");
const copy = copyTheme(library, "a", 9000, "Night copy");
assert.equal(JSON.parse(copy.snapshot).theme.name, "Night copy", "a copy carries its own name");
assert.equal(library.themes.length, 2);
const tight = shelf(JSON.stringify(library).length - 200);
assert.ok(writeLibrary(tight, library), "when the browser is full the oldest versions make room");
assert.ok(library.themes[0].versions.length + library.themes[1].versions.length < VERSION_LIMIT);
assert.deepEqual(readLibrary(tight).themes.map((entry) => entry.name), ["Night copy", "Night"]);
assert.equal(writeLibrary(shelf(10), library), false, "and it says so when nothing fits");
removeTheme(library, library.current);
assert.equal(library.current, copy.id, "removing the open theme opens the next");
const garbled = shelf(1e9);
garbled.setItem(LIBRARY, "{not json");
assert.deepEqual(readLibrary(garbled).themes, []);
const plainHome = drawingScript(startDrawing("standard", "home"), homeTheme);
assert.match(plainHome, /^backdrop\(x, y, width, height\)$/m, "Home starts on the wallpaper, written once for both ways");
assert.match(plainHome, /menu\(x, y, 176, 146\)/);
assert.match(plainHome, /reflect\(/);
for (const face of ["vu", "cassette", "turntable", "lcd"]) {
  const worn = unpack(new Uint8Array(readFileSync(join(site, "themes", `${face}.echotheme`))), schema);
  const consts = Object.fromEntries([...worn.now_playing_scene.script.matchAll(/^const ([A-Z][A-Z0-9_]*) = (#[0-9a-fA-F]{6})/gm)].map((found) => [found[1], found[2]]));
  const drawing = startDrawing(face, "playing", consts);
  assert.ok(drawing.elements.length > 3, `${face} becomes a drawing`);
  assert.ok(JSON.stringify(drawing).includes(Object.values(consts)[1]), `${face} keeps the face's colours`);
}
for (const kind of Object.keys(KINDS)) {
  compileScene(drawingScript({ version: 2, screen: "home", elements: [newElement(kind, "home")] }, homeTheme), schema.scene);
}
const at = (x, y, w, h) => ({ x, y, w, h });
const box = element("box", at(10, 20, 30, 4), at(10, 20, 30, 4), { color: "#ff0000" });
assert.equal(drawingScript({ version: 2, elements: [box] }, homeTheme).trim(), "rect(x + 10, y + 20, 30, 4, #ff0000)", "a part placed alike both ways is written once");
const moved = element("box", at(10, 20, 30, 4), at(12, 50, 30, 4), { color: "#ff0000" });
assert.match(drawingScript({ version: 2, elements: [moved] }, homeTheme), /^if width > height \{\n {4}rect\(x \+ 10, y \+ 20, 30, 4, #ff0000\)\n\} else \{\n {4}rect\(x \+ 12, y \+ 50/);
const filled = element("box", at(0, 0, 200, 3), at(0, 0, 150, 3), { bind: { w: { source: "progress", from: 0, to: 100 } } });
const filledScript = drawingScript({ version: 2, elements: [filled] }, homeTheme);
assert.match(filledScript, /rect\(x, y, \(200 \* progress\(1000\) \/ 1000\), 3/, "a width that follows the song fills its own width");
assert.match(filledScript, /\(150 \* progress\(1000\) \/ 1000\)/);
const needle = element("line", at(50, 60, 40, 0), at(50, 60, 40, 0), { angle: -45, bind: { angle: { source: "left", from: -45, to: 45 } } });
assert.match(drawingScript({ version: 2, elements: [needle] }, homeTheme), /^let level_l = 0$/m, "a needle that follows the left level measures it");
const ring = element("group", at(0, 0, 40, 40), at(0, 0, 40, 40), { repeat: { count: 8, columns: 1, dx: 0, dy: 0, turn: 45, start: 0, spin: 0 }, children: [
  element("box", at(19, 0, 2, 6), at(19, 0, 2, 6), { color: "#00ff00", bind: { color: { when: { on: "lit", source: "progress" }, color: "#ffffff" } } }),
] });
const ringScript = drawingScript({ version: 2, elements: [ring] }, homeTheme);
assert.match(ringScript, /repeat copy_1 from 0 to 8/);
assert.match(ringScript, /let lit_1_progress = progress\(1000\) \* 8 \/ 1000\n *repeat copy_1/, "the song's position is read once, before the copies");
assert.match(ringScript, /copy_1 < lit_1_progress/, "a copy lights while the song has passed it");
assert.match(ringScript, /let turn_1 = \(copy_1 \* 32000 \/ 1000\) & 255/);
const spokes = element("group", at(0, 0, 40, 40), at(0, 0, 40, 40), { repeat: { count: 12, columns: 1, dx: 0, dy: 0, turn: 30, start: 0, spin: 0 }, children: [
  element("line", at(20, 20, 6, 0), at(20, 20, 6, 0), { start: 12 }),
] });
const spokesScript = drawingScript({ version: 2, elements: [spokes] }, homeTheme);
assert.doesNotMatch(spokesScript, /cos|sin/, "a line turning about the middle doesn't move its pivot");
assert.match(spokesScript, /spoke\(x \+ 20, y \+ 20, turn_1, 32, 12, 18, /);
const tiles = element("group", at(0, 0, 200, 100), null, { repeat: { count: "items", columns: 2, dx: 100, dy: 40, turn: 0, start: 0, spin: 0 }, children: [
  element("text", at(0, 0, 90, 30), null, { source: "item", color: "#ffffff", bind: { color: { when: { on: "highlighted" }, color: "#000000" } } }),
] });
const tilesScript = drawingScript({ version: 2, screen: "home", elements: [tiles] }, homeTheme);
assert.match(tilesScript, /item\(copy_1\)/);
assert.match(tilesScript, /copy_1 == chosen_1/);
assert.match(tilesScript, /^if width > height \{/m, "a part drawn only sideways stays sideways");
const bars = element("group", at(0, 0, 50, 40), at(0, 0, 50, 40), { repeat: { count: 5, columns: 5, dx: 9, dy: 0, turn: 0, start: 0, spin: 0 }, children: [
  element("group", at(0, 35, 7, 3), at(0, 35, 7, 3), { repeat: { count: 8, columns: 1, dx: 0, dy: -5, turn: 0, start: 0, spin: 0 }, children: [
    element("box", at(0, 0, 7, 3), at(0, 0, 7, 3), { color: "#333333", bind: { color: { when: { on: "lit", source: "beat" }, color: "#ffffff" } } }),
  ] }),
] });
const barsScript = drawingScript({ version: 2, elements: [bars] }, homeTheme);
assert.equal(barsScript.match(/let beat_/g).length, 1, "bars inside bars share the outer beat");
assert.match(barsScript, /copy_2 < \(beat_copy_1 \* 125\) \* 8 \/ 1000/);
compileScene(barsScript, schema.scene);
const veil = element("box", at(0, 0, 40, 20), at(0, 0, 40, 20), { fill: "none", grid: 2, gridColor: "#000000" });
assert.equal(drawingScript({ version: 2, elements: [veil] }, homeTheme).trim(), "lattice(x, y, 40, 20, 2, #000000)", "a box with no fill draws only its grid");
const hidden = { ...box, hidden: true };
assert.equal(drawingScript({ version: 2, elements: [hidden] }, homeTheme).trim(), "");
assert.equal(readDrawing("not json"), null);
assert.equal(readDrawing(JSON.stringify({ version: 1, layers: [] })), null);
const older = {
  version: 1,
  background: { style: "gradient", top: "@header_background", bottom: "#102030" },
  layers: [
    { id: 1, type: "cover", frames: { wide: at(10, 8, 112, 112), tall: at(29, 8, 112, 112) }, shape: "square" },
    { id: 2, type: "text", frames: { wide: at(132, 8, 180, 22), tall: at(8, 128, 154, 22) }, source: "song", size: "large", color: "@text", feedback: true },
    { id: 3, type: "progress", style: "ring", frames: { wide: at(10, 8, 112, 112), tall: at(29, 8, 112, 112) }, color: "@text", track: "@divider" },
    { id: 4, type: "vu", frames: { wide: at(170, 40, 146, 72), tall: null }, channel: "right", colours: { ink: "#112233" } },
    { id: 5, type: "levels", frames: { wide: at(170, 120, 60, 20), tall: at(10, 200, 80, 30) } },
    { id: 6, type: "cassette", frames: { wide: null, tall: at(8, 160, 153, 100) } },
  ],
};
const converted = drawingFrom(JSON.stringify(older), "playing");
assert.equal(converted.version, 2);
assert.equal(converted.elements[0].fill, "gradient", "the old background becomes a box");
assert.ok(converted.elements.some((item) => item.name === "Messages"), "the old seek and volume spot becomes its own text");
assert.ok(converted.elements.some((item) => item.name === "VU meter" && item.at.tall === null), "an old part becomes a group");
assert.ok(JSON.stringify(converted).includes("#112233"), "an old part keeps its colours");
assert.ok(sceneSize(converted) < SCENE_ROOM);
assert.deepEqual(fromDesign(older, "playing").elements.length, converted.elements.length);

for (const [name, style] of Object.entries(LETTER_STYLES)) {
  for (const entry of schema.fonts) {
    if (letterHeight(style) > entry.height) {
      assert.throws(() => letterFont(style, entry.height), RangeError, `${name} is refused where it doesn't fit`);
      continue;
    }
    const font = letterFont(style, entry.height);
    assert.ok(font.ascent + font.descent <= entry.height, `${name} fits ${entry.name}`);
    assert.equal(font.glyphs.length, Object.keys(LETTERS).length + 26, `${name} has capitals for lowercase`);
    const lettered = { ...defaultTheme(schema), fonts: { ...defaultTheme(schema).fonts, [entry.name]: font }, font_sources: { [entry.name]: { kind: "drawn" } }, font_edits: {} };
    const back = unpack(pack(lettered, schema), schema);
    assert.equal(back.fonts[entry.name].glyphs.length, font.glyphs.length, `${name} packs as the ${entry.name} font`);
  }
}
const everyChar = schema.font_ranges.flatMap(([first, last]) => Array.from({ length: last - first + 1 }, (_, index) => String.fromCharCode(first + index)));
const dotted = letterFont(LETTER_STYLES.dots, 20);
assert.deepEqual(everyChar.filter((char) => !dotted.glyphs.some((glyph) => glyph.char === char)), [], "the letter set covers every character a theme font may have");
const order = sheetOrder(schema.font_ranges);
assert.equal(Array.from(order).length, everyChar.length, "the template has a box for every character a theme font may have");
for (const entry of schema.fonts) {
  const layout = sheetLayout(entry.height);
  const sheet = sheetTemplate(layout, order);
  assert.equal(sheet.width, layout.cell[0] * 16);
  assert.throws(() => sheetFont(sheet, { ...layout, order, gap: 1, limit: entry.height }), RangeError, "guides and hints alone are no letters");
  const ink = (image, colour) => {
    const at = Array.from(order).indexOf("I");
    const [left, top] = [(at % 16) * layout.cell[0], Math.floor(at / 16) * layout.cell[1]];
    for (let y = 2; y < layout.baseline + 1; y++) {
      for (let x = 2; x < 4; x++) {
        image.data.set(colour, ((top + y) * image.width + left + x) * 4);
      }
    }
  };
  ink(sheet, [0, 0, 0, 255]);
  const font = sheetFont(sheet, { ...layout, order, gap: 1, limit: entry.height });
  const letter = font.glyphs.find((glyph) => glyph.char === "I");
  assert.deepEqual([letter.left, letter.top, letter.advance, letter.rows.length, letter.rows[0]], [2, layout.baseline - 2, 5, layout.baseline - 1, "ff"], `an I drawn on the ${entry.name} template`);
  assert.equal(font.ascent + font.descent, layout.baseline - 1, "the guide line under the letter is not ink");
  const clear = { width: sheet.width, height: sheet.height, data: new Uint8ClampedArray(sheet.data.length) };
  ink(clear, [255, 255, 255, 255]);
  assert.deepEqual(sheetFont(clear, { ...layout, order, gap: 1, limit: entry.height }).glyphs.find((glyph) => glyph.char === "I").rows, letter.rows, "white on see-through reads the same");
}
const memoryTheme = { ...defaultTheme(schema), fonts: { ...defaultTheme(schema).fonts, title: dotted } };
const memory = memoryParts(memoryTheme, schema);
assert.ok(memory.fonts.title > 0 && memory.wallpaper === 0 && memory.used === memory.fonts.title && memory.limit === schema.asset_memory);
assert.equal(memoryParts(defaultTheme(schema), schema).used, 0);

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
assert.equal(paperedView.getUint16(6, true), 56);
assert.equal(checked, dark.length + 16);
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
assert.equal(pack({ ...fresh, wallpapers: { ...fresh.wallpapers, wallpaper: many } }, schema).length, dark.length + 16 + 32 + 32 * 64);

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
  /more than the player has room for/,
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
assert.equal(arrangedView.getUint16(6, true), 56);
assert.deepEqual(Array.from(arrangedBytes.subarray(48, 51)), [0, 10, 10]);
assert.equal(arrangedBytes.length, dark.length + 2 * 10 * 8);
const placesAt = 56 + 2 * schema.colors.length + 12 * schema.boxes.length + schema.spacing.length;
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

const source = toToml(ember, schema);
assert.match(source, /^name = "Ember"$/m);
assert.match(source, /^row_height = 28$/m);
assert.match(source, /^\[boxes\.toast\]$/m);
assert.match(source, /^radius = 8$/m);
console.log(`ok: ${packages.length} themes repack to themselves`);

const accented = defaultTheme(schema);
accented.name = "Accented";
accented.cover_flow = "dark";
accented.accents = [
  { name: "Red", selection_background: "#c0262e", selection_top: "#e65458", selection_shine: "#f48080" },
  { name: "Ruby", selection_background: "#900020", selection_top: "#b00030", selection_shine: "#e06080" },
];
const accentedBytes = pack(accented, schema);
assert.equal(accentedBytes[51], 2);
assert.equal(accentedBytes[52], 1);
const accentedBack = unpack(accentedBytes, schema);
assert.deepEqual(accentedBack.accents.map((accent) => accent.name), ["Red", "Ruby"]);
assert.equal(accentedBack.cover_flow, "dark");
assert.deepEqual(Array.from(pack(accentedBack, schema)), Array.from(accentedBytes), "accents repack to themselves");
const accentedToml = toToml(accented, schema);
assert.ok(accentedToml.includes('cover_flow = "dark"'));
assert.ok(accentedToml.includes('[[accents]]\nname = "Ruby"'));
accented.accents = Array.from({ length: 9 }, () => accented.accents[0]);
assert.throws(() => pack(accented, schema), /at most 8 accents/);

const tape = unpack(new Uint8Array(readFileSync(join(site, "themes", "cassette.echotheme"))), schema);
assert.equal(typeof tape.now_playing_scene.script, "string");
assert.ok(tape.now_playing_scene.script.includes("rect("));
assert.equal(tape.now_playing_scene.chrome.header_top, "#4a454a");
const tapeToml = toToml(tape, schema);
assert.ok(tapeToml.includes("[now_playing_scene]\nscript = '''"));
assert.ok(tapeToml.includes("[now_playing_scene.chrome]"));
const broken = { ...tape, now_playing_scene: { script: "rect(1, 2)", chrome: null } };
assert.throws(() => pack(broken, schema), /Now Playing scene: line 1: rect\(\) takes 5 values/);
assert.equal(unpack(pack({ ...tape, now_playing_scene: null }, schema), schema).now_playing_scene, null);

const outline = batteryRows("frame", 22, 10);
const fill = batteryRows("level", 22, 10);
assert.equal(outline.length, 10);
assert.equal(outline[0], "ffffffffffffffffffff..");
assert.equal(outline[4], "f..................fff");
assert.equal(fill[4], "..ffffffffffffffff....");
const paired = { ...defaultTheme(schema), icons: { ...defaultTheme(schema).icons, battery: outline, battery_level: fill } };
assert.ok(pack(paired, schema).length > 0, "a drawn battery outline and fill pack together");

const tiled = startDrawing("tiles", "home");
const tiledScript = drawingScript(tiled, homeTheme);
assert.match(tiledScript, /item_icon\(copy_1, /, "each tile draws the icon of the item it holds");
assert.match(tiledScript, /line\(item\(copy_1\), small, /, "and its name");
assert.equal(tiled.elements.filter((item) => item.repeat?.count === "items").length, 2, "one set of tiles for each way the player is held");
const single = element("symbol", { x: 4, y: 4, w: 24, h: 24 }, { x: 4, y: 4, w: 24, h: 24 }, { mark: "item" });
const loneScript = drawingScript({ version: 2, screen: "home", elements: [single] }, homeTheme);
assert.match(loneScript, /item_icon\(selected\(\), x \+ 4, y \+ 4, 24, 24, /, "outside the tiles, the highlighted item's icon");
compileScene(loneScript, schema.scene);

const striped = startDrawing("tiles", "home");
const stripedTile = striped.elements[1].children.find((item) => item.name === "Tile");
stripedTile.each = { color: ["#0050ef", "@selection_background", "#a20025"] };
const stripedScript = drawingScript(striped, homeTheme);
assert.ok(stripedScript.includes(`(copy_1 % 3 == 0 ? #0050ef : copy_1 % 3 == 1 ? ${homeTheme.colors.selection_background} : #a20025)`), "copies take their colours in turn");
compileScene(stripedScript, schema.scene);
const apart = element("box", { x: 0, y: 0, w: 10, h: 10 }, { x: 0, y: 0, w: 10, h: 10 }, { color: "#112233", each: { color: ["#0050ef", "#a20025"] } });
assert.doesNotMatch(drawingScript({ version: 2, screen: "home", elements: [apart] }, homeTheme), /%/, "a part that isn't copied keeps its one colour");

assert.deepEqual(TILE_ICONS, schema.icons.slice(-TILE_ICONS.length).map((entry) => entry.name), "a ready-made icon for every Home item");
for (const name of TILE_ICONS) {
  const rows = starterRows(name);
  assert.deepEqual([rows.length, rows[0].length], [24, 24]);
  const inked = rows.join("").replaceAll(".", "").length;
  assert.ok(inked > 60 && inked < 420, `${name} is drawn, with room around it`);
  assert.deepEqual([starterRows(name, 16, 12).length, starterRows(name, 16, 12)[0].length], [12, 16]);
}
assert.equal(starterRows("chevron"), null);
assert.equal(starterRows("home_settings")[12][12], ".", "the gear has a hole in the middle");
const iconed = defaultTheme(schema);
assert.equal(addTileIcons(iconed, schema), true);
assert.ok(TILE_ICONS.every((name) => iconed.icons[name].length === 24));
assert.equal(addTileIcons(iconed, schema), false, "icons already drawn are left alone");
const tiledTheme = { ...iconed, home_scene: { script: tiledScript, chrome: null, design: JSON.stringify(tiled) } };
const tiledBytes = pack(tiledTheme, schema);
assert.deepEqual(unpack(tiledBytes, schema).icons.home_music, iconed.icons.home_music);
const crowded = { ...iconed, icons: { ...ember.icons, ...Object.fromEntries(TILE_ICONS.map((name) => [name, iconed.icons[name]])) } };
assert.throws(() => pack(crowded, schema), /Make some icons smaller/, "too many big icons is said plainly");

for (const [name, background, text, accent] of COLOUR_SETS) {
  const coloured = defaultTheme(schema);
  const scheme = schemeOf(background, text, accent);
  assert.deepEqual([scheme.background, scheme.text, scheme.selection_background], [background, text, accent], `${name} keeps its three colours`);
  assert.deepEqual(Object.keys(scheme).sort(), Object.keys(coloured.colors).sort(), "three colours make every colour a theme has");
  Object.assign(coloured.colors, scheme);
  assert.deepEqual(lint(coloured, schema), [], `the ${name} colour set reads well`);
}
const recoloured = schemeOf("#101114", "#f2f2f5", "#e0469a");
const before = schemeOf("#101114", "#f2f2f5", "#2f6fdb");
const reshaded = Object.keys(before).filter((role) => before[role] !== recoloured[role]);
assert.ok(reshaded.length >= 3 && reshaded.every((role) => ACCENT_FAMILY.includes(role)), "a new accent changes the highlighted row and nothing else");

for (const screen of ["home", "playing"]) {
  for (const [key, label, make, only] of [...SHELF, ...SHELF_MORE]) {
    if (only && only !== screen) {
      continue;
    }
    const drawing = startDrawing("empty", screen);
    drawing.elements.push(...[make(screen)].flat());
    compileScene(drawingScript(drawing, homeTheme), schema.scene);
    assert.ok(sceneSize(drawing) < SCENE_ROOM, `${label} fits on ${screen}`);
    assert.ok(drawing.elements.slice(1).every((item) => item.at.wide || item.at.tall), `${key} has a place`);
  }
}
assert.deepEqual(SHELF.map(([key]) => key).filter((key) => !SHELF_ART[key]), [], "every part on the shelf has a picture");
const narrow = tilesFor({ wide: { x: 0, y: 0, w: 176, h: 146 }, tall: null });
assert.equal(narrow.length, 1, "tiles are made only for the screens the menu was on");
assert.deepEqual([narrow[0].repeat.columns, narrow[0].repeat.dx, narrow[0].repeat.dy], [2, 90, 73], "tiles fill the place the list had");
assert.deepEqual(narrow[0].at.wide, { x: 0, y: 0, w: 176, h: 146 });

const plainPlaying = startDrawing("standard", "playing");
const handMade = Object.fromEntries(plainPlaying.elements.map((item) => [item.name || item.source || item.kind, { ...item.at.tall }]));
assert.equal(uprightMatches(plainPlaying), false, "the hand-made upright screen isn't quite the arranged one");
arrangeUpright(plainPlaying);
assert.equal(uprightMatches(plainPlaying), true);
const arrangedNow = Object.fromEntries(plainPlaying.elements.map((item) => [item.name || item.source || item.kind, item.at.tall]));
assert.deepEqual(arrangedNow.cover, { x: 29, y: 8, w: 112, h: 112 }, "the cover is centred at the top");
assert.deepEqual(arrangedNow.song, { x: 8, y: 126, w: 154, h: 22, align: "center" }, "the title goes under it, across the screen and centred");
assert.deepEqual([arrangedNow.remaining.align, arrangedNow.elapsed.align], ["right", undefined], "text that was lined up keeps its side");
assert.ok(arrangedNow.Progress.y > 240 && arrangedNow.elapsed.y > arrangedNow.Progress.y, "the progress bar and times stay at the bottom");
for (const [name, at] of Object.entries(arrangedNow)) {
  assert.ok(at.x >= 0 && at.y >= 0 && at.x + at.w <= 170 && at.y + at.h <= 280, `${name} is on the upright screen`);
  assert.ok(Math.abs(at.y - handMade[name].y) <= 20, `${name} lands near where a person put it`);
}
const uprightHome = startDrawing("standard", "home");
arrangeUpright(uprightHome);
const homeNow = Object.fromEntries(uprightHome.elements.map((item) => [item.name || item.kind, item.at.tall]));
assert.deepEqual(homeNow.Menu, { x: 0, y: 0, w: 170, h: 146 }, "the menu takes the top, edge to edge");
assert.deepEqual([homeNow["Art panel"].x, homeNow["Art panel"].w, homeNow["Art panel"].y], [0, 170, 146], "what was beside it goes below it");
assert.deepEqual(homeNow.Divider, { x: 0, y: 146, w: 170, h: 1 }, "and the line between them turns to lie between them");
assert.equal(homeNow["Art panel"].y + homeNow["Art panel"].h, 280);
compileScene(drawingScript(uprightHome, homeTheme), schema.scene);
const oneWayTiles = startDrawing("tiles", "home");
arrangeUpright(oneWayTiles);
assert.deepEqual(oneWayTiles.elements.map((item) => [Boolean(item.at.wide), Boolean(item.at.tall)]), [[true, true], [true, false], [false, true]], "parts drawn for one way up are left alone");
const packedIn = startDrawing("empty", "playing");
for (let row = 0; row < 5; row++) {
  packedIn.elements.push(element("cover", { x: 10, y: 4 + row * 28, w: 100, h: 100 }, { x: 0, y: 0, w: 10, h: 10 }));
}
for (const box of arrangedBoxes(packedIn).values()) {
  assert.ok(box.y >= 0 && box.y + box.h <= 280, "a packedIn screen is shrunk to fit");
}

assert.equal(plainPicture("home", "wide", 250, 60), true, "the art beside the Home menu is a picture, not a colour");
assert.equal(plainPicture("home", "wide", 50, 60), false, "the menu isn't");
assert.equal(plainPicture("home", "tall", 80, 200), true, "upright, the art is under the menu");
assert.equal(plainPicture("playing", "wide", 60, 60), true, "and so is the cover on Now Playing");
assert.equal(plainPicture("playing", "wide", 200, 20), false);

const panel = element("box", { x: 176, y: 0, w: 144, h: 146 }, { x: 0, y: 146, w: 170, h: 134 }, { color: "#08080a" });
const softCover = element("cover", { x: 192, y: 8, w: 112, h: 112 }, { x: 29, y: 160, w: 112, h: 112 }, { corner: 12 });
const cornered = { version: 2, screen: "home", elements: [startDrawing("empty", "home").elements[0], panel, softCover] };
const corneredScript = drawingScript(cornered, homeTheme);
assert.equal(corneredScript.match(/ring\(/g).length, 4, "a cover's four corners are painted over it");
assert.equal(corneredScript.match(/clip\([^)]*, 12, 12\)/g).length, 4, "each in a square the size of the corner");
assert.equal(corneredScript.match(/\* 16, 288, 192, #08080a\)/g).length, 4, "and in the colour of the box behind the cover");
assert.match(corneredScript, /rounded\([^)]*, 112, 112, 12, /, "and with no album art the empty square is rounded too");
compileScene(corneredScript, schema.scene);
softCover.behind = "#123456";
assert.ok(drawingScript(cornered, homeTheme).includes(", 192, #123456)"), "or in a colour you choose");
delete softCover.behind;
panel.fill = "wallpaper";
assert.ok(drawingScript(cornered, homeTheme).includes(`, 192, ${homeTheme.colors.background})`), "over a picture they fall back to the theme's background");
softCover.shape = "circle";
assert.doesNotMatch(drawingScript(cornered, homeTheme), /clip\(/, "a round cover has no corners to round");
softCover.shape = "square";
softCover.corner = 0;
assert.doesNotMatch(drawingScript(cornered, homeTheme), /ring\(/);

const stretchy = tilesFor({ wide: { x: 7, y: 6, w: 176, h: 146 }, tall: null })[0];
assert.equal(isTiles(stretchy), true);
stretchy.at.wide.w = 307;
stretchy.at.wide.h = 134;
fitTiles(stretchy, "wide");
assert.deepEqual([stretchy.repeat.columns, tileRows(stretchy, "wide"), stretchy.repeat.dx, stretchy.repeat.dy], [2, 2, 155, 67], "a group made wider keeps its tiles across and down, and stretches them to fill it");
const stretched = Object.fromEntries(stretchy.children.map((child) => [child.name, child.at.wide]));
assert.deepEqual(stretched.Tile, { x: 2, y: 2, w: 147, h: 59 });
assert.deepEqual(stretched.Icon, { x: 63, y: 12, w: 24, h: 24 }, "with the icon and name kept in place on each tile");
for (let width = 300; width <= 316; width++) {
  stretchy.at.wide.w = width;
  fitTiles(stretchy, "wide");
  assert.equal(stretchy.repeat.columns, 2, "a few pixels more or less never changes how many go across");
}
stretchy.repeat.columns = 3;
stretchy.repeat.rows = 1;
fitTiles(stretchy, "wide");
assert.deepEqual([stretchy.repeat.dx, stretchy.repeat.dy], [106, 135], "choosing three across in one row makes three tiles that fill the group");
assert.ok(3 * 106 - 4 <= 316);
const unmarked = tilesFor({ wide: { x: 0, y: 0, w: 176, h: 146 }, tall: null })[0];
delete unmarked.repeat.rows;
assert.equal(tileRows(unmarked, "wide"), 2, "tiles made before rows were kept still know how many rows they show");
compileScene(drawingScript({ version: 2, screen: "home", elements: [stretchy] }, homeTheme), schema.scene);
assert.equal(isTiles(element("group", { x: 0, y: 0, w: 10, h: 10 }, null)), false);

const bridged = startDrawing("standard", "home");
bridged.elements.push(element("text", { x: 12, y: 113, w: 180, h: 22 }, { x: 0, y: 0, w: 10, h: 10 }, { source: "song", size: "large" }));
arrangeUpright(bridged);
const bridgedNow = Object.fromEntries(bridged.elements.map((item) => [item.name || item.source || item.kind, item.at.tall]));
assert.deepEqual([bridgedNow.Menu.w, bridgedNow["Art panel"].w], [170, 170], "a title laid across the menu and the art doesn't shrink the screen to a miniature");
assert.ok(bridgedNow.song.y >= 100 && bridgedNow.song.y < 146 && bridgedNow.song.x + bridgedNow.song.w <= 170, "it stays with the menu it mostly sat on");
assert.equal(bridgedNow["Art panel"].y + bridgedNow["Art panel"].h, 280);

const crossed = startDrawing("standard", "playing");
crossed.elements.push(element("circle", { x: 146, y: 59, w: 28, h: 28 }, { x: 0, y: 0, w: 28, h: 28 }));
arrangeUpright(crossed);
const crossedNow = Object.fromEntries(crossed.elements.map((item) => [item.name || item.source || item.kind, item.at.tall]));
assert.deepEqual([crossedNow.album.w, crossedNow.format.w, crossedNow.format.h], [154, 154, 16], "a small part dropped over some text doesn't squeeze the text into its own box");
assert.ok(crossedNow.circle.w === crossedNow.circle.h && crossedNow.circle.w >= 20 && crossedNow.circle.w <= 28, "and stays round");

const coverFlowText = (text, text_secondary) => textReadsInCoverFlow({ colors: { text, text_secondary } });
assert.equal(coverFlowText("#000000", "#3b3b3b"), false, "black text can't be read on Cover Flow's black wall");
assert.equal(coverFlowText("#ffffff", "#505050"), false, "nor can dim secondary text under light titles");
assert.equal(coverFlowText("#ff8647", "#b36132"), true, "amber text reads on black");
assert.equal(coverFlowText("#f2f2f5", "#8e8e96"), true, "the player's own Dark text reads on black");

const outerGroup = element("group", at(40, 10, 200, 100), at(10, 30, 150, 200));
const innerGroup = element("group", at(20, 30, 100, 50), at(5, 15, 120, 60));
const dot = element("circle", at(15, 12, 10, 10), at(7, 9, 10, 10));
const loose = element("box", at(100, 100, 20, 20), at(30, 40, 20, 20));
innerGroup.children = [dot];
outerGroup.children = [innerGroup];
const nested = { version: 2, elements: [outerGroup, loose] };
assert.ok(moveParts(nested, [loose.id], innerGroup.id, "inside"), "a part can be dropped two groups deep");
assert.deepEqual(pathOf(nested, loose.id), [outerGroup, innerGroup]);
assert.deepEqual([loose.at.wide.x, loose.at.wide.y, loose.at.tall.x, loose.at.tall.y], [40, 60, 15, -5], "and stays where it was on both screens");
assert.equal(innerGroup.children.at(-1), loose, "in front of what was there");
assert.ok(moveParts(nested, [dot.id], outerGroup.id, "before"), "a part can be dragged out of two groups at once");
assert.deepEqual(nested.elements.map((item) => item.id), [dot.id, outerGroup.id]);
assert.deepEqual([dot.at.wide.x, dot.at.wide.y, dot.at.tall.x, dot.at.tall.y], [75, 52, 22, 54]);
assert.ok(!canMoveParts(nested, [outerGroup.id], innerGroup.id, "inside"), "a group can't go inside itself");
assert.ok(!canMoveParts(nested, [dot.id], loose.id, "inside"), "only a group takes parts inside");
assert.ok(!canMoveParts(nested, [dot.id], dot.id, "after"), "a part can't be dropped on itself");
assert.ok(moveParts(nested, [loose.id], dot.id, "after"), "a part can be dropped beside another");
assert.deepEqual(nested.elements.map((item) => item.id), [dot.id, loose.id, outerGroup.id]);
assert.deepEqual([loose.at.wide.x, loose.at.wide.y], [100, 100], "back where it started");

const backdrop = element("box", at(3, 4, 5, 6), at(3, 4, 5, 6), { stretch: true, color: "#123456" });
const card = element("group", at(30, 20, 120, 70), at(10, 50, 140, 90), { children: [backdrop] });
const stretchedDrawing = fitStretched({ version: 2, elements: [card] });
assert.deepEqual([backdrop.at.wide, backdrop.at.tall], [at(0, 0, 120, 70), at(0, 0, 140, 90)], "a part that fills its group takes the group's size");
card.at.wide.w = 180;
card.at.wide.h = 40;
const stretchedScript = drawingScript(stretchedDrawing, homeTheme);
assert.deepEqual(backdrop.at.wide, at(0, 0, 180, 40), "and follows the group when it is resized");
assert.match(stretchedScript, /let gx_1 = x \+ 30\n *let gy_1 = y \+ 20\n *rect\(gx_1, gy_1, 180, 40, #123456\)/, "the player draws it at the group's place and size");
backdrop.bind = { x: { source: "progress", from: 0, to: 50 }, w: { source: "progress", from: 0, to: 100 } };
const boundScript = drawingScript(stretchedDrawing, homeTheme);
assert.match(boundScript, /rect\(gx_1, gy_1, \(180 \* progress\(1000\) \/ 1000\), 40,/, "a filling part keeps its place but still grows with the song");
compileScene(stretchedScript, schema.scene);
compileScene(boundScript, schema.scene);
const screenWide = element("box", at(5, 5, 10, 10), at(5, 5, 10, 10), { stretch: true, color: "#abcdef" });
fitStretched({ version: 2, elements: [screenWide] });
assert.deepEqual([screenWide.at.wide, screenWide.at.tall], [at(0, 0, 320, 146), at(0, 0, 170, 280)], "with no group it fills the screen");
assert.ok(!canStretch(element("line", at(0, 0, 10, 0), at(0, 0, 10, 0)), []), "a line has a length, not a size, so it can't fill");
assert.ok(!canStretch(element("box", at(0, 0, 10, 10), null), [ring]), "nor can a part inside copies");
const hopper = element("box", at(0, 0, 10, 10), at(0, 0, 10, 10), { stretch: true });
const smallGroup = element("group", at(0, 0, 50, 30), at(0, 0, 60, 30), { children: [] });
const bigGroup = element("group", at(0, 0, 90, 80), at(0, 0, 100, 70), { children: [hopper] });
const hopping = fitStretched({ version: 2, elements: [smallGroup, bigGroup] });
assert.ok(moveParts(hopping, [hopper.id], smallGroup.id, "inside"));
assert.deepEqual(hopper.at.wide, at(0, 0, 50, 30), "a filling part dropped in another group fills that one");

