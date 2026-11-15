import { ICON_MARKS } from "./theme_format.js";

const LABELS = { title: "Titles", body: "List text", small: "Small text" };
const SAMPLE = "Afterglow Motel · 2021";
const LEAST_SIZE = 8;
const MOST_SIZE = 32;
const FULL = 255;
const SOLID = 15;
let families = 0;

function tag(view, at) {
  return String.fromCharCode(...[0, 1, 2, 3].map((index) => view.getUint8(at + index)));
}

function cmapAt(view) {
  const tables = view.getUint16(4);
  for (let index = 0; index < tables; index++) {
    const record = 12 + 16 * index;
    if (tag(view, record) === "cmap") {
      return view.getUint32(record + 8);
    }
  }
  throw new Error("the font has no character map");
}

function bestSubtable(view, cmap) {
  let best = null;
  for (let index = 0; index < view.getUint16(cmap + 2); index++) {
    const record = cmap + 4 + 8 * index;
    const [platform, encoding] = [view.getUint16(record), view.getUint16(record + 2)];
    const at = cmap + view.getUint32(record + 4);
    const unicode = platform === 0 || (platform === 3 && (encoding === 1 || encoding === 10));
    const format = view.getUint16(at);
    if (unicode && (format === 12 || (format === 4 && best === null))) {
      best = at;
    }
  }
  if (best === null) {
    throw new Error("the font has no Unicode character map");
  }
  return best;
}

function format4Has(view, at, code) {
  const segments = view.getUint16(at + 6) / 2;
  const ends = at + 14;
  const starts = ends + 2 * segments + 2;
  for (let segment = 0; segment < segments; segment++) {
    const [start, end] = [view.getUint16(starts + 2 * segment), view.getUint16(ends + 2 * segment)];
    if (code < start || code > end) {
      continue;
    }
    const delta = view.getInt16(starts + 2 * segments + 2 * segment);
    const rangeAt = starts + 4 * segments + 2 * segment;
    const range = view.getUint16(rangeAt);
    const glyph = range === 0 ? code : view.getUint16(rangeAt + range + 2 * (code - start));
    return range === 0 || glyph !== 0 ? (glyph + delta) % 65536 !== 0 : false;
  }
  return false;
}

function format12Has(view, at, code) {
  for (let group = 0; group < view.getUint32(at + 12); group++) {
    const record = at + 16 + 12 * group;
    if (code >= view.getUint32(record) && code <= view.getUint32(record + 4)) {
      return view.getUint32(record + 8) + (code - view.getUint32(record)) !== 0;
    }
  }
  return false;
}

export function coveredCodes(buffer, ranges) {
  const view = new DataView(buffer);
  const at = bestSubtable(view, cmapAt(view));
  const has = view.getUint16(at) === 12 ? format12Has : format4Has;
  const codes = [];
  for (const [first, last] of ranges) {
    for (let code = first; code <= last; code++) {
      if (has(view, at, code)) {
        codes.push(code);
      }
    }
  }
  return [...new Set(codes)].sort((first, second) => first - second);
}

function measured(context, family, size) {
  context.font = `${size}px "${family}"`;
  const metrics = context.measureText("Hg");
  return { size, ascent: Math.ceil(metrics.fontBoundingBoxAscent - 0.01), descent: Math.ceil(metrics.fontBoundingBoxDescent - 0.01) };
}

function fittingSize(context, family, wanted, limit) {
  const sizes = wanted ? [wanted] : Array.from({ length: MOST_SIZE - LEAST_SIZE + 1 }, (_, index) => MOST_SIZE - index);
  for (const size of sizes) {
    const found = measured(context, family, size);
    if (found.ascent + found.descent <= limit) {
      return found;
    }
  }
  throw new Error(`at ${wanted} px the letters are taller than the ${limit} px this text has room for`);
}

function inkBox(alphas, side) {
  let [left, top, right, bottom] = [side, side, -1, -1];
  for (let y = 0; y < side; y++) {
    for (let x = 0; x < side; x++) {
      if (Math.round((alphas[(y * side + x) * 4 + 3] * SOLID) / FULL) > 0) {
        [left, top, right, bottom] = [Math.min(left, x), Math.min(top, y), Math.max(right, x), Math.max(bottom, y)];
      }
    }
  }
  return right < 0 ? null : { left, top, right, bottom };
}

function glyphOf(context, code, pen) {
  const char = String.fromCharCode(code);
  const side = context.canvas.width;
  context.clearRect(0, 0, side, side);
  context.fillText(char, pen, pen);
  const alphas = context.getImageData(0, 0, side, side).data;
  const advance = Math.round(context.measureText(char).width);
  const box = inkBox(alphas, side);
  if (box === null) {
    return { char, left: 0, top: 0, advance, rows: [] };
  }
  const rows = [];
  for (let y = box.top; y <= box.bottom; y++) {
    let marks = "";
    for (let x = box.left; x <= box.right; x++) {
      marks += ICON_MARKS[Math.round((alphas[(y * side + x) * 4 + 3] * SOLID) / FULL)];
    }
    rows.push(marks);
  }
  return { char, left: box.left - pen, top: pen - box.top, advance, rows };
}

export async function rasterise(buffer, wanted, limit, ranges) {
  const family = `EchoThemeFont${families++}`;
  const face = new FontFace(family, buffer);
  await face.load();
  document.fonts.add(face);
  const found = fittingSize(document.createElement("canvas").getContext("2d"), family, wanted, limit);
  const canvas = document.createElement("canvas");
  canvas.width = canvas.height = found.size * 3;
  const context = canvas.getContext("2d", { willReadFrequently: true });
  context.font = `${found.size}px "${family}"`;
  context.textBaseline = "alphabetic";
  context.fillStyle = "#000";
  const glyphs = coveredCodes(buffer, ranges).map((code) => glyphOf(context, code, found.size));
  return { size: found.size, font: { ascent: found.ascent, descent: found.descent, glyphs } };
}

function drawSample(canvas, font) {
  const height = font.ascent + font.descent;
  const byChar = new Map(font.glyphs.map((glyph) => [glyph.char, glyph]));
  const width = Array.from(SAMPLE).reduce((sum, char) => sum + (byChar.get(char)?.advance ?? 0), 0);
  canvas.width = Math.max(1, width);
  canvas.height = height;
  const context = canvas.getContext("2d");
  const image = context.createImageData(canvas.width, canvas.height);
  let pen = 0;
  for (const char of SAMPLE) {
    const glyph = byChar.get(char);
    glyph?.rows.forEach((row, line) => {
      Array.from(row, (mark, column) => {
        const [x, y] = [pen + glyph.left + column, font.ascent - glyph.top + line];
        if (x >= 0 && x < canvas.width && y >= 0 && y < canvas.height) {
          image.data.set([236, 237, 241, (ICON_MARKS.indexOf(mark) * FULL) / SOLID], (y * canvas.width + x) * 4);
        }
      });
    });
    pen += glyph?.advance ?? 0;
  }
  context.putImageData(image, 0, 0);
}

function control(text, input) {
  const wrap = document.createElement("label");
  wrap.append(text, input);
  return wrap;
}

function button(text) {
  const made = document.createElement("button");
  made.type = "button";
  made.textContent = text;
  return made;
}

const kept = new Map();

function keptFor(entry, theme) {
  const saved = kept.get(entry.name);
  return saved && saved.made === JSON.stringify(theme.fonts[entry.name]) ? saved : null;
}

function fontPanel(entry, theme, schema, changed) {
  const details = document.createElement("details");
  const summary = document.createElement("summary");
  const controls = document.createElement("div");
  const sample = document.createElement("canvas");
  const picker = document.createElement("input");
  const size = document.createElement("input");
  const choose = button("Choose a font file…");
  const remove = button("Use the player's own");
  const note = document.createElement("p");
  const saved = keptFor(entry, theme);
  let source = saved?.source ?? null;

  const show = () => {
    const font = theme.fonts[entry.name];
    const named = source ? `${source.name}, ` : "";
    summary.textContent = font
      ? `${LABELS[entry.name]}: ${named}${font.glyphs.length} characters, ${font.ascent + font.descent} px tall`
      : `${LABELS[entry.name]}: the player's own`;
    sample.hidden = !font;
    remove.hidden = !font;
    if (font) {
      drawSample(sample, font);
    }
  };
  const put = (font) => {
    theme.fonts[entry.name] = font;
    if (source && font) {
      kept.set(entry.name, { source, size: size.value, made: JSON.stringify(font) });
    } else {
      kept.delete(entry.name);
    }
    show();
    changed();
  };
  const remake = async () => {
    if (!source) {
      return;
    }
    try {
      const made = await rasterise(source.buffer.slice(0), Number(size.value) || 0, entry.height, schema.font_ranges);
      size.placeholder = String(made.size);
      note.textContent = "";
      put(made.font);
    } catch (error) {
      note.textContent = error.message;
    }
  };

  Object.assign(picker, { type: "file", accept: ".ttf,.otf", hidden: true });
  Object.assign(size, { type: "number", min: LEAST_SIZE, max: MOST_SIZE, placeholder: "fit", value: saved?.size ?? "" });
  size.title = "Pixel size; leave empty for the largest that fits";
  sample.className = "font-sample";
  note.className = "hint";
  controls.className = "row";
  controls.append(choose, control("Size", size), remove, picker);

  choose.addEventListener("click", () => picker.click());
  picker.addEventListener("change", async () => {
    if (picker.files.length > 0) {
      source = { name: picker.files[0].name, buffer: await picker.files[0].arrayBuffer() };
      picker.value = "";
      await remake();
    }
  });
  size.addEventListener("change", remake);
  remove.addEventListener("click", () => {
    source = null;
    put(null);
  });

  details.append(summary, controls, note, sample);
  show();
  return details;
}

export function buildFonts(host, theme, schema, changed) {
  host.replaceChildren(...schema.fonts.map((entry) => fontPanel(entry, theme, schema, changed)));
}
