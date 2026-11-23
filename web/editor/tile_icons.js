import { ICON_MARKS } from "./theme_format.js";

const UNIT = 24;
const SAMPLES = 4;
const SOLID = 15;

const SHAPES = {
  home_music: [
    ["circle", 7, 18.5, 3.5],
    ["circle", 17, 16.5, 3.5],
    ["rect", 8.5, 5, 10.5, 18.5],
    ["rect", 18.5, 3, 20.5, 16.5],
    ["poly", 8.5, 3.5, 20.5, 1.5, 20.5, 6, 8.5, 8],
  ],
  home_audiobooks: [
    ["poly", 2, 5, 11, 7, 11, 21, 2, 19],
    ["poly", 13, 7, 22, 5, 22, 19, 13, 21],
  ],
  home_podcasts: [
    ["circle", 12, 11, 7.5],
    ["cut", "circle", 12, 11, 5.7],
    ["cut", "rect", 0, 0, 24, 11],
    ["rect", 9, 2, 15, 14.5, 3],
    ["rect", 11, 18, 13, 21],
    ["rect", 7.5, 20.5, 16.5, 22.5, 1],
  ],
  home_settings: [
    ["bar", 12, 1.5, 12, 22.5, 4.4],
    ["bar", 1.5, 12, 22.5, 12, 4.4],
    ["bar", 4.58, 4.58, 19.42, 19.42, 4.4],
    ["bar", 19.42, 4.58, 4.58, 19.42, 4.4],
    ["circle", 12, 12, 7.6],
    ["cut", "circle", 12, 12, 3.3],
  ],
  home_diagnostics: [
    ["line", 2, 13, 7.5, 13, 2.4],
    ["line", 7.5, 13, 10.5, 5.5, 2.4],
    ["line", 10.5, 5.5, 14.5, 19.5, 2.4],
    ["line", 14.5, 19.5, 17, 13, 2.4],
    ["line", 17, 13, 22, 13, 2.4],
  ],
  home_now_playing: [
    ["circle", 12, 12, 10.5],
    ["cut", "circle", 12, 12, 8.4],
    ["poly", 9.3, 7, 17.5, 12, 9.3, 17],
  ],
};

export const TILE_ICONS = Object.keys(SHAPES);

function inPolygon(points, x, y) {
  let inside = false;
  for (let at = 0, before = points.length - 2; at < points.length; before = at, at += 2) {
    const [ax, ay, bx, by] = [points[at], points[at + 1], points[before], points[before + 1]];
    if ((ay > y) !== (by > y) && x < ((bx - ax) * (y - ay)) / (by - ay) + ax) {
      inside = !inside;
    }
  }
  return inside;
}

function within([kind, ...n], x, y) {
  switch (kind) {
    case "circle":
      return (x - n[0]) ** 2 + (y - n[1]) ** 2 <= n[2] ** 2;
    case "rect": {
      const radius = n[4] ?? 0;
      const across = Math.max(n[0] + radius - x, 0, x - n[2] + radius);
      const down = Math.max(n[1] + radius - y, 0, y - n[3] + radius);
      return x >= n[0] && x <= n[2] && y >= n[1] && y <= n[3] && across ** 2 + down ** 2 <= radius ** 2;
    }
    case "line":
    case "bar": {
      const [ax, ay] = [n[2] - n[0], n[3] - n[1]];
      const along = ((x - n[0]) * ax + (y - n[1]) * ay) / (ax * ax + ay * ay);
      if (kind === "bar" && (along < 0 || along > 1)) {
        return false;
      }
      const near = Math.max(0, Math.min(1, along));
      return (x - n[0] - near * ax) ** 2 + (y - n[1] - near * ay) ** 2 <= (n[4] / 2) ** 2;
    }
    default:
      return inPolygon(n, x, y);
  }
}

function inked(shapes, x, y) {
  let on = false;
  for (const shape of shapes) {
    const cut = shape[0] === "cut";
    if (within(cut ? shape.slice(1) : shape, x, y)) {
      on = !cut;
    }
  }
  return on;
}

export function starterRows(name, width = UNIT, height = UNIT) {
  const shapes = SHAPES[name];
  if (!shapes) {
    return null;
  }
  const scale = Math.min(width, height) / UNIT;
  const [left, top] = [(width - UNIT * scale) / 2, (height - UNIT * scale) / 2];
  return Array.from({ length: height }, (_, row) => {
    let marks = "";
    for (let column = 0; column < width; column++) {
      let hits = 0;
      for (let sample = 0; sample < SAMPLES * SAMPLES; sample++) {
        const x = (column + ((sample % SAMPLES) + 0.5) / SAMPLES - left) / scale;
        const y = (row + (Math.floor(sample / SAMPLES) + 0.5) / SAMPLES - top) / scale;
        hits += inked(shapes, x, y) ? 1 : 0;
      }
      marks += ICON_MARKS[Math.round((hits * SOLID) / (SAMPLES * SAMPLES))];
    }
    return marks;
  });
}

export function addTileIcons(theme, schema, size = UNIT) {
  const entries = schema.icons.filter((entry) => SHAPES[entry.name]);
  if (entries.some((entry) => theme.icons?.[entry.name])) {
    return false;
  }
  theme.icons ??= {};
  for (const entry of entries) {
    theme.icons[entry.name] = starterRows(entry.name, Math.min(size, entry.width), Math.min(size, entry.height));
  }
  return entries.length > 0;
}
