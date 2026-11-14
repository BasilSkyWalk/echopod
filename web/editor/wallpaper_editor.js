import { HEX_DIGITS, PALETTE_COLORS } from "./theme_format.js";

const LABELS = { wallpaper: "Wallpaper", wallpaper_portrait: "Wallpaper, upright screen" };
const DEFAULT_DIM = 40;
const MOST_DIM = 90;
const SAMPLE_STEP = 3;

function snapped(value, bits) {
  const kept = Math.max(0, Math.min(255, Math.round(value))) >> (8 - bits);
  return (kept << (8 - bits)) | (kept >> (2 * bits - 8));
}

function hex(color) {
  return "#" + color.map((part) => part.toString(16).padStart(2, "0")).join("");
}

function widest(box) {
  let best = { channel: 0, range: -1 };
  for (let channel = 0; channel < 3; channel++) {
    const values = box.map((color) => color[channel]);
    const range = Math.max(...values) - Math.min(...values);
    best = range > best.range ? { channel, range } : best;
  }
  return best;
}

function split(boxes) {
  const ranked = boxes.map((box, index) => ({ index, ...widest(box) }));
  const pick = ranked.reduce((best, next) => (next.range > best.range ? next : best));
  if (pick.range <= 0) {
    return false;
  }
  const sorted = boxes[pick.index].toSorted((first, second) => first[pick.channel] - second[pick.channel]);
  const middle = sorted.length >> 1;
  boxes.splice(pick.index, 1, sorted.slice(0, middle), sorted.slice(middle));
  return true;
}

function paletteOf(colors) {
  const boxes = [colors];
  while (boxes.length < PALETTE_COLORS && split(boxes)) {
    continue;
  }
  const shades = boxes.map((box) => {
    const mean = [0, 1, 2].map((channel) => box.reduce((sum, color) => sum + color[channel], 0) / box.length);
    return [snapped(mean[0], 5), snapped(mean[1], 6), snapped(mean[2], 5)];
  });
  return [...new Map(shades.map((shade) => [hex(shade), shade])).values()];
}

function nearest(palette, color) {
  let best = 0;
  let least = Infinity;
  palette.forEach((shade, index) => {
    const far = (shade[0] - color[0]) ** 2 + (shade[1] - color[1]) ** 2 + (shade[2] - color[2]) ** 2;
    if (far < least) {
      best = index;
      least = far;
    }
  });
  return best;
}

function spread(colors, size, at, error) {
  const shares = [[1, 0, 7], [-1, 1, 3], [0, 1, 5], [1, 1, 1]];
  for (const [across, down, share] of shares) {
    const [column, row] = [at.column + across, at.row + down];
    if (column >= 0 && column < size.width && row < size.height) {
      const target = colors[row * size.width + column];
      error.forEach((part, channel) => {
        target[channel] += (part * share) / 16;
      });
    }
  }
}

export function quantise(pixels, width, height) {
  const colors = [];
  for (let at = 0; at < width * height * 4; at += 4) {
    colors.push([pixels[at], pixels[at + 1], pixels[at + 2]]);
  }
  const palette = paletteOf(colors.filter((_, index) => index % SAMPLE_STEP === 0 || colors.length < 64));
  const rows = [];
  for (let row = 0; row < height; row++) {
    let marks = "";
    for (let column = 0; column < width; column++) {
      const color = colors[row * width + column];
      const pick = nearest(palette, color);
      spread(colors, { width, height }, { column, row }, color.map((part, channel) => part - palette[pick][channel]));
      marks += HEX_DIGITS[pick];
    }
    rows.push(marks);
  }
  return { palette: palette.map(hex), rows };
}

export function dimmed(pixels, background, percent) {
  const paper = [1, 3, 5].map((at) => parseInt(background.slice(at, at + 2), 16));
  return pixels.map((value, index) =>
    index % 4 === 3 ? 255 : Math.round(value + ((paper[index % 4] - value) * percent) / 100),
  );
}

export function fittedSize(picture, entry, tile) {
  const fits = picture.width <= entry.width && picture.height <= entry.height;
  if (!tile || !fits) {
    return { width: entry.width, height: entry.height };
  }
  return { width: Math.max(2, picture.width - (picture.width % 2)), height: picture.height };
}

function drawnPixels(picture, size) {
  const canvas = document.createElement("canvas");
  Object.assign(canvas, size);
  const context = canvas.getContext("2d", { willReadFrequently: true });
  const scale = Math.max(size.width / picture.width, size.height / picture.height);
  const [across, down] = [picture.width * scale, picture.height * scale];
  context.imageSmoothingQuality = "high";
  context.drawImage(picture, (size.width - across) / 2, (size.height - down) / 2, across, down);
  return context.getImageData(0, 0, size.width, size.height).data;
}

function drawPaper(canvas, paper) {
  canvas.width = paper.rows[0].length;
  canvas.height = paper.rows.length;
  const context = canvas.getContext("2d");
  const image = context.createImageData(canvas.width, canvas.height);
  paper.rows.forEach((row, line) => {
    Array.from(row, (mark, column) => {
      const color = paper.palette[HEX_DIGITS.indexOf(mark)];
      const at = (line * canvas.width + column) * 4;
      image.data.set([1, 3, 5].map((part) => parseInt(color.slice(part, part + 2), 16)), at);
      image.data[at + 3] = 255;
    });
  });
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

function wallpaperPanel(entry, theme, changed) {
  const details = document.createElement("details");
  const summary = document.createElement("summary");
  const controls = document.createElement("div");
  const shown = document.createElement("canvas");
  const picker = document.createElement("input");
  const dim = document.createElement("input");
  const tile = document.createElement("input");
  const choose = button("Choose an image…");
  const remove = button("Remove");
  let picture = null;

  const show = () => {
    const paper = theme.wallpapers[entry.name];
    summary.textContent = paper
      ? `${LABELS[entry.name]}: ${paper.rows[0].length} × ${paper.rows.length}, ${paper.palette.length} colours`
      : `${LABELS[entry.name]}: none`;
    shown.hidden = !paper;
    remove.hidden = !paper;
    if (paper) {
      drawPaper(shown, paper);
    }
  };
  const put = (paper) => {
    theme.wallpapers[entry.name] = paper;
    show();
    changed();
  };
  const remake = () => {
    if (picture) {
      const size = fittedSize(picture, entry, tile.checked);
      const pixels = dimmed(drawnPixels(picture, size), theme.colors.background, Number(dim.value));
      put(quantise(pixels, size.width, size.height));
    }
  };

  Object.assign(picker, { type: "file", accept: "image/*", hidden: true });
  Object.assign(dim, { type: "range", min: 0, max: MOST_DIM, value: DEFAULT_DIM });
  dim.title = "How far the picture fades into the background colour";
  tile.type = "checkbox";
  shown.className = "wallpaper-shown";
  controls.className = "row";
  controls.append(choose, control("Fade", dim), control("Repeat a small picture", tile), remove, picker);

  choose.addEventListener("click", () => picker.click());
  picker.addEventListener("change", async () => {
    if (picker.files.length > 0) {
      picture = await createImageBitmap(picker.files[0]);
      picker.value = "";
      remake();
    }
  });
  dim.addEventListener("change", remake);
  tile.addEventListener("change", remake);
  remove.addEventListener("click", () => {
    picture = null;
    put(null);
  });

  details.append(summary, controls, shown);
  show();
  return details;
}

export function buildWallpapers(host, theme, schema, changed) {
  host.replaceChildren(...schema.wallpapers.map((entry) => wallpaperPanel(entry, theme, changed)));
}
