import { SceneError, compileScene } from "./scene_compile.js";
const MAGIC = [0x45, 0x54, 0x48, 0x4d];
const VERSION = 1;
const HEADER_SIZE = 48;
const CHECKED_FROM = 16;
const NAME_SIZE = 24;
const BOX_SIZE = 12;
export const OPAQUE = 15;
const MAX_SIZE = 3072;
const ICON_ENTRY_SIZE = 4;
const ICON_FIELD = 23;
export const ICON_MARKS = ".123456789abcdef";
const LONG_HEADER_SIZE = 52;
const ASSET_HEADER_SIZE = 56;
const ACCENT_SIZE = 20;
const ACCENT_NAME_SIZE = 13;
const CUSTOM_ACCENT = 0xff;
const FLAG_DARK_COVER_FLOW = 1;
const FLAG_NO_HOME_ART = 2;
export const MOST_ACCENTS = 8;
const SCENE_KIND = 5;
const SCENE_SOURCE_KIND = 6;
const SCENE_DESIGN_KIND = 7;
const SCENE_CHROME = 1;
const SCENE_MOST_BYTES = 6144;
export const SCENES = [
  { key: "now_playing_scene", title: "Now Playing scene", kinds: [SCENE_KIND, SCENE_SOURCE_KIND, SCENE_DESIGN_KIND] },
  { key: "home_scene", title: "Home scene", kinds: [8, 9, 10] },
];
export const ACCENT_NAMES = ["blue", "red", "orange", "green", "teal", "purple", "pink", "graphite"];
export const ACCENT_SHADES = ["selection_background", "selection_top", "selection_shine"];
const ASSET_ENTRY_SIZE = 16;
export const PALETTE_COLORS = 16;
export const HEX_DIGITS = "0123456789abcdef";
const HASH_START = 2166136261;
const HASH_PRIME = 16777619;

export const BOX_COLOR_FIELDS = ["fill_top", "fill_bottom", "border", "shine", "ink"];
export const BOX_NUMBER_FIELDS = [
  "border_width",
  "border_edges",
  "shine_height",
  "inset_across",
  "inset_down",
  "radius",
  "see_through",
];
export const EDGES = { top: 1, bottom: 2, left: 4, right: 8 };

export class ThemeError extends Error {}

export function checksum(bytes) {
  let value = HASH_START;
  for (const byte of bytes) {
    value = Math.imul(value ^ byte, HASH_PRIME) >>> 0;
  }
  return value;
}

export function rgb565(color) {
  const match = /^#([0-9a-fA-F]{6})$/.exec(color);
  if (!match) {
    throw new ThemeError(`${color} is not a colour like #1a2b3c`);
  }
  const value = parseInt(match[1], 16);
  const red = value >> 16;
  const green = (value >> 8) & 0xff;
  const blue = value & 0xff;
  return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

export function hexColor(value) {
  const red = value >> 11;
  const green = (value >> 5) & 0x3f;
  const blue = value & 0x1f;
  const wide = [(red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2)];
  return "#" + wide.map((part) => part.toString(16).padStart(2, "0")).join("");
}

function channelLight(value, bits) {
  const wide = ((value << (8 - bits)) | (value >> (2 * bits - 8))) / 255;
  return wide <= 0.04045 ? wide / 12.92 : ((wide + 0.055) / 1.055) ** 2.4;
}

function luminance(color) {
  return (
    0.2126 * channelLight(color >> 11, 5) +
    0.7152 * channelLight((color >> 5) & 0x3f, 6) +
    0.0722 * channelLight(color & 0x1f, 5)
  );
}

export function contrast(first, second) {
  const lights = [luminance(rgb565(first)), luminance(rgb565(second))];
  return (Math.max(...lights) + 0.05) / (Math.min(...lights) + 0.05);
}

function wallpaperContrast(color, theme, share) {
  let lowest = 21;
  for (const paper of Object.values(theme.wallpapers ?? {})) {
    const counts = new Array(PALETTE_COLORS).fill(0);
    for (const row of paper?.rows ?? []) {
      for (const mark of row) {
        counts[HEX_DIGITS.indexOf(mark)] += 1;
      }
    }
    const total = counts.reduce((sum, count) => sum + count, 0);
    const ranked = (paper?.palette ?? [])
      .map((shade, index) => [contrast(color, shade), counts[index]])
      .filter(([, count]) => count > 0)
      .sort((first, second) => first[0] - second[0]);
    let covered = 0;
    for (const [ratio, count] of ranked) {
      covered += count;
      if (covered > (total * share) / 100) {
        lowest = Math.min(lowest, ratio);
        break;
      }
    }
  }
  return lowest;
}

function lowestContrast(check, theme) {
  const colors = theme.colors;
  if (check.kind === "wallpaper") {
    return wallpaperContrast(colors[check.color], theme, check.share);
  }
  if (check.kind === "pair") {
    return contrast(colors[check.color], colors[check.on]);
  }
  const box = theme.boxes[check.box || theme.selection];
  if (check.kind === "ink") {
    return Math.min(
      contrast(colors[box.ink], colors[box.fill_top]),
      contrast(colors[box.ink], colors[box.fill_bottom]),
    );
  }
  return contrast(colors[box.fill_top], colors[theme.boxes[check.on].fill_top]);
}

export function lint(theme, schema) {
  const found = [];
  for (const check of schema.checks) {
    const ratio = lowestContrast(check, theme);
    if (ratio < check.floor) {
      found.push(`${check.what}: contrast ${ratio.toFixed(1)}, below ${check.floor.toFixed(1)}`);
    }
  }
  return found;
}

export function defaultTheme(schema) {
  return {
    name: "My theme",
    selection: schema.dark.selection,
    colors: { ...schema.dark.colors },
    boxes: Object.fromEntries(
      schema.boxes.map((name) => [name, { ...schema.dark.boxes[name] }]),
    ),
    spacing: Object.fromEntries(schema.spacing.map((entry) => [entry.name, null])),
    icons: Object.fromEntries(schema.icons.map((entry) => [entry.name, null])),
    wallpapers: Object.fromEntries(schema.wallpapers.map((entry) => [entry.name, null])),
    fonts: Object.fromEntries(schema.fonts.map((entry) => [entry.name, null])),
    layouts: Object.fromEntries(
      schema.layouts.map((key) => [key, Object.fromEntries(schema.layout.map((entry) => [entry.name, null]))]),
    ),
    accents: (schema.dark.accents ?? []).map((accent) => ({ ...accent })),
    now_playing_scene: null,
    home_scene: null,
    cover_flow: "theme",
    home_art: true,
  };
}

function iconLevels(row) {
  const levels = Array.from(row.toLowerCase(), (mark) => (mark === "0" ? 0 : ICON_MARKS.indexOf(mark)));
  if (levels.includes(-1)) {
    throw new ThemeError("an icon row is made of . and 1-9, a-f, from clear to solid");
  }
  return levels.length % 2 === 1 ? [...levels, 0] : levels;
}

function iconMask(rows, entry) {
  const width = rows[0]?.length ?? 0;
  if (width === 0 || rows.some((row) => row.length !== width)) {
    throw new ThemeError(`${entry.name}: every row needs the same length`);
  }
  if (width > entry.width || rows.length > entry.height) {
    throw new ThemeError(`${entry.name}: at most ${entry.width} wide and ${entry.height} high`);
  }
  const levels = rows.flatMap(iconLevels);
  const bytes = [];
  for (let index = 0; index < levels.length; index += 2) {
    bytes.push((levels[index] << 4) | levels[index + 1]);
  }
  return { width, height: rows.length, bytes };
}

function checkIconPairs(masks, schema) {
  for (const [frame, level] of schema.icon_pairs) {
    const [first, second] = [masks[frame], masks[level]];
    const alike = first && second && first.width === second.width && first.height === second.height;
    if ((first || second) && !alike) {
      throw new ThemeError(`${frame} and ${level} go together, at one size`);
    }
  }
}

function iconSection(theme, schema) {
  const given = schema.icons.filter((entry) => theme.icons?.[entry.name]);
  const masks = Object.fromEntries(given.map((entry) => [entry.name, iconMask(theme.icons[entry.name], entry)]));
  checkIconPairs(masks, schema);
  const count = schema.icons.findLastIndex((entry) => masks[entry.name]) + 1;
  const table = [];
  const data = [];
  for (const entry of schema.icons.slice(0, count)) {
    const mask = masks[entry.name] ?? { width: 0, height: 0, bytes: [] };
    const offset = mask.width > 0 ? data.length : 0;
    table.push(mask.width, mask.height, offset & 0xff, offset >> 8);
    data.push(...mask.bytes);
  }
  return { count, bytes: [...table, ...data] };
}

function accentBytes(accent) {
  const name = typeof accent.name === "string" ? accent.name : "";
  const encoded = new TextEncoder().encode(name);
  if (encoded.length === 0 || encoded.length >= ACCENT_NAME_SIZE) {
    throw new ThemeError(`Give each accent a name, up to ${ACCENT_NAME_SIZE - 1} letters`);
  }
  const bytes = new Uint8Array(ACCENT_SIZE);
  const view = new DataView(bytes.buffer);
  ACCENT_SHADES.forEach((key, index) => view.setUint16(2 * index, rgb565(accent[key]), true));
  const standard = ACCENT_NAMES.indexOf(name.toLowerCase());
  bytes[6] = standard >= 0 ? standard : CUSTOM_ACCENT;
  if (standard < 0) {
    bytes.set(encoded, 7);
  }
  return bytes;
}

function accentSection(theme) {
  const accents = theme.accents ?? [];
  if (accents.length > MOST_ACCENTS) {
    throw new ThemeError(`a theme offers at most ${MOST_ACCENTS} accents`);
  }
  const bytes = new Uint8Array(ACCENT_SIZE * accents.length);
  accents.forEach((accent, index) => bytes.set(accentBytes(accent), ACCENT_SIZE * index));
  return { count: accents.length, bytes };
}

function nameBytes(name) {
  const encoded = new TextEncoder().encode(name);
  if (encoded.length === 0 || encoded.length >= NAME_SIZE) {
    throw new ThemeError(`Give the theme a name, up to ${NAME_SIZE - 1} letters`);
  }
  const padded = new Uint8Array(NAME_SIZE);
  padded.set(encoded);
  return padded;
}

function boxBytes(box, schema) {
  const ids = BOX_COLOR_FIELDS.map((field) => {
    const id = schema.colors.indexOf(box[field]);
    if (id < 0) {
      throw new ThemeError(`${box[field]} is not a colour name`);
    }
    return id;
  });
  return [...ids, ...BOX_NUMBER_FIELDS.map((field) => (box[field] ?? 0) & 0xff)];
}

function spacingBytes(theme, schema) {
  return schema.spacing.map((entry) => {
    const value = theme.spacing[entry.name];
    if (value === null || value === undefined) {
      return 0;
    }
    if (!Number.isInteger(value) || value < entry.least || value > entry.most) {
      throw new ThemeError(`${entry.name} must be from ${entry.least} to ${entry.most}`);
    }
    return value + 1;
  });
}

function wallpaperSection(paper, entry) {
  const { palette, rows } = paper;
  const width = rows[0]?.length ?? 0;
  if (palette.length < 1 || palette.length > PALETTE_COLORS) {
    throw new ThemeError(`${entry.name}: the palette holds 1 to ${PALETTE_COLORS} colours`);
  }
  if (width < 2 || width % 2 === 1 || rows.some((row) => row.length !== width)) {
    throw new ThemeError(`${entry.name}: every row needs the same, even length`);
  }
  if (width > entry.width || rows.length > entry.height) {
    throw new ThemeError(`${entry.name}: at most ${entry.width} wide and ${entry.height} high`);
  }
  const bytes = new Uint8Array(2 * PALETTE_COLORS + (width / 2) * rows.length);
  const view = new DataView(bytes.buffer);
  palette.forEach((color, index) => view.setUint16(2 * index, rgb565(color), true));
  rows.forEach((row, line) => {
    const marks = Array.from(row.toLowerCase(), (mark) => HEX_DIGITS.indexOf(mark));
    if (marks.some((mark) => mark < 0 || mark >= palette.length)) {
      throw new ThemeError(`${entry.name}: a row is made of the palette's numbers, 0-9 then a-f`);
    }
    for (let column = 0; column < width; column += 2) {
      bytes[2 * PALETTE_COLORS + (line * width + column) / 2] = (marks[column] << 4) | marks[column + 1];
    }
  });
  return { width, height: rows.length, bytes };
}

function wallpaperSections(theme, schema) {
  return schema.wallpapers
    .map((entry, kind) => [entry, kind, theme.wallpapers?.[entry.name]])
    .filter(([, , paper]) => paper)
    .map(([entry, kind, paper]) => {
      const wall = wallpaperSection(paper, entry);
      const head = [kind, 0, wall.width & 0xff, wall.width >> 8, wall.height & 0xff, wall.height >> 8, 0, 0];
      return { head, bytes: wall.bytes, memory: WALLPAPER_PAIR_BYTES + (wall.width / 2) * wall.height };
    });
}

function assetTable(assets, firstOffset) {
  const table = new Uint8Array(ASSET_ENTRY_SIZE * assets.length);
  const view = new DataView(table.buffer);
  let offset = firstOffset;
  assets.forEach((asset, index) => {
    const field = ASSET_ENTRY_SIZE * index;
    table.set(asset.head, field);
    view.setUint32(field + 8, offset, true);
    view.setUint32(field + 12, checksum(asset.bytes), true);
    offset += asset.bytes.length;
  });
  return table;
}

const WALLPAPER_PAIR_BYTES = 1024;
const FIRST_FONT_KIND = 2;
const MAX_FONT_GLYPHS = 512;
const GLYPH_RECORD_SIZE = 10;

function aligned(size) {
  return Math.ceil(size / 4) * 4;
}

function wholeNumber(value, least, most, what) {
  if (!Number.isInteger(value) || value < least || value > most) {
    throw new ThemeError(`${what} must be a whole number from ${least} to ${most}`);
  }
  return value;
}

function glyphCode(glyph, where) {
  const code = typeof glyph.char === "string" && glyph.char.length === 1 ? glyph.char.charCodeAt(0) : -1;
  if (code < 0) {
    throw new ThemeError(`${where}: each glyph names one character`);
  }
  return code;
}

function glyphLevels(glyph, where) {
  const rows = glyph.rows ?? [];
  const width = rows[0]?.length ?? 0;
  if (rows.some((row) => row.length !== width) || width > 255 || rows.length > 255) {
    throw new ThemeError(`${where}: every row needs the same length, at most 255`);
  }
  const levels = rows.flatMap((row) => Array.from(row.toLowerCase(), (mark) => (mark === "0" ? 0 : ICON_MARKS.indexOf(mark))));
  if (levels.includes(-1)) {
    throw new ThemeError(`${where}: a row is made of . and 1-9, a-f, from clear to solid`);
  }
  return { width, height: rows.length, levels };
}

function fontAsset(font, entry, kind) {
  const where = `font ${entry.name}`;
  const ascent = wholeNumber(font.ascent, 1, entry.height, `${where}: ascent`);
  const descent = wholeNumber(font.descent, 0, entry.height, `${where}: descent`);
  if (ascent + descent > entry.height) {
    throw new ThemeError(`${where}: ascent and descent add up to ${ascent + descent}, more than ${entry.height}`);
  }
  if (!Array.isArray(font.glyphs) || font.glyphs.length < 1 || font.glyphs.length > MAX_FONT_GLYPHS) {
    throw new ThemeError(`${where}: 1 to ${MAX_FONT_GLYPHS} glyphs`);
  }
  const ordered = font.glyphs.map((glyph) => [glyphCode(glyph, where), glyph]).sort((first, second) => first[0] - second[0]);
  const records = new Uint8Array(GLYPH_RECORD_SIZE * ordered.length);
  const view = new DataView(records.buffer);
  const bitmap = [];
  ordered.forEach(([code, glyph], index) => {
    if (index > 0 && ordered[index - 1][0] === code) {
      throw new ThemeError(`${where}: ${glyph.char} is given twice`);
    }
    const { width, height, levels } = glyphLevels(glyph, where);
    const at = GLYPH_RECORD_SIZE * index;
    view.setUint16(at, code, true);
    view.setUint16(at + 2, bitmap.length, true);
    records.set([width, height], at + 4);
    view.setInt8(at + 6, wholeNumber(glyph.left, -128, 127, `${where}: left`));
    view.setInt8(at + 7, wholeNumber(glyph.top, -128, 127, `${where}: top`));
    records[at + 8] = wholeNumber(glyph.advance, 0, 255, `${where}: advance`);
    const padded = levels.length % 2 === 1 ? [...levels, 0] : levels;
    for (let level = 0; level < padded.length; level += 2) {
      bitmap.push((padded[level] << 4) | padded[level + 1]);
    }
  });
  if (bitmap.length > 0xffff) {
    throw new ThemeError(`${where}: the glyphs need more than 65,535 bytes`);
  }
  const bytes = new Uint8Array(records.length + bitmap.length);
  bytes.set(records);
  bytes.set(bitmap, records.length);
  const head = [kind, ascent, ordered.length & 0xff, ordered.length >> 8, bitmap.length & 0xff, bitmap.length >> 8, descent, 0];
  return { head, bytes, memory: aligned(records.length) + aligned(bitmap.length) };
}

function fontAssets(theme, schema) {
  return schema.fonts
    .map((entry, index) => [entry, index, theme.fonts?.[entry.name]])
    .filter(([, , font]) => font)
    .map(([entry, index, font]) => fontAsset(font, entry, FIRST_FONT_KIND + index));
}

function oneScene(theme, schema, { key, title, kinds }) {
  const scene = theme[key];
  if (!scene || typeof scene.script !== "string") {
    return [];
  }
  let compiled;
  try {
    compiled = compileScene(scene.script, schema.scene);
  } catch (error) {
    if (error instanceof SceneError) {
      throw new ThemeError(`${title}: ${error.message}`);
    }
    throw error;
  }
  const [codeKind, sourceKind, designKind] = kinds;
  const chrome = scene.chrome && Object.keys(scene.chrome).length > 0 ? scene.chrome : null;
  const shades = chrome ? schema.colors.map((name) => rgb565(chrome[name] ?? theme.colors[name])) : [];
  const size = 2 * shades.length + compiled.code.length + compiled.strings.length;
  const bytes = new Uint8Array(size);
  const view = new DataView(bytes.buffer);
  shades.forEach((shade, index) => view.setUint16(2 * index, shade, true));
  bytes.set(compiled.code, 2 * shades.length);
  bytes.set(compiled.strings, 2 * shades.length + compiled.code.length);
  const [code, strings] = [compiled.code.length, compiled.strings.length];
  const head = [codeKind, chrome ? SCENE_CHROME : 0, code & 0xff, code >> 8, strings & 0xff, strings >> 8, 0, 0];
  const source = new TextEncoder().encode(scene.script);
  const found = [
    { head, bytes, memory: 0 },
    { head: [sourceKind, 0, 0, 0, 0, 0, 0, 0], bytes: source, memory: 0 },
  ];
  if (typeof scene.design === "string") {
    found.push({ head: [designKind, 0, 0, 0, 0, 0, 0, 0], bytes: new TextEncoder().encode(scene.design), memory: 0 });
  }
  return found;
}

export function sceneBytes(theme, schema) {
  return SCENES.flatMap((scene) => oneScene(theme, schema, scene))
    .filter((asset) => SCENES.some((scene) => scene.kinds[0] === asset.head[0]))
    .reduce((sum, asset) => sum + Math.ceil(asset.bytes.length / 4) * 4, 0);
}

function sceneAssets(theme, schema) {
  const found = SCENES.flatMap((scene) => oneScene(theme, schema, scene));
  const sections = found.filter((asset) => SCENES.some((scene) => scene.kinds[0] === asset.head[0]));
  const total = sections.reduce((sum, asset) => sum + Math.ceil(asset.bytes.length / 4) * 4, 0);
  if (total > SCENE_MOST_BYTES) {
    const what = sections.length === 1 ? "This screen's design is" : "The Home and Now Playing designs together are";
    throw new ThemeError(`${what} too big for the player (${total.toLocaleString("en")} of ${SCENE_MOST_BYTES.toLocaleString("en")} bytes). Remove a few layers.`);
  }
  return found;
}

function checkMemory(walls, fonts, schema) {
  const needed = fonts.reduce((sum, font) => sum + font.memory, 0) + Math.max(0, ...walls.map((wall) => wall.memory));
  if (needed > schema.asset_memory) {
    throw new ThemeError(
      `the fonts and wallpaper need ${needed.toLocaleString("en")} bytes of the player's memory, more than its ` +
        `${schema.asset_memory.toLocaleString("en")}; use a smaller or repeating wallpaper, or fewer fonts`,
    );
  }
  return needed;
}

export function assetMemory(theme, schema) {
  return checkMemory(wallpaperSections(theme, schema), fontAssets(theme, schema), schema);
}

export function memoryParts(theme, schema) {
  const fonts = {};
  schema.fonts.forEach((entry, index) => {
    const font = theme.fonts?.[entry.name];
    if (!font) {
      return;
    }
    try {
      fonts[entry.name] = fontAsset(font, entry, FIRST_FONT_KIND + index).memory;
    } catch (error) {
      if (!(error instanceof ThemeError)) {
        throw error;
      }
      fonts[entry.name] = 0;
    }
  });
  let wallpaper = 0;
  try {
    wallpaper = Math.max(0, ...wallpaperSections(theme, schema).map((wall) => wall.memory));
  } catch (error) {
    if (!(error instanceof ThemeError)) {
      throw error;
    }
  }
  const used = Object.values(fonts).reduce((sum, bytes) => sum + bytes, wallpaper);
  return { fonts, wallpaper, used, limit: schema.asset_memory };
}


const LAYOUT_BOX_SIZE = 8;
const LAYOUT_REACH = 320;
const LAYOUT_MOST_COVER = 170;
const LAYOUT_UNSET = 0xff;
export const ALIGNS = ["left", "center", "right"];

function placeBox(place, entry, where) {
  if (place.hide === true) {
    return [0, 0, 0, entry.height, 0];
  }
  const x = wholeNumber(place.x, -LAYOUT_REACH, LAYOUT_REACH, `${where}: x`);
  const y = wholeNumber(place.y, -LAYOUT_REACH, LAYOUT_REACH, `${where}: y`);
  if (entry.name === "cover") {
    return [x, y, wholeNumber(place.size, 1, LAYOUT_MOST_COVER, `${where}: size`), 0, 0];
  }
  const width = wholeNumber(place.width, 1, LAYOUT_REACH, `${where}: width`);
  const height = wholeNumber(place.height ?? entry.height, 1, 255, `${where}: height`);
  const align = ALIGNS.indexOf(place.align ?? "left");
  if (align < 0) {
    throw new ThemeError(`${where}: align is left, center or right`);
  }
  return [x, y, width, height, align];
}

function layoutBytes(theme, schema, key) {
  const places = theme.layouts?.[key] ?? {};
  if (!schema.layout.some((entry) => places[entry.name])) {
    return new Uint8Array(0);
  }
  const bytes = new Uint8Array(LAYOUT_BOX_SIZE * schema.layout.length);
  const view = new DataView(bytes.buffer);
  schema.layout.forEach((entry, index) => {
    const place = places[entry.name];
    const [x, y, width, height, align] = place ? placeBox(place, entry, `${key}.${entry.name}`) : [0, 0, 0, 0, LAYOUT_UNSET];
    const at = LAYOUT_BOX_SIZE * index;
    view.setInt16(at, x, true);
    view.setInt16(at + 2, y, true);
    view.setUint16(at + 4, width, true);
    bytes.set([height, align], at + 6);
  });
  return bytes;
}

function placeFrom(view, at, entry) {
  const [x, y, width] = [view.getInt16(at, true), view.getInt16(at + 2, true), view.getUint16(at + 4, true)];
  const [height, align] = [view.getUint8(at + 6), view.getUint8(at + 7)];
  if (align >= ALIGNS.length) {
    return null;
  }
  if (width === 0) {
    return { hide: true };
  }
  return entry.name === "cover" ? { x, y, size: width } : { x, y, width, height, align: ALIGNS[align] };
}

function unpackAccents(bytes, view, at, theme) {
  const header = view.getUint16(6, true);
  const count = header >= LONG_HEADER_SIZE ? bytes[51] : 0;
  if (at + ACCENT_SIZE * count > bytes.length) {
    throw new ThemeError("the theme file is cut short");
  }
  theme.accents = [];
  for (let index = 0; index < Math.min(count, MOST_ACCENTS); index++) {
    const field = at + ACCENT_SIZE * index;
    const text = new TextDecoder().decode(bytes.subarray(field + 7, field + 7 + ACCENT_NAME_SIZE));
    const standard = ACCENT_NAMES[bytes[field + 6]];
    const name = standard ? standard[0].toUpperCase() + standard.slice(1) : text.replace(/\0.*$/s, "");
    const accent = { name };
    ACCENT_SHADES.forEach((key, shade) => {
      accent[key] = hexColor(view.getUint16(field + 2 * shade, true));
    });
    theme.accents.push(accent);
  }
  theme.cover_flow = header >= ASSET_HEADER_SIZE && (bytes[52] & FLAG_DARK_COVER_FLOW) ? "dark" : "theme";
  theme.home_art = !(header >= ASSET_HEADER_SIZE && (bytes[52] & FLAG_NO_HOME_ART));
  return ACCENT_SIZE * count;
}

function unpackLayouts(bytes, view, at, theme, schema) {
  const counts = view.getUint16(6, true) >= LONG_HEADER_SIZE ? [bytes[49], bytes[50]] : [0, 0];
  let offset = at;
  schema.layouts.forEach((key, side) => {
    schema.layout.slice(0, counts[side]).forEach((entry, index) => {
      theme.layouts[key][entry.name] = placeFrom(view, offset + LAYOUT_BOX_SIZE * index, entry);
    });
    offset += LAYOUT_BOX_SIZE * counts[side];
  });
  return offset - at;
}

function tomlPlace(key, name, place) {
  const lines = [`[${key}.${name}]`];
  if (place.hide) {
    lines.push("hide = true");
  } else if (name === "cover") {
    lines.push(`x = ${place.x}`, `y = ${place.y}`, `size = ${place.size}`);
  } else {
    lines.push(`x = ${place.x}`, `y = ${place.y}`, `width = ${place.width}`, `height = ${place.height}`, `align = "${place.align}"`);
  }
  return [...lines, ""];
}

function putHeader(bytes, counts) {
  const view = new DataView(bytes.buffer);
  bytes.set(MAGIC);
  view.setUint16(4, VERSION, true);
  view.setUint16(6, counts.header, true);
  view.setUint32(8, counts.core, true);
  view.setUint16(16, counts.colors, true);
  view.setUint16(18, counts.boxes, true);
  bytes.set([BOX_SIZE, counts.selection, counts.spaces, counts.icons], 20);
  if (counts.header === ASSET_HEADER_SIZE) {
    bytes.set([counts.walls, counts.wide, counts.tall, counts.accents, counts.flags], HEADER_SIZE);
  }
}

export function pack(theme, schema) {
  const selection = schema.boxes.indexOf(theme.selection);
  if (selection < 0) {
    throw new ThemeError(`${theme.selection} is not a box name`);
  }
  const spaces = spacingBytes(theme, schema);
  const icons = iconSection(theme, schema);
  const walls = wallpaperSections(theme, schema);
  const fonts = fontAssets(theme, schema);
  checkMemory(walls, fonts, schema);
  const assets = [...walls, ...fonts, ...sceneAssets(theme, schema)];
  const [wide, tall] = schema.layouts.map((key) => layoutBytes(theme, schema, key));
  const accents = accentSection(theme);
  const flags = (theme.cover_flow === "dark" ? FLAG_DARK_COVER_FLOW : 0) | (theme.home_art === false ? FLAG_NO_HOME_ART : 0);
  const long = assets.length > 0 || wide.length + tall.length > 0 || accents.count > 0 || flags !== 0;
  const header = long ? ASSET_HEADER_SIZE : HEADER_SIZE;
  const tables = 2 * schema.colors.length + BOX_SIZE * schema.boxes.length + spaces.length + wide.length +
    tall.length + accents.bytes.length;
  const core = header + tables + ASSET_ENTRY_SIZE * assets.length + icons.bytes.length;
  if (core > MAX_SIZE) {
    throw new ThemeError(`The colours, shapes and icons take ${core.toLocaleString("en")} bytes, more than the ${MAX_SIZE.toLocaleString("en")} the player keeps for them. Make some icons smaller, or draw fewer.`);
  }
  const bytes = new Uint8Array(core + assets.reduce((sum, asset) => sum + asset.bytes.length, 0));
  const view = new DataView(bytes.buffer);
  putHeader(bytes, {
    header,
    core,
    colors: schema.colors.length,
    boxes: schema.boxes.length,
    selection,
    spaces: spaces.length,
    icons: icons.count,
    walls: assets.length,
    wide: wide.length / LAYOUT_BOX_SIZE,
    tall: tall.length / LAYOUT_BOX_SIZE,
    accents: accents.count,
    flags,
  });
  bytes.set(nameBytes(theme.name), 24);
  let offset = header;
  for (const name of schema.colors) {
    view.setUint16(offset, rgb565(theme.colors[name]), true);
    offset += 2;
  }
  for (const name of schema.boxes) {
    bytes.set(boxBytes(theme.boxes[name], schema), offset);
    offset += BOX_SIZE;
  }
  const parts = [spaces, wide, tall, accents.bytes, assetTable(assets, core), icons.bytes,
    ...assets.map((asset) => asset.bytes)];
  for (const part of parts) {
    bytes.set(part, offset);
    offset += part.length;
  }
  view.setUint32(12, checksum(bytes.subarray(CHECKED_FROM, core)), true);
  return bytes;
}

function checkHeader(bytes, view) {
  const magic = MAGIC.every((byte, index) => bytes[index] === byte);
  if (bytes.length < HEADER_SIZE || !magic) {
    throw new ThemeError("this is not a theme file");
  }
  if (view.getUint16(4, true) !== VERSION) {
    throw new ThemeError("this theme file needs a newer editor");
  }
  const core = view.getUint32(8, true);
  if (core < HEADER_SIZE || core > bytes.length) {
    throw new ThemeError("the theme file is cut short");
  }
  if (view.getUint32(12, true) !== checksum(bytes.subarray(CHECKED_FROM, core))) {
    throw new ThemeError("the theme file is damaged");
  }
  return core;
}

function unpackBox(bytes, start, length, box, schema) {
  const known = [...BOX_COLOR_FIELDS, ...BOX_NUMBER_FIELDS];
  const taken = { ...box };
  for (let index = 0; index < Math.min(length, known.length); index++) {
    const value = bytes[start + index];
    const field = known[index];
    taken[field] = index < BOX_COLOR_FIELDS.length ? schema.colors[value] : value;
  }
  return BOX_COLOR_FIELDS.every((field) => taken[field] !== undefined) ? taken : box;
}

function iconRows(bytes, at, width, height) {
  const stride = (width + 1) >> 1;
  const rows = [];
  for (let row = 0; row < height; row++) {
    let marks = "";
    for (let column = 0; column < width; column++) {
      const pair = bytes[at + row * stride + (column >> 1)];
      marks += ICON_MARKS[column % 2 === 0 ? pair >> 4 : pair & 0x0f];
    }
    rows.push(marks);
  }
  return rows;
}

function unpackIcons(bytes, tableAt, theme, schema) {
  const dataAt = tableAt + ICON_ENTRY_SIZE * bytes[ICON_FIELD];
  if (dataAt > bytes.length) {
    throw new ThemeError("the theme file is cut short");
  }
  schema.icons.slice(0, bytes[ICON_FIELD]).forEach((entry, index) => {
    const field = tableAt + ICON_ENTRY_SIZE * index;
    const [width, height] = [bytes[field], bytes[field + 1]];
    const at = dataAt + bytes[field + 2] + (bytes[field + 3] << 8);
    const fits = width <= entry.width && height <= entry.height;
    if (width > 0 && height > 0 && fits && at + ((width + 1) >> 1) * height <= bytes.length) {
      theme.icons[entry.name] = iconRows(bytes, at, width, height);
    }
  });
}

function wallpaperFrom(bytes, view, at, size) {
  const palette = [];
  for (let index = 0; index < PALETTE_COLORS; index++) {
    palette.push(hexColor(view.getUint16(at + 2 * index, true)));
  }
  const rows = [];
  let used = 0;
  for (let row = 0; row < size.height; row++) {
    let marks = "";
    for (let column = 0; column < size.width; column++) {
      const pair = bytes[at + 2 * PALETTE_COLORS + ((row * size.width + column) >> 1)];
      const mark = column % 2 === 0 ? pair >> 4 : pair & 0x0f;
      used = Math.max(used, mark);
      marks += HEX_DIGITS[mark];
    }
    rows.push(marks);
  }
  const kept = Math.max(used, palette.findLastIndex((color) => color !== "#000000")) + 1;
  return { palette: palette.slice(0, kept), rows };
}

function fontFrom(bytes, view, at, entry) {
  const font = { ascent: entry.ascent, descent: entry.descent, glyphs: [] };
  const bitmapAt = at + GLYPH_RECORD_SIZE * entry.count;
  for (let index = 0; index < entry.count; index++) {
    const record = at + GLYPH_RECORD_SIZE * index;
    const [width, height] = [bytes[record + 4], bytes[record + 5]];
    const start = view.getUint16(record + 2, true);
    const rows = [];
    for (let row = 0; row < height; row++) {
      let marks = "";
      for (let column = 0; column < width; column++) {
        const place = row * width + column;
        const pair = bytes[bitmapAt + start + (place >> 1)];
        marks += ICON_MARKS[place % 2 === 0 ? pair >> 4 : pair & 0x0f];
      }
      rows.push(marks);
    }
    font.glyphs.push({
      char: String.fromCharCode(view.getUint16(record, true)),
      left: view.getInt8(record + 6),
      top: view.getInt8(record + 7),
      advance: bytes[record + 8],
      rows,
    });
  }
  return font;
}

function unpackFont(bytes, view, field, theme, schema) {
  const entry = schema.fonts[bytes[field] - FIRST_FONT_KIND];
  const found = {
    ascent: bytes[field + 1],
    descent: bytes[field + 6],
    count: view.getUint16(field + 2, true),
    bitmap: view.getUint16(field + 4, true),
  };
  const at = view.getUint32(field + 8, true);
  const fits = entry && found.count >= 1 && found.ascent >= 1 && found.ascent + found.descent <= entry.height;
  if (fits && at + GLYPH_RECORD_SIZE * found.count + found.bitmap <= bytes.length) {
    theme.fonts[entry.name] = fontFrom(bytes, view, at, found);
  }
}

function sectionEnd(bytes, view, tableAt, count, at) {
  let end = bytes.length;
  for (let index = 0; index < count; index++) {
    const offset = view.getUint32(tableAt + ASSET_ENTRY_SIZE * index + 8, true);
    end = offset > at && offset < end ? offset : end;
  }
  return end;
}

function sceneOf(kind) {
  return SCENES.find((scene) => scene.kinds.includes(kind)) ?? null;
}

function unpackScene(bytes, view, field, theme, schema, end) {
  const at = view.getUint32(field + 8, true);
  const { key, kinds } = sceneOf(bytes[field]);
  const scene = theme[key] ?? { script: null, chrome: null };
  if (bytes[field] === kinds[1]) {
    scene.script = new TextDecoder().decode(bytes.subarray(at, end));
  } else if (bytes[field] === kinds[2]) {
    scene.design = new TextDecoder().decode(bytes.subarray(at, end));
  } else if (bytes[field + 1] & SCENE_CHROME) {
    scene.chrome = Object.fromEntries(
      schema.colors.map((name, index) => [name, hexColor(view.getUint16(at + 2 * index, true))]),
    );
  }
  theme[key] = scene;
}

function unpackWallpapers(bytes, view, tableAt, theme, schema) {
  const count = view.getUint16(6, true) >= LONG_HEADER_SIZE ? bytes[HEADER_SIZE] : 0;
  for (let index = 0; index < count; index++) {
    const field = tableAt + ASSET_ENTRY_SIZE * index;
    if (sceneOf(bytes[field])) {
      const end = sectionEnd(bytes, view, tableAt, count, view.getUint32(field + 8, true));
      unpackScene(bytes, view, field, theme, schema, end);
      continue;
    }
    if (bytes[field] >= FIRST_FONT_KIND) {
      unpackFont(bytes, view, field, theme, schema);
      continue;
    }
    const entry = schema.wallpapers[bytes[field]];
    const size = { width: view.getUint16(field + 2, true), height: view.getUint16(field + 4, true) };
    const at = view.getUint32(field + 8, true);
    const shaped = size.width >= 2 && size.width % 2 === 0 && size.height >= 1;
    const fits = entry && size.width <= entry.width && size.height <= entry.height;
    if (shaped && fits && at + 2 * PALETTE_COLORS + (size.width / 2) * size.height <= bytes.length) {
      theme.wallpapers[entry.name] = wallpaperFrom(bytes, view, at, size);
    }
  }
  return ASSET_ENTRY_SIZE * count;
}

export function unpack(bytes, schema) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const core = checkHeader(bytes, view);
  const theme = defaultTheme(schema);
  const header = view.getUint16(6, true);
  const colors = view.getUint16(16, true);
  const boxes = view.getUint16(18, true);
  const boxSize = bytes[20];
  const boxesAt = header + 2 * colors;
  const spacingAt = boxesAt + boxes * boxSize;
  if (spacingAt + bytes[22] > core) {
    throw new ThemeError("the theme file is cut short");
  }
  const text = new TextDecoder().decode(bytes.subarray(24, 24 + NAME_SIZE));
  theme.name = text.replace(/\0.*$/s, "");
  schema.colors.slice(0, colors).forEach((name, index) => {
    theme.colors[name] = hexColor(view.getUint16(header + 2 * index, true));
  });
  schema.boxes.slice(0, boxes).forEach((name, index) => {
    theme.boxes[name] = unpackBox(bytes, boxesAt + index * boxSize, boxSize, theme.boxes[name], schema);
  });
  schema.spacing.slice(0, bytes[22]).forEach((entry, index) => {
    const stored = bytes[spacingAt + index];
    theme.spacing[entry.name] = stored === 0 ? null : stored - 1;
  });
  theme.selection = schema.boxes[bytes[21]] ?? schema.dark.selection;
  const layoutsAt = spacingAt + bytes[22];
  const accentsAt = layoutsAt + unpackLayouts(bytes, view, layoutsAt, theme, schema);
  const wallsAt = accentsAt + unpackAccents(bytes, view, accentsAt, theme);
  unpackIcons(bytes, wallsAt + unpackWallpapers(bytes, view, wallsAt, theme, schema), theme, schema);
  for (const { key } of SCENES) {
    if (theme[key] && typeof theme[key].script !== "string") {
      theme[key] = null;
    }
  }
  return theme;
}

export function opacityPercent(seeThrough) {
  return Math.round(((OPAQUE - seeThrough) * 100) / OPAQUE);
}

export function seeThroughOf(percent) {
  return OPAQUE - Math.floor((percent * OPAQUE + 50) / 100);
}

function tomlBox(box) {
  const lines = [];
  const fill = box.fill_top === box.fill_bottom
    ? `"${box.fill_top}"`
    : `["${box.fill_top}", "${box.fill_bottom}"]`;
  lines.push(`fill = ${fill}`);
  if (box.border_width > 0) {
    const edges = Object.keys(EDGES).filter((edge) => box.border_edges & EDGES[edge]);
    lines.push(`border = "${box.border}"`, `border_width = ${box.border_width}`);
    lines.push(edges.length === 4 ? 'border_edges = "all"' : `border_edges = ${JSON.stringify(edges)}`);
  }
  if (box.shine_height > 0) {
    lines.push(`shine = "${box.shine}"`, `shine_height = ${box.shine_height}`);
  }
  lines.push(`ink = "${box.ink}"`);
  if (box.inset_across > 0 || box.inset_down > 0) {
    lines.push(`inset = [${box.inset_across}, ${box.inset_down}]`);
  }
  if (box.radius > 0) {
    lines.push(`radius = ${box.radius}`);
  }
  if (box.see_through > 0) {
    lines.push(`opacity = ${opacityPercent(box.see_through)}`);
  }
  return lines.join("\n");
}

export function toToml(theme, schema) {
  const parts = [`name = ${JSON.stringify(theme.name)}`];
  for (const key of ["author", "description"]) {
    if (typeof theme[key] === "string" && theme[key].trim() !== "") {
      parts.push(`${key} = ${JSON.stringify(theme[key].trim())}`);
    }
  }
  parts.push(`selection = "${theme.selection}"`);
  if (theme.home_art === false) {
    parts.push("home_art = false");
  }
  if (theme.cover_flow === "dark") {
    parts.push('cover_flow = "dark"');
  }
  parts.push("");
  const spacing = schema.spacing.filter((entry) => theme.spacing[entry.name] !== null);
  if (spacing.length > 0) {
    parts.push("[spacing]", ...spacing.map((entry) => `${entry.name} = ${theme.spacing[entry.name]}`), "");
  }
  parts.push("[colors]", ...schema.colors.map((name) => `${name} = "${theme.colors[name]}"`), "");
  for (const name of schema.boxes) {
    parts.push(`[boxes.${name}]`, tomlBox(theme.boxes[name]), "");
  }
  for (const entry of schema.wallpapers.filter((wall) => theme.wallpapers?.[wall.name])) {
    const paper = theme.wallpapers[entry.name];
    const palette = paper.palette.map((color) => `"${color}"`).join(", ");
    const rows = paper.rows.map((row) => `    "${row}",`);
    parts.push(`[${entry.name}]`, `palette = [${palette}]`, "rows = [", ...rows, "]", "");
  }
  for (const key of schema.layouts) {
    for (const entry of schema.layout.filter((place) => theme.layouts?.[key]?.[place.name])) {
      parts.push(...tomlPlace(key, entry.name, theme.layouts[key][entry.name]));
    }
  }
  for (const entry of schema.fonts.filter((slot) => theme.fonts?.[slot.name])) {
    const font = theme.fonts[entry.name];
    parts.push(`[fonts.${entry.name}]`, `ascent = ${font.ascent}`, `descent = ${font.descent}`, "glyphs = [");
    for (const glyph of font.glyphs) {
      const rows = glyph.rows.map((row) => `"${row}"`).join(", ");
      const where = `left = ${glyph.left}, top = ${glyph.top}, advance = ${glyph.advance}`;
      parts.push(`    { char = ${JSON.stringify(glyph.char)}, ${where}, rows = [${rows}] },`);
    }
    parts.push("]", "");
  }
  for (const { key } of SCENES) {
    const scene = theme[key];
    if (!scene || typeof scene.script !== "string") {
      continue;
    }
    const script = scene.script.includes("'''") ? JSON.stringify(scene.script) : `'''
${scene.script}'''`;
    parts.push(`[${key}]`, `script = ${script}`, "");
    if (typeof scene.design === "string") {
      parts.splice(parts.length - 1, 0, `design = ${JSON.stringify(scene.design)}`);
    }
    if (scene.chrome) {
      parts.push(`[${key}.chrome]`,
        ...schema.colors.map((name) => `${name} = "${scene.chrome[name]}"`), "");
    }
  }
  for (const accent of theme.accents ?? []) {
    parts.push("[[accents]]", `name = ${JSON.stringify(accent.name)}`,
      ...ACCENT_SHADES.map((key) => `${key} = "${accent[key]}"`), "");
  }
  const icons = schema.icons.filter((entry) => theme.icons?.[entry.name]);
  if (icons.length > 0) {
    parts.push("[icons]");
    for (const entry of icons) {
      const rows = theme.icons[entry.name].map((row) => `    "${row}",`);
      parts.push(`${entry.name} = [`, ...rows, "]");
    }
    parts.push("");
  }
  return parts.join("\n");
}
