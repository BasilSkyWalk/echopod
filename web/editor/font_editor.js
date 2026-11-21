import { LETTERS, LETTER_STYLES, inked, letterFont, letterHeight, sheetFont, sheetLayout, sheetOrder, sheetTemplate } from "./pixel_letters.js";
import { ICON_MARKS } from "./theme_format.js";

const LABELS = { title: "Titles", body: "List text", small: "Small text" };
const USES = {
  title: "The title bar and big song names",
  body: "Lists, menus and most writing",
  small: "Second lines, times and the status bar",
};
const SAMPLE = "Afterglow Motel · 2021";
const LEAST_SIZE = 8;
const MOST_SIZE = 32;
const FULL = 255;
const SOLID = 15;
const HALF = 8;
const SIZE_STEPS = [0, 2, 4];
const CELL = 14;
const SHADES = [["Solid", 15], ["Strong", 11], ["Half", 8], ["Faint", 4]];
const SFNT = new Set(["\u0000\u0001\u0000\u0000", "OTTO", "true", "typ1"]);
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

function everyCode(ranges) {
  return ranges.flatMap(([first, last]) => Array.from({ length: last - first + 1 }, (_, index) => first + index));
}

function drawnWith(context, char, stack, side) {
  context.font = `${side / 3}px ${stack}`;
  context.clearRect(0, 0, side, side);
  context.fillText(char, side / 3, side / 1.5);
  return context.getImageData(0, 0, side, side).data;
}

function renderedCodes(family, ranges) {
  const side = 72;
  const canvas = document.createElement("canvas");
  canvas.width = canvas.height = side;
  const context = canvas.getContext("2d", { willReadFrequently: true });
  return everyCode(ranges).filter((code) => {
    const char = String.fromCharCode(code);
    if (char === " ") {
      return true;
    }
    const first = drawnWith(context, char, `"${family}", serif`, side);
    const second = drawnWith(context, char, `"${family}", monospace`, side);
    return first.some((value) => value !== 0) && first.every((value, index) => value === second[index]);
  });
}

function isSfnt(buffer) {
  return buffer.byteLength >= 4 && SFNT.has(tag(new DataView(buffer), 0));
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

function level(alpha, crisp) {
  const shade = Math.round((alpha * SOLID) / FULL);
  return crisp ? (shade >= HALF ? SOLID : 0) : shade;
}

function glyphOf(context, code, pen, crisp) {
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
  const grid = [];
  for (let y = box.top; y <= box.bottom; y++) {
    const row = [];
    for (let x = box.left; x <= box.right; x++) {
      row.push(level(alphas[(y * side + x) * 4 + 3], crisp));
    }
    grid.push(row);
  }
  const made = inked(grid);
  if (made.rows.length === 0) {
    return { char, left: 0, top: 0, advance, rows: [] };
  }
  return { char, left: box.left + made.left - pen, top: pen - box.top - made.top, advance, rows: made.rows };
}

async function loadedFamily(buffer) {
  const family = `EchoThemeFont${families++}`;
  const face = new FontFace(family, buffer);
  await face.load();
  document.fonts.add(face);
  return family;
}

export async function rasterise(buffer, wanted, limit, ranges, options = {}) {
  const family = options.family ?? await loadedFamily(buffer);
  const found = fittingSize(document.createElement("canvas").getContext("2d"), family, wanted, limit);
  const canvas = document.createElement("canvas");
  canvas.width = canvas.height = found.size * 3;
  const context = canvas.getContext("2d", { willReadFrequently: true });
  context.font = `${found.size}px "${family}"`;
  context.textBaseline = "alphabetic";
  context.fillStyle = "#000";
  const codes = options.codes ?? (isSfnt(buffer) ? coveredCodes(buffer, ranges) : renderedCodes(family, ranges));
  const glyphs = codes.map((code) => glyphOf(context, code, found.size, Boolean(options.crisp)));
  return { size: found.size, font: { ascent: found.ascent, descent: found.descent, glyphs } };
}

function h(tagName, attributes = {}, ...children) {
  const node = document.createElement(tagName);
  for (const [key, value] of Object.entries(attributes)) {
    if (key === "class") {
      node.className = value;
    } else if (key.startsWith("on")) {
      node.addEventListener(key.slice(2), value);
    } else if (value !== undefined && value !== null && value !== false) {
      node.setAttribute(key, value === true ? "" : value);
    }
  }
  node.append(...children.flat().filter((child) => child !== null && child !== undefined && child !== false));
  return node;
}

function chips(options, current, set) {
  return h("div", { class: "chips" }, options.map(([value, label]) =>
    h("button", { type: "button", "aria-pressed": String(value === current), onclick: () => set(value) }, label)));
}

function slider(label, value, least, most, set, note) {
  const input = h("input", { type: "range", min: least, max: most, value });
  const output = h("output", {}, String(value));
  input.addEventListener("input", () => {
    output.textContent = input.value;
    set(Number(input.value));
  });
  return h("label", { class: "field" }, h("span", {}, label, note ? h("small", {}, note) : null), h("span", { class: "range" }, input, output));
}

function paintText(canvas, font, text, scale = 1) {
  const height = font.ascent + font.descent;
  const byChar = new Map(font.glyphs.map((glyph) => [glyph.char, glyph]));
  const width = Array.from(text).reduce((sum, char) => sum + (byChar.get(char)?.advance ?? 0), 0);
  canvas.width = Math.max(1, width);
  canvas.height = Math.max(1, height);
  canvas.style.width = `${canvas.width * scale}px`;
  canvas.style.height = `${canvas.height * scale}px`;
  const context = canvas.getContext("2d");
  const image = context.createImageData(canvas.width, canvas.height);
  let pen = 0;
  for (const char of text) {
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
  return canvas;
}

function grown(glyph, font) {
  const height = font.ascent + font.descent;
  const reach = glyph.rows.length ? glyph.left + glyph.rows[0].length : 0;
  const width = Math.max(glyph.advance, reach, 1);
  const grid = Array.from({ length: height }, () => new Array(width).fill(0));
  glyph.rows.forEach((row, line) => {
    Array.from(row, (mark, column) => {
      const [x, y] = [glyph.left + column, font.ascent - glyph.top + line];
      if (x >= 0 && x < width && y >= 0 && y < height) {
        grid[y][x] = ICON_MARKS.indexOf(mark);
      }
    });
  });
  return grid;
}

function shrunk(grid, font, char, advance) {
  const made = inked(grid);
  return { char, left: made.left, top: font.ascent - made.top, advance, rows: made.rows };
}

function number(label, value, least, most, set, note) {
  const input = h("input", { type: "number", min: least, max: most, value, inputmode: "numeric" });
  input.addEventListener("change", () => {
    const wanted = Math.max(least, Math.min(most, Math.round(Number(input.value) || least)));
    input.value = wanted;
    set(wanted);
  });
  return h("label", { class: "field" }, h("span", {}, label, note ? h("small", {}, note) : null), input);
}

function downloadPicture(image, name) {
  const canvas = document.createElement("canvas");
  canvas.width = image.width;
  canvas.height = image.height;
  canvas.getContext("2d").putImageData(new ImageData(image.data, image.width, image.height), 0, 0);
  canvas.toBlob((blob) => {
    const link = h("a", { href: URL.createObjectURL(blob), download: name });
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(link.href), 1000);
  }, "image/png");
}

async function pixelsOf(file) {
  const bitmap = await createImageBitmap(file);
  const canvas = document.createElement("canvas");
  canvas.width = bitmap.width;
  canvas.height = bitmap.height;
  const context = canvas.getContext("2d", { willReadFrequently: true });
  context.drawImage(bitmap, 0, 0);
  return context.getImageData(0, 0, bitmap.width, bitmap.height);
}

function shifted(grid, across, down) {
  return grid.map((row, y) => row.map((_, x) => grid[y - down]?.[x - across] ?? 0));
}

function mirrored(grid, advance) {
  const reach = Math.min(grid[0].length, Math.max(advance, ...grid.map((row) => row.findLastIndex((value) => value > 0) + 1)));
  return grid.map((row) => row.map((value, x) => (x < reach ? row[reach - 1 - x] : value)));
}

function upsideDown(grid) {
  const inkedRows = grid.map((row, y) => (row.some((value) => value > 0) ? y : -1)).filter((y) => y >= 0);
  if (!inkedRows.length) {
    return grid;
  }
  const [top, bottom] = [inkedRows[0], inkedRows.at(-1)];
  return grid.map((row, y) => (y >= top && y <= bottom ? [...grid[top + bottom - y]] : row));
}

function letterChars(ranges) {
  return everyCode(ranges).map((code) => String.fromCharCode(code));
}

const buffers = new Map();
const pictures = new Map();
const opened = new Set();

export function buildFonts(host, theme, schema, changed) {
  theme.font_sources ??= {};
  theme.font_edits ??= {};
  const open = opened;
  const render = () => host.replaceChildren(...schema.fonts.map((entry) => slotCard(entry)));

  const finalFont = (entry, base) => {
    if (!base) {
      return null;
    }
    const edits = theme.font_edits[entry.name] ?? {};
    const byChar = new Map(base.glyphs.map((glyph) => [glyph.char, glyph]));
    for (const [char, glyph] of Object.entries(edits)) {
      byChar.set(char, glyph);
    }
    return { ascent: base.ascent, descent: base.descent, glyphs: [...byChar.values()] };
  };

  const settle = (entry, base, note) => {
    theme.fonts[entry.name] = finalFont(entry, base);
    changed();
    if (note) {
      note.textContent = "";
    }
  };

  const makeDrawn = (entry, note) => {
    const source = theme.font_sources[entry.name];
    try {
      settle(entry, letterFont(source.style, entry.height), note);
    } catch (error) {
      if (!(error instanceof RangeError)) {
        throw error;
      }
      if (note) {
        note.textContent = `${error.message}. Make the dots smaller or closer together.`;
      }
    }
  };

  const makeFromFile = async (entry, note) => {
    const source = theme.font_sources[entry.name];
    const held = buffers.get(entry.name);
    if (!held) {
      return;
    }
    try {
      const made = await rasterise(held.buffer.slice(0), source.size || 0, entry.height, schema.font_ranges,
        { crisp: source.crisp, family: held.family });
      source.size = made.size;
      settle(entry, made.font, note);
    } catch (error) {
      if (note) {
        note.textContent = error.message;
      }
    }
  };

  const makePicture = (entry, note) => {
    const source = theme.font_sources[entry.name];
    const held = pictures.get(entry.name);
    if (!held) {
      return;
    }
    try {
      settle(entry, sheetFont(held.image, { ...source, limit: entry.height }), note);
    } catch (error) {
      if (!(error instanceof RangeError)) {
        throw error;
      }
      if (note) {
        note.textContent = `${error.message[0].toUpperCase()}${error.message.slice(1)}. Check the letter boxes below.`;
      }
    }
  };

  function pictureArea(entry, note) {
    const source = theme.font_sources[entry.name];
    const held = pictures.get(entry.name);
    const picker = h("input", { type: "file", accept: ".png,.gif,.webp,.bmp", hidden: true });
    const take = async (file) => {
      if (!file) {
        return;
      }
      try {
        const image = await pixelsOf(file);
        pictures.set(entry.name, { image, name: file.name });
        source.name = file.name;
        makePicture(entry, note);
        render();
      } catch {
        note.textContent = `${file.name} couldn't be opened as a picture. Try a PNG.`;
      }
    };
    picker.addEventListener("change", () => take(picker.files[0]));
    const drop = h("div", { class: "drop-zone", tabindex: "0", role: "button",
      onclick: () => picker.click(),
      onkeydown: (event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          picker.click();
        }
      },
      ondragover: (event) => {
        event.preventDefault();
        drop.classList.add("over");
      },
      ondragleave: () => drop.classList.remove("over"),
      ondrop: (event) => {
        event.preventDefault();
        drop.classList.remove("over");
        take(event.dataTransfer.files[0]);
      },
    }, h("strong", {}, held?.name ?? source.name ?? "Drop your letters picture here"),
    h("span", {}, held ? "or click to choose another" : "or click to choose a PNG"), picker);
    const relayout = (change) => {
      change();
      if (pictures.get(entry.name)) {
        makePicture(entry, note);
        refreshSample(entry);
      }
    };
    const rows = Math.ceil(Array.from(source.order).length / 16);
    return [
      h("ol", { class: "steps-list" },
        h("li", {}, "Download the template: one box per letter, 16 to a row, in the order the player expects."),
        h("li", {}, "Draw your letters over the pink hints in any paint program, sitting on the dotted pink line. Pink is ignored, so there's no need to erase it."),
        h("li", {}, "Drop the picture back here. Dark on light, light on dark and a see-through background all work."),
      ),
      h("div", { class: "actions" }, h("button", { type: "button", onclick: () =>
        downloadPicture(sheetTemplate(source, source.order), `${entry.name}-letters-${source.cell[0]}x${source.cell[1]}.png`) }, "Download the template")),
      drop,
      h("details", { class: "fine-print" }, h("summary", {}, "Letter boxes"),
        h("p", { class: "hint" }, `Your own picture works too: ${rows} rows of 16 equal boxes, starting with a space, then ! " # and on through the keyboard's characters.`),
        h("div", { class: "group" },
          number("Box width", source.cell[0], 3, 40, (value) => relayout(() => { source.cell[0] = value; }), "pixels"),
          number("Box height", source.cell[1], 4, 48, (value) => relayout(() => { source.cell[1] = value; }), "pixels"),
          number("Baseline", source.baseline, 1, 48, (value) => relayout(() => { source.baseline = value; }), "rows down from the top of a box"),
        )),
      slider("Space between letters", source.gap, 0, 6, (value) => relayout(() => { source.gap = value; })),
    ];
  }

  function sizeChoices(entry, note, list) {
    const source = theme.font_sources[entry.name];
    const held = buffers.get(entry.name);
    if (!held) {
      return;
    }
    list.replaceChildren(h("span", { class: "hint" }, "Working out sizes…"));
    rasterise(held.buffer.slice(0), 0, entry.height, schema.font_ranges, { crisp: source.crisp, family: held.family }).then(async (largest) => {
      const sizes = SIZE_STEPS.map((step) => largest.size - step).filter((size) => size >= LEAST_SIZE);
      const options = [];
      for (const size of sizes) {
        const made = size === largest.size ? largest : await rasterise(held.buffer.slice(0), size, entry.height, schema.font_ranges,
          { crisp: source.crisp, family: held.family, codes: largest.font.glyphs.map((glyph) => glyph.char.charCodeAt(0)) });
        const picked = size === (source.size || largest.size);
        options.push(h("button", { type: "button", class: "size-choice", "aria-pressed": String(picked), onclick: () => {
          source.size = size;
          settle(entry, made.font, note);
          render();
        } }, paintText(h("canvas", { class: "font-sample" }), made.font, SAMPLE, 2), h("span", {}, `${size} px`)));
      }
      list.replaceChildren(...options);
    }).catch((error) => {
      list.replaceChildren();
      note.textContent = error.message;
    });
  }

  function fileArea(entry, note) {
    const source = theme.font_sources[entry.name];
    const picker = h("input", { type: "file", accept: ".ttf,.otf,.woff,.woff2", hidden: true });
    const take = async (file) => {
      if (!file) {
        return;
      }
      try {
        const buffer = await file.arrayBuffer();
        const family = await loadedFamily(buffer.slice(0));
        buffers.set(entry.name, { buffer, family, name: file.name });
        theme.font_sources[entry.name] = { kind: "file", name: file.name, size: 0, crisp: source?.crisp ?? false };
        await makeFromFile(entry, note);
        render();
      } catch {
        note.textContent = `${file.name} couldn't be read as a font. Try a .ttf or .otf file.`;
      }
    };
    picker.addEventListener("change", () => take(picker.files[0]));
    const drop = h("div", { class: "drop-zone", tabindex: "0", role: "button",
      onclick: () => picker.click(),
      onkeydown: (event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          picker.click();
        }
      },
      ondragover: (event) => {
        event.preventDefault();
        drop.classList.add("over");
      },
      ondragleave: () => drop.classList.remove("over"),
      ondrop: (event) => {
        event.preventDefault();
        drop.classList.remove("over");
        take(event.dataTransfer.files[0]);
      },
    }, h("strong", {}, buffers.get(entry.name)?.name ?? source?.name ?? "Drop a font file here"),
    h("span", {}, buffers.get(entry.name) ? "or click to choose another" : "or click to choose one: .ttf, .otf, .woff or .woff2"), picker);
    const parts = [drop];
    if (buffers.get(entry.name)) {
      const list = h("div", { class: "size-choices" });
      parts.push(
        h("p", { class: "hint" }, "Pick the size that reads best. Bigger isn't always sharper."),
        list,
        h("label", { class: "field" }, h("span", {}, "Crisp edges", h("small", {}, "No soft grey edges; good for pixel fonts")),
          (() => {
            const box = h("input", { type: "checkbox" });
            box.checked = Boolean(source.crisp);
            box.addEventListener("change", async () => {
              source.crisp = box.checked;
              await makeFromFile(entry, note);
              render();
            });
            return box;
          })()),
      );
      sizeChoices(entry, note, list);
    } else if (source?.name) {
      parts.push(h("p", { class: "hint" }, `This theme uses ${source.name}. Drop it here again to change its size.`));
    }
    return parts;
  }

  function drawnArea(entry, note) {
    const source = theme.font_sources[entry.name];
    const style = source.style;
    const restyle = (key, value) => {
      style[key] = value;
      makeDrawn(entry, note);
      refreshSample(entry);
    };
    const pair = (key, index) => (value) => {
      style[key][index] = value;
      if (key === "dot") {
        style.spacing[index] = Math.max(style.spacing[index], value);
      }
      makeDrawn(entry, note);
      refreshSample(entry);
    };
    const height = letterHeight(style);
    return [
      h("p", { class: "hint" }, "Start from a letter style, shape it with the sliders, then redraw any letter below."),
      chips(Object.entries(LETTER_STYLES).map(([key, value]) => [key, value.name]), source.base, (base) => {
        source.base = base;
        source.style = structuredClone(LETTER_STYLES[base]);
        makeDrawn(entry, note);
        render();
      }),
      h("div", { class: "group" },
        slider("Dot width", style.dot[0], 1, 4, pair("dot", 0)),
        slider("Dot height", style.dot[1], 1, 4, pair("dot", 1)),
        slider("Space across", style.spacing[0], 1, 5, (value) => {
          style.spacing[0] = Math.max(value, style.dot[0]);
          makeDrawn(entry, note);
          refreshSample(entry);
        }),
        slider("Space down", style.spacing[1], 1, 5, (value) => {
          style.spacing[1] = Math.max(value, style.dot[1]);
          makeDrawn(entry, note);
          refreshSample(entry);
        }, `Letters are ${height} px of ${entry.height}`),
        slider("Glow between dots", style.glow, 0, 12, (value) => restyle("glow", value)),
        slider("Bold", style.bold, 0, 2, (value) => restyle("bold", value)),
        slider("Space between letters", style.gap, 0, 6, (value) => restyle("gap", value)),
      ),
    ];
  }

  const samples = new Map();
  function refreshSample(entry) {
    const canvas = samples.get(entry.name);
    const font = theme.fonts[entry.name];
    if (canvas && font) {
      paintText(canvas, font, SAMPLE, 2);
    }
  }

  function letterGrid(entry) {
    const font = theme.fonts[entry.name];
    const editor = h("div", { class: "glyph-editor" });
    const byChar = new Map(font.glyphs.map((glyph) => [glyph.char, glyph]));
    const edits = theme.font_edits[entry.name] ??= {};
    const cells = letterChars(schema.font_ranges).map((char) => {
      const glyph = byChar.get(char);
      const thumb = glyph ? paintText(h("canvas"), { ...font, glyphs: [glyph] }, char, 2) : h("span", { class: "missing" }, char);
      return h("button", {
        type: "button",
        class: [glyph ? "" : "missing-letter", edits[char] ? "edited" : ""].join(" ").trim(),
        title: glyph ? `Redraw ${char}` : `${char} isn't in this font; draw it`,
        onclick: (event) => openLetter(entry, char, editor, event.currentTarget),
      }, thumb);
    });
    const missing = letterChars(schema.font_ranges).filter((char) => !byChar.has(char)).length;
    return [
      h("h4", {}, "Letters"),
      h("p", { class: "hint" }, missing ? `${missing} are missing (dashed); the player uses its own for those. Click any letter to draw it.` : "Click any letter to redraw it."),
      h("div", { class: "letter-grid" }, cells),
      editor,
    ];
  }

  function openLetter(entry, char, editor, cell) {
    const font = theme.fonts[entry.name];
    const glyph = font.glyphs.find((one) => one.char === char) ?? { char, left: 0, top: 0, advance: Math.max(4, Math.round((font.ascent + font.descent) / 2)), rows: [] };
    let grid = grown(glyph, font);
    let advance = glyph.advance;
    let shade = SOLID;
    let erasing = false;
    const canvas = h("canvas", { class: "glyph-canvas" });
    const eraser = h("button", { type: "button", "aria-pressed": "false", onclick: () => {
      erasing = !erasing;
      eraser.setAttribute("aria-pressed", String(erasing));
    } }, "Erase");
    const draw = () => {
      const width = grid[0].length;
      canvas.width = width * CELL + 1;
      canvas.height = grid.length * CELL + 1;
      const pen = canvas.getContext("2d");
      pen.fillStyle = "#16161a";
      pen.fillRect(0, 0, canvas.width, canvas.height);
      grid.forEach((row, y) => row.forEach((value, x) => {
        if (value > 0) {
          pen.fillStyle = `rgba(236, 237, 241, ${value / SOLID})`;
          pen.fillRect(x * CELL + 1, y * CELL + 1, CELL - 1, CELL - 1);
        }
      }));
      pen.strokeStyle = "rgba(255, 255, 255, 0.08)";
      for (let x = 0; x <= width; x++) {
        pen.beginPath();
        pen.moveTo(x * CELL + 0.5, 0);
        pen.lineTo(x * CELL + 0.5, canvas.height);
        pen.stroke();
      }
      for (let y = 0; y <= grid.length; y++) {
        pen.beginPath();
        pen.moveTo(0, y * CELL + 0.5);
        pen.lineTo(canvas.width, y * CELL + 0.5);
        pen.stroke();
      }
      pen.strokeStyle = "#e07a5f";
      pen.beginPath();
      pen.moveTo(0, font.ascent * CELL + 0.5);
      pen.lineTo(canvas.width, font.ascent * CELL + 0.5);
      pen.stroke();
      pen.strokeStyle = "#81b29a";
      pen.beginPath();
      pen.moveTo(advance * CELL + 0.5, 0);
      pen.lineTo(advance * CELL + 0.5, canvas.height);
      pen.stroke();
    };
    let pending = false;
    const save = () => {
      theme.font_edits[entry.name][char] = shrunk(grid, font, char, advance);
      if (pending) {
        return;
      }
      pending = true;
      setTimeout(() => {
        pending = false;
        const base = theme.fonts[entry.name];
        const glyphs = base.glyphs.filter((one) => one.char !== char);
        glyphs.push(theme.font_edits[entry.name][char]);
        theme.fonts[entry.name] = { ...base, glyphs };
        changed();
        refreshSample(entry);
        if (cell) {
          const drawnGlyph = theme.font_edits[entry.name][char];
          cell.className = "edited";
          cell.replaceChildren(paintText(h("canvas"), { ...theme.fonts[entry.name], glyphs: [drawnGlyph] }, char, 2));
        }
      }, 30);
    };
    let painting = null;
    const paintAt = (event) => {
      const box = canvas.getBoundingClientRect();
      const x = Math.floor(((event.clientX - box.left) * (canvas.width / box.width)) / CELL);
      const y = Math.floor(((event.clientY - box.top) * (canvas.height / box.height)) / CELL);
      if (y >= 0 && y < grid.length && x >= 0 && x < grid[0].length) {
        grid[y][x] = painting === "erase" ? 0 : shade;
        draw();
        save();
      }
    };
    canvas.addEventListener("contextmenu", (event) => event.preventDefault());
    canvas.addEventListener("pointerdown", (event) => {
      canvas.setPointerCapture(event.pointerId);
      painting = erasing || event.button === 2 ? "erase" : "paint";
      paintAt(event);
    });
    canvas.addEventListener("pointermove", (event) => {
      if (painting) {
        paintAt(event);
      }
    });
    const stop = () => {
      painting = null;
    };
    canvas.addEventListener("pointerup", stop);
    canvas.addEventListener("pointercancel", stop);
    const widen = (step) => {
      advance = Math.max(1, Math.min(60, advance + step));
      const width = Math.max(advance, ...grid.map((row) => row.findLastIndex((value) => value > 0) + 1), 1);
      grid = grid.map((row) => row.slice(0, width).concat(new Array(Math.max(0, width - row.length)).fill(0)));
      draw();
      save();
    };
    const reshape = (change) => {
      grid = change(grid);
      draw();
      save();
    };
    const others = font.glyphs.filter((one) => one.char !== char && one.rows.length);
    const copyFrom = h("select", { "aria-label": "Start from another letter" },
      new Option("Copy another letter…", ""), others.map((one) => new Option(one.char, one.char)));
    copyFrom.addEventListener("change", () => {
      const other = theme.fonts[entry.name].glyphs.find((one) => one.char === copyFrom.value);
      if (other) {
        const copied = grown(other, font);
        const width = Math.max(copied[0].length, grid[0].length);
        grid = copied.map((row) => row.concat(new Array(width - row.length).fill(0)));
        advance = other.advance;
        draw();
        save();
      }
      copyFrom.value = "";
    });
    editor.replaceChildren(
      h("div", { class: "glyph-head" }, h("strong", {}, `Drawing “${char}”`),
        h("button", { type: "button", onclick: () => editor.replaceChildren() }, "Done")),
      h("p", { class: "hint" }, "Click or drag to paint, right-click or Erase to clear. The red line is where letters sit; the green line is where the next letter starts."),
      canvas,
      h("div", { class: "glyph-tools" },
        chips(SHADES.map(([label, value]) => [value, label]), shade, (value) => {
          shade = value;
          erasing = false;
          editor.querySelectorAll(".glyph-tools .chips button").forEach((button, index) =>
            button.setAttribute("aria-pressed", String(SHADES[index][1] === value)));
          eraser.setAttribute("aria-pressed", "false");
        }),
        eraser,
        h("button", { type: "button", onclick: () => widen(-1) }, "Narrower"),
        h("button", { type: "button", onclick: () => widen(1) }, "Wider"),
        h("button", { type: "button", onclick: () => {
          grid = grid.map((row) => row.map(() => 0));
          draw();
          save();
        } }, "Clear"),
      ),
      h("div", { class: "glyph-tools" },
        copyFrom,
        h("button", { type: "button", title: "Flip left to right", onclick: () => reshape((rows) => mirrored(rows, advance)) }, "Mirror"),
        h("button", { type: "button", title: "Flip top to bottom", onclick: () => reshape(upsideDown) }, "Upside down"),
        h("span", { class: "nudge", role: "group", "aria-label": "Move the letter" },
          h("button", { type: "button", title: "Move left", "aria-label": "Move left", onclick: () => reshape((rows) => shifted(rows, -1, 0)) }, "←"),
          h("button", { type: "button", title: "Move up", "aria-label": "Move up", onclick: () => reshape((rows) => shifted(rows, 0, -1)) }, "↑"),
          h("button", { type: "button", title: "Move down", "aria-label": "Move down", onclick: () => reshape((rows) => shifted(rows, 0, 1)) }, "↓"),
          h("button", { type: "button", title: "Move right", "aria-label": "Move right", onclick: () => reshape((rows) => shifted(rows, 1, 0)) }, "→")),
        theme.font_edits[entry.name]?.[char] ? h("button", { type: "button", onclick: () => {
          delete theme.font_edits[entry.name][char];
          rebuildBase(entry);
          editor.replaceChildren();
        } }, "Undo my changes to this letter") : null,
      ),
    );
    draw();
    editor.scrollIntoView({ block: "nearest" });
  }

  const rebuildBase = (entry) => {
    const source = theme.font_sources[entry.name];
    if (source?.kind === "drawn") {
      makeDrawn(entry);
    } else if (source?.kind === "picture" && pictures.get(entry.name)) {
      makePicture(entry);
    } else if (source?.kind === "file" && buffers.get(entry.name)) {
      makeFromFile(entry).then(render);
      return;
    } else if (theme.fonts[entry.name]) {
      const glyphs = theme.fonts[entry.name].glyphs.filter((glyph) => !(glyph.char in (theme.font_edits[entry.name] ?? {})));
      theme.fonts[entry.name] = { ...theme.fonts[entry.name], glyphs };
      changed();
    }
    render();
  };

  function slotCard(entry) {
    const font = theme.fonts[entry.name];
    const source = theme.font_sources[entry.name];
    const kind = source?.kind ?? (font ? "kept" : "own");
    const note = h("p", { class: "notice-line" });
    const sample = h("canvas", { class: "font-sample" });
    samples.set(entry.name, sample);
    if (font) {
      paintText(sample, font, SAMPLE, 2);
    }
    const modes = [["own", "The player's own"], ["file", "From a font file"], ["picture", "From a picture"], ["drawn", "Draw my own"]];
    if (kind === "kept") {
      modes.splice(1, 0, ["kept", "This theme's"]);
    }
    const body = [];
    if (kind === "file") {
      body.push(...fileArea(entry, note));
    } else if (kind === "drawn") {
      body.push(...drawnArea(entry, note));
    } else if (kind === "picture") {
      body.push(...pictureArea(entry, note));
    }
    if (font) {
      body.push(...letterGrid(entry));
    }
    const card = h("details", { class: "font-card" },
      h("summary", {}, h("span", { class: "font-name" }, LABELS[entry.name]), h("small", {}, USES[entry.name]),
        font ? sample : h("span", { class: "hint" }, "The player's own")),
      chips(modes, kind, (mode) => {
        if (mode === "own") {
          theme.fonts[entry.name] = null;
          delete theme.font_sources[entry.name];
          changed();
        } else if (mode === "drawn") {
          theme.font_sources[entry.name] = { kind: "drawn", base: "dots", style: structuredClone(LETTER_STYLES.dots) };
          if (letterHeight(LETTER_STYLES.dots) > entry.height) {
            theme.font_sources[entry.name] = { kind: "drawn", base: "fine", style: structuredClone(LETTER_STYLES.fine) };
          }
          makeDrawn(entry, note);
        } else if (mode === "picture") {
          theme.font_sources[entry.name] = { kind: "picture", name: null, ...sheetLayout(entry.height), gap: 1, order: sheetOrder(schema.font_ranges) };
          if (pictures.get(entry.name)) {
            makePicture(entry, note);
          }
        } else if (mode === "file") {
          theme.font_sources[entry.name] = { kind: "file", name: null, size: 0, crisp: false };
          if (buffers.get(entry.name)) {
            makeFromFile(entry, note).then(render);
            return;
          }
        }
        open.add(entry.name);
        render();
      }),
      note,
      ...body,
    );
    card.open = open.has(entry.name);
    card.addEventListener("toggle", () => {
      if (card.open) {
        open.add(entry.name);
      } else {
        open.delete(entry.name);
      }
    });
    return card;
  }

  render();
}
