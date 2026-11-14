import { ICON_MARKS } from "./theme_format.js";

const CELL = 12;
const SOLID = 15;
const INK = [236, 237, 241];
const PAPER = ["#101114", "#1a1c21"];

function label(name) {
  const words = name.replaceAll("_", " ");
  return words[0].toUpperCase() + words.slice(1);
}

export function blankRows(width, height) {
  return Array.from({ length: height }, () => ".".repeat(width));
}

export function resizedRows(rows, width, height) {
  return blankRows(width, height).map((blank, row) => ((rows[row] ?? "") + blank).slice(0, width));
}

export function paintedRows(rows, column, row, level) {
  return rows.map((marks, index) =>
    index === row ? marks.slice(0, column) + ICON_MARKS[level] + marks.slice(column + 1) : marks,
  );
}

export function rowsFromPixels(pixels, width, height) {
  const opaque = pixels.every((value, index) => index % 4 !== 3 || value === 255);
  return Array.from({ length: height }, (_, row) => {
    let marks = "";
    for (let column = 0; column < width; column++) {
      const at = (row * width + column) * 4;
      const light = (pixels[at] * 299 + pixels[at + 1] * 587 + pixels[at + 2] * 114) / 1000;
      const cover = opaque ? 255 - light : pixels[at + 3];
      marks += ICON_MARKS[Math.round((cover * SOLID) / 255)];
    }
    return marks;
  });
}

function drawGrid(canvas, rows) {
  const context = canvas.getContext("2d");
  canvas.width = rows[0].length * CELL;
  canvas.height = rows.length * CELL;
  rows.forEach((marks, row) => {
    Array.from(marks, (mark, column) => {
      context.fillStyle = PAPER[(row + column) % 2];
      context.fillRect(column * CELL, row * CELL, CELL, CELL);
      context.fillStyle = `rgba(${INK.join(",")},${ICON_MARKS.indexOf(mark) / SOLID})`;
      context.fillRect(column * CELL, row * CELL, CELL, CELL);
    });
  });
}

async function importedRows(file, width, height) {
  const picture = await createImageBitmap(file);
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d", { willReadFrequently: true });
  const scale = Math.min(width / picture.width, height / picture.height);
  const [across, down] = [picture.width * scale, picture.height * scale];
  context.imageSmoothingQuality = "high";
  context.drawImage(picture, (width - across) / 2, (height - down) / 2, across, down);
  return rowsFromPixels(context.getImageData(0, 0, width, height).data, width, height);
}

function numberField(text, most) {
  const wrap = document.createElement("label");
  const input = document.createElement("input");
  Object.assign(input, { type: "number", min: 1, max: most });
  wrap.append(text, input);
  return { wrap, input };
}

function button(text) {
  const made = document.createElement("button");
  made.type = "button";
  made.textContent = text;
  return made;
}

function iconPanel(entry, theme, changed) {
  const details = document.createElement("details");
  const summary = document.createElement("summary");
  const grid = document.createElement("canvas");
  const controls = document.createElement("div");
  const wide = numberField("Width", entry.width);
  const high = numberField("Height", entry.height);
  const level = document.createElement("input");
  const picker = document.createElement("input");
  const draw = button("Draw one");
  const load = button("From an image…");
  const clear = button("Use the player's own");
  let painting = false;

  const rows = () => theme.icons[entry.name];
  const show = () => {
    const own = rows() === null;
    summary.textContent = own
      ? `${label(entry.name)}: the player's own`
      : `${label(entry.name)}: ${rows()[0].length} × ${rows().length}`;
    grid.hidden = own;
    clear.hidden = own;
    draw.hidden = !own;
    wide.input.value = own ? entry.width : rows()[0].length;
    high.input.value = own ? entry.height : rows().length;
    if (!own) {
      drawGrid(grid, rows());
    }
  };
  const put = (next) => {
    theme.icons[entry.name] = next;
    show();
    changed();
  };
  const sized = () => {
    const clamp = (input, most) => Math.max(1, Math.min(most, Math.round(Number(input.value) || 1)));
    return [clamp(wide.input, entry.width), clamp(high.input, entry.height)];
  };
  const paint = (event) => {
    if (rows() === null) {
      return;
    }
    const box = grid.getBoundingClientRect();
    const column = Math.floor(((event.clientX - box.left) / box.width) * rows()[0].length);
    const row = Math.floor(((event.clientY - box.top) / box.height) * rows().length);
    if (column >= 0 && row >= 0 && column < rows()[0].length && row < rows().length) {
      const erase = event.buttons === 2 || event.shiftKey;
      put(paintedRows(rows(), column, row, erase ? 0 : Number(level.value)));
    }
  };

  Object.assign(level, { type: "range", min: 0, max: SOLID, value: SOLID, title: "How solid the brush is" });
  Object.assign(picker, { type: "file", accept: "image/*", hidden: true });
  grid.className = "icon-grid";
  controls.className = "row";
  controls.append(wide.wrap, high.wrap, "Brush", level, draw, load, clear, picker);

  draw.addEventListener("click", () => put(blankRows(...sized())));
  clear.addEventListener("click", () => put(null));
  load.addEventListener("click", () => picker.click());
  picker.addEventListener("change", async () => {
    if (picker.files.length > 0) {
      put(await importedRows(picker.files[0], ...sized()));
      picker.value = "";
    }
  });
  for (const input of [wide.input, high.input]) {
    input.addEventListener("change", () => {
      if (rows() !== null) {
        put(resizedRows(rows(), ...sized()));
      }
    });
  }
  grid.addEventListener("contextmenu", (event) => event.preventDefault());
  grid.addEventListener("pointerdown", (event) => {
    painting = true;
    grid.setPointerCapture(event.pointerId);
    paint(event);
  });
  grid.addEventListener("pointermove", (event) => painting && paint(event));
  grid.addEventListener("pointerup", () => {
    painting = false;
  });

  details.append(summary, controls, grid);
  show();
  return details;
}

export function buildIcons(host, theme, schema, changed) {
  host.replaceChildren(...schema.icons.map((entry) => iconPanel(entry, theme, changed)));
}
