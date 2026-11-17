import { contrast } from "./theme_format.js";

const WHITE = "#ffffff";
const BLACK = "#000000";
const STEPS = 50;
const SAMPLES = 6000;
const CLEAR = 128;
const DANGER = "#e5484d";
const SUCCESS = "#30a46c";

export function rgbOf(color) {
  const value = parseInt(color.slice(1), 16);
  return [value >> 16, (value >> 8) & 255, value & 255];
}

export function hexOf(rgb) {
  return `#${rgb.map((part) => Math.max(0, Math.min(255, Math.round(part))).toString(16).padStart(2, "0")).join("")}`;
}

export function mix(first, second, share) {
  const [a, b] = [rgbOf(first), rgbOf(second)];
  return hexOf(a.map((part, index) => part + (b[index] - part) * share));
}

function lightness(color) {
  const [red, green, blue] = rgbOf(color).map((part) => part / 255);
  return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

function saturation(color) {
  const parts = rgbOf(color).map((part) => part / 255);
  const [most, least] = [Math.max(...parts), Math.min(...parts)];
  const middle = (most + least) / 2;
  return most === least ? 0 : (most - least) / (1 - Math.abs(2 * middle - 1));
}

export function readableShade(color, against, floor) {
  const backs = Array.isArray(against) ? against : [against];
  const worst = (shade) => Math.min(...backs.map((back) => contrast(shade, back)));
  if (worst(color) >= floor) {
    return color;
  }
  let best = color;
  for (let step = 1; step <= STEPS; step++) {
    for (const end of [WHITE, BLACK]) {
      const shade = mix(color, end, step / STEPS);
      if (worst(shade) >= floor) {
        return shade;
      }
      if (worst(shade) > worst(best)) {
        best = shade;
      }
    }
  }
  return best;
}

function split(box) {
  const ranges = [0, 1, 2].map((channel) => {
    const values = box.map((pixel) => pixel[channel]);
    return Math.max(...values) - Math.min(...values);
  });
  const channel = ranges.indexOf(Math.max(...ranges));
  const sorted = [...box].sort((a, b) => a[channel] - b[channel]);
  const half = Math.floor(sorted.length / 2);
  return [sorted.slice(0, half), sorted.slice(half)];
}

function spread(box) {
  return Math.max(...[0, 1, 2].map((channel) => {
    const values = box.map((pixel) => pixel[channel]);
    return Math.max(...values) - Math.min(...values);
  })) * box.length;
}

export function paletteOf(pixels, count = 6) {
  const every = Math.max(1, Math.floor(pixels.length / 4 / SAMPLES));
  const picked = [];
  for (let at = 0; at < pixels.length; at += 4 * every) {
    if (pixels[at + 3] >= CLEAR) {
      picked.push([pixels[at], pixels[at + 1], pixels[at + 2]]);
    }
  }
  if (!picked.length) {
    return [];
  }
  let boxes = [picked];
  while (boxes.length < count) {
    const widest = boxes.reduce((best, box) => (box.length > 1 && spread(box) > spread(best) ? box : best), boxes[0]);
    if (widest.length < 2) {
      break;
    }
    boxes = boxes.filter((box) => box !== widest).concat(split(widest));
  }
  const merged = new Map();
  for (const box of boxes) {
    const color = hexOf([0, 1, 2].map((channel) => box.reduce((sum, pixel) => sum + pixel[channel], 0) / box.length));
    merged.set(color, (merged.get(color) ?? 0) + box.length / picked.length);
  }
  return [...merged].map(([color, share]) => ({ color, share })).sort((a, b) => b.share - a.share);
}

export function schemeFrom(palette, dark = true) {
  const colours = palette.map((entry) => entry.color);
  const common = palette.filter((entry) => entry.share >= 0.05).map((entry) => entry.color);
  const pool = common.length ? common : colours;
  const base = pool.reduce((best, color) => (dark ? lightness(color) < lightness(best) : lightness(color) > lightness(best)) ? color : best, pool[0]);
  const background = dark ? mix(base, BLACK, Math.max(0, lightness(base) - 0.04) * 1.5) : mix(base, WHITE, Math.max(0, 0.92 - lightness(base)) * 1.5);
  const lively = colours.filter((color) => color !== base && lightness(color) > 0.08 && lightness(color) < 0.9);
  const accent = (lively.length ? lively : colours).reduce((best, color) => (saturation(color) > saturation(best) ? color : best));
  const text = readableShade(mix(background, dark ? WHITE : BLACK, 0.9), background, 7);
  const selectionTop = mix(accent, WHITE, 0.15);
  const ink = contrast(WHITE, accent) >= contrast(BLACK, accent) ? WHITE : BLACK;
  const headerBackground = mix(background, text, 0.08);
  const headerTop = mix(background, text, 0.16);
  return {
    background,
    text,
    text_secondary: readableShade(mix(text, background, 0.3), background, 3.5),
    text_tertiary: readableShade(mix(text, background, 0.5), background, 2.6),
    divider: mix(background, text, 0.18),
    chevron: readableShade(mix(text, background, 0.55), background, 2.2),
    scrollbar_thumb: mix(background, text, 0.3),
    selection_background: accent,
    selection_top: selectionTop,
    selection_shine: mix(accent, WHITE, 0.35),
    selection_text: readableShade(ink, [accent, selectionTop], 4.5),
    header_background: headerBackground,
    header_top: headerTop,
    header_text: readableShade(text, [headerBackground, headerTop], 4.5),
    danger: readableShade(DANGER, background, 3.2),
    success: readableShade(SUCCESS, background, 3.2),
  };
}
