export const AREAS = {
  wide: { top: 24, width: 320, height: 146 },
  tall: { top: 40, width: 170, height: 280 },
};

export const LAYER_TYPES = {
  cover: "Album cover",
  text: "Text",
  times: "Times",
  progress: "Progress",
  shape: "Shape",
  playmark: "Play and pause mark",
  spinner: "Turning mark",
};

export const HOME_LAYER_TYPES = {
  menu: "Menu",
  cover: "Album cover",
  text: "Text",
  progress: "Progress",
  times: "Times",
  shape: "Shape",
  playmark: "Play and pause mark",
};

export const TEXT_SOURCES = {
  song: "Song title",
  artist: "Artist",
  album: "Album",
  lyric: "Lyrics, or the artist",
  stream: "Audio format",
  format: "Audio format with codec badge",
  detail: "Bit rate and track number",
  modes: "Shuffle and repeat",
  custom: "My own words",
};

export const HOME_TEXT_SOURCES = {
  item: "Highlighted menu item",
  song: "Song playing",
  artist: "Artist playing",
  album: "Album playing",
  custom: "My own words",
};

export const MENU_STYLES = { list: "List", tiles: "Tiles", focus: "One at a time" };

export const BACKGROUNDS = { wallpaper: "Wallpaper", solid: "Solid", gradient: "Gradient" };

export const TEXT_SIZES = { large: "title", medium: "body", small: "small" };
const SIZE_HEIGHTS = { large: 22, medium: 20, small: 16 };
const RING_TICKS = 48;

let nextId = 1;

function frame(x, y, w, h) {
  return { x, y, w, h };
}

function layer(type, wide, tall, props = {}) {
  return { id: nextId++, type, hidden: false, frames: { wide, tall }, ...props };
}

export function layerTypes(screen) {
  return screen === "home" ? HOME_LAYER_TYPES : LAYER_TYPES;
}

export function textSources(screen) {
  return screen === "home" ? HOME_TEXT_SOURCES : TEXT_SOURCES;
}

function playingDesign() {
  return {
    version: 1,
    background: { style: "solid", color: "@background", top: "@header_background", bottom: "@background" },
    layers: [
      layer("cover", frame(10, 8, 112, 112), frame(29, 8, 112, 112), { shape: "square", border: 0, borderColor: "@divider", placeholder: "@divider" }),
      layer("text", frame(132, 8, 180, 22), frame(8, 128, 154, 22), { source: "song", size: "large", align: "left", tallAlign: "center", color: "@text", scroll: true, feedback: false }),
      layer("text", frame(132, 32, 180, 20), frame(8, 152, 154, 20), { source: "lyric", size: "medium", align: "left", tallAlign: "center", color: "@text_secondary", scroll: true, feedback: true }),
      layer("text", frame(132, 54, 180, 20), frame(8, 174, 154, 20), { source: "album", size: "medium", align: "left", tallAlign: "center", color: "@text_tertiary", scroll: true, feedback: false }),
      layer("text", frame(132, 80, 180, 16), frame(8, 200, 154, 16), { source: "format", size: "small", align: "left", tallAlign: "center", color: "@text_secondary", scroll: false, feedback: false }),
      layer("progress", frame(10, 128, 300, 3), frame(8, 236, 154, 3), { style: "bar", color: "@selection_background", track: "@divider", thickness: 3 }),
      layer("times", frame(10, 132, 300, 14), frame(8, 242, 154, 14), { size: "small", color: "@text_secondary", color2: "@text_tertiary" }),
    ],
  };
}

function homeDesign() {
  const panel = "#08080a";
  return {
    version: 1,
    screen: "home",
    background: { style: "wallpaper", color: "@background", top: "@header_background", bottom: "@background" },
    layers: [
      layer("menu", frame(0, 0, 176, 146), frame(0, 0, 170, 152), menuProps()),
      layer("shape", frame(176, 0, 144, 146), frame(0, 152, 170, 128), { kind: "rect", color: panel, radius: 8, thickness: 3 }),
      layer("shape", frame(176, 0, 1, 146), frame(0, 152, 170, 1), { kind: "rect", color: "@divider", radius: 8, thickness: 3 }),
      layer("cover", frame(192, 8, 112, 138), frame(29, 160, 112, 120), { shape: "square", border: 0, borderColor: "@divider", placeholder: panel, reflection: true }),
    ],
  };
}

function menuProps() {
  return {
    style: "list", columns: 2, tileHeight: 40, gap: 6, radius: 8, size: "medium", align: "center",
    tile: "@header_background", tileSelected: "@selection_background", color: "@text", colorSelected: "@selection_text",
    boxed: true,
  };
}

export function defaultDesign(screen = "playing") {
  return screen === "home" ? homeDesign() : playingDesign();
}

export function readDesign(text) {
  try {
    const design = JSON.parse(text);
    if (design && Array.isArray(design.layers)) {
      nextId = Math.max(nextId, ...design.layers.map((item) => (item.id ?? 0) + 1));
      return design;
    }
  } catch {
    return null;
  }
  return null;
}

export function newLayer(type, screen = "playing") {
  const wide = frame(110, 50, 100, 40);
  const tall = frame(35, 120, 100, 40);
  const props = {
    menu: menuProps(),
    cover: { shape: "square", border: 0, borderColor: "@divider", placeholder: "@divider", reflection: false },
    text: { source: screen === "home" ? "item" : "song", custom: "Hello", size: "medium", align: "left", tallAlign: "center", color: "@text", scroll: true, feedback: false },
    times: { size: "small", color: "@text_secondary", color2: "@text_tertiary" },
    progress: { style: "bar", color: "@selection_background", track: "@divider", thickness: 3 },
    shape: { kind: "rounded", color: "@header_background", radius: 8, thickness: 3 },
    playmark: { color: "@text" },
    spinner: { color: "@selection_shine", speed: 3, length: 4 },
  }[type];
  if (type === "text" || type === "times") {
    wide.h = tall.h = 20;
  }
  if (type === "progress") {
    wide.h = tall.h = 3;
  }
  if (type === "playmark") {
    wide.w = wide.h = tall.w = tall.h = 18;
  }
  if (type === "cover" || type === "spinner") {
    wide.w = wide.h = tall.w = tall.h = 80;
  }
  if (type === "menu") {
    Object.assign(wide, frame(10, 10, 200, 120));
    Object.assign(tall, frame(10, 10, 150, 200));
  }
  return layer(type, wide, tall, props);
}

export function colorOf(value, theme) {
  if (typeof value === "string" && value.startsWith("@")) {
    return theme.colors[value.slice(1)] ?? "#ffffff";
  }
  return typeof value === "string" && /^#[0-9a-fA-F]{6}$/.test(value) ? value.toLowerCase() : "#ffffff";
}

function quoted(text) {
  return `"${String(text).replace(/[\\"]/g, (mark) => `\\${mark}`).replace(/\n/g, " ")}"`;
}

function at(item, shape) {
  const f = item.frames[shape];
  return {
    x: `x + ${Math.round(f.x)}`,
    y: `y + ${Math.round(f.y)}`,
    w: Math.max(1, Math.round(f.w)),
    h: Math.max(1, Math.round(f.h)),
    cx: `x + ${Math.round(f.x + f.w / 2)}`,
    cy: `y + ${Math.round(f.y + f.h / 2)}`,
    cx16: `(x + ${Math.round(f.x)}) * 16 + ${Math.round(f.w) * 8}`,
    cy16: `(y + ${Math.round(f.y)}) * 16 + ${Math.round(f.h) * 8}`,
    r: Math.max(1, Math.floor(Math.min(f.w, f.h) / 2)),
  };
}

function lineBox(place, size) {
  const height = Math.min(place.h, SIZE_HEIGHTS[size] ?? 20);
  const drop = Math.floor((place.h - height) / 2);
  return `${place.x}, ${place.y}${drop > 0 ? ` + ${drop}` : ""}, ${place.w}, ${height}`;
}

function textLines(item, place, color, shape) {
  const font = TEXT_SIZES[item.size] ?? "body";
  const align = shape === "tall" ? item.tallAlign ?? item.align ?? "left" : item.align ?? "left";
  const verb = item.scroll ? "line" : "text";
  const box = lineBox(place, item.size);
  const say = (what) => `${verb}(${what}, ${font}, ${box}, ${align}, ${color})`;
  let body;
  if (item.source === "format") {
    body = [`format(${font}, ${box}, ${align}, ${color})`];
  } else if (item.source === "lyric") {
    body = [`if present(lyric) {`, `    ${say("lyric")}`, `} else {`, `    ${say("artist")}`, `}`];
  } else if (item.source === "custom") {
    body = [say(quoted(item.custom ?? ""))];
  } else if (item.source === "item") {
    body = [say("item(selected())")];
  } else {
    body = [say(TEXT_SOURCES[item.source] ? item.source : "song")];
  }
  if (!item.feedback) {
    return body;
  }
  return [`if present(feedback) {`, `    text(feedback, ${font}, ${box}, ${align}, ${color})`, `} else {`, ...body.map((line) => `    ${line}`), `}`];
}

function tileLines(item, place, theme) {
  const columns = Math.max(1, Math.min(6, Math.round(item.columns ?? 2)));
  const gap = Math.max(0, Math.min(40, Math.round(item.gap ?? 6)));
  const tileWidth = Math.max(1, Math.floor((place.w - gap * (columns - 1)) / columns));
  const tileHeight = Math.max(8, Math.min(place.h, Math.round(item.tileHeight ?? 40)));
  const rows = Math.max(1, Math.floor((place.h + gap) / (tileHeight + gap)));
  const font = TEXT_SIZES[item.size] ?? "body";
  const textHeight = Math.min(tileHeight, SIZE_HEIGHTS[item.size] ?? 20);
  const drop = Math.floor((tileHeight - textHeight) / 2);
  const align = item.align ?? "center";
  const radius = Math.max(0, Math.round(item.radius ?? 8));
  const [tile, chosenTile] = [colorOf(item.tile, theme), colorOf(item.tileSelected, theme)];
  const [ink, chosenInk] = [colorOf(item.color, theme), colorOf(item.colorSelected, theme)];
  const label = (verb, colour) =>
    `${verb}(item(k), ${font}, tx + 4, ty + ${drop}, ${Math.max(1, tileWidth - 8)}, ${textHeight}, ${align}, ${colour})`;
  return [
    "let chosen = selected()",
    `let first = max(0, chosen / ${columns} - ${rows - 1}) * ${columns}`,
    `repeat k from first to min(items(), first + ${columns * rows}) {`,
    "    let spot = k - first",
    `    let tx = ${place.x} + spot % ${columns} * ${tileWidth + gap}`,
    `    let ty = ${place.y} + spot / ${columns} * ${tileHeight + gap}`,
    `    rounded(tx, ty, ${tileWidth}, ${tileHeight}, ${radius}, k == chosen ? ${chosenTile} : ${tile})`,
    "    if k == chosen {",
    `        ${label("line", chosenInk)}`,
    "    } else {",
    `        ${label("text", ink)}`,
    "    }",
    "}",
  ];
}

function focusLines(item, place, theme, shape) {
  const font = TEXT_SIZES[item.size] ?? "title";
  const height = Math.min(place.h, SIZE_HEIGHTS[item.size] ?? 22);
  const middle = Math.floor((place.h - height) / 2);
  const align = shape === "tall" ? item.tallAlign ?? item.align ?? "center" : item.align ?? "center";
  const lines = ["let chosen = selected()"];
  if (item.boxed) {
    const box = Math.min(place.h, height + 12);
    lines.push(`rounded(${place.x}, ${place.y} + ${Math.floor((place.h - box) / 2)}, ${place.w}, ${box}, ${Math.max(0, Math.round(item.radius ?? 8))}, ${colorOf(item.tileSelected, theme)})`);
  }
  const ink = colorOf(item.colorSelected, theme);
  const faint = colorOf(item.color, theme);
  lines.push(`line(item(chosen), ${font}, ${place.x} + 6, ${place.y} + ${middle}, ${Math.max(1, place.w - 12)}, ${height}, ${align}, ${ink})`);
  if (place.h >= height + 2 * 16) {
    lines.push(
      "if chosen > 0 {",
      `    text(item(chosen - 1), small, ${place.x} + 6, ${place.y}, ${Math.max(1, place.w - 12)}, 16, ${align}, ${faint})`,
      "}",
      "if chosen + 1 < items() {",
      `    text(item(chosen + 1), small, ${place.x} + 6, ${place.y} + ${place.h - 16}, ${Math.max(1, place.w - 12)}, 16, ${align}, ${faint})`,
      "}",
    );
  }
  return lines;
}

function menuLines(item, place, theme, shape) {
  if (item.style === "tiles") {
    return tileLines(item, place, theme);
  }
  if (item.style === "focus") {
    return focusLines(item, place, theme, shape);
  }
  return [`menu(${place.x}, ${place.y}, ${place.w}, ${place.h})`];
}

function coverLines(item, place, theme) {
  const border = Math.max(0, Math.min(12, Math.round(item.border ?? 0)));
  const edge = colorOf(item.borderColor, theme);
  const empty = colorOf(item.placeholder, theme);
  if (item.shape === "circle") {
    const lines = border > 0 ? [`ring(${place.cx16}, ${place.cy16}, ${(place.r + border) * 16}, ${place.r * 16}, ${edge})`] : [];
    return [...lines, `if has_cover {`, `    round_cover(${place.cx}, ${place.cy}, ${place.r})`, `} else {`,
      `    ring(${place.cx16}, ${place.cy16}, ${place.r * 16}, 0, ${empty})`, `}`];
  }
  const side = Math.min(place.w, place.h);
  const rows = item.reflection ? Math.max(0, place.h - side) : 0;
  const lines = border > 0 ? [`rect(${place.x} - ${border}, ${place.y} - ${border}, ${side + 2 * border}, ${side + 2 * border}, ${edge})`] : [];
  const shown = [`    cover(${place.x}, ${place.y}, ${side})`];
  if (rows > 0) {
    shown.push(`    reflect(${place.x}, ${place.y}, ${side}, ${rows})`);
  }
  return [...lines, `if has_cover {`, ...shown, `} else {`,
    `    rect(${place.x}, ${place.y}, ${side}, ${side}, ${empty})`, `}`];
}

function layerLines(item, shape, theme) {
  const place = at(item, shape);
  const color = colorOf(item.color, theme);
  switch (item.type) {
    case "menu":
      return menuLines(item, place, theme, shape);
    case "cover":
      return coverLines(item, place, theme);
    case "text":
      return textLines(item, place, color, shape);
    case "times":
      return [`times(${TEXT_SIZES[item.size] ?? "small"}, ${lineBox(place, item.size ?? "small")}, ${color}, ${colorOf(item.color2, theme)})`];
    case "progress": {
      const track = colorOf(item.track, theme);
      if (item.style === "ring") {
        const length = Math.max(1, Math.round(item.thickness ?? 6));
        return [`let lit = progress(${RING_TICKS})`, `repeat tick_mark from 0 to ${RING_TICKS} {`,
          `    spoke(${place.cx}, ${place.cy}, tick_mark * 16 / 3, 32, ${Math.max(0, place.r - length)}, ${place.r}, tick_mark < lit ? ${color} : ${track})`, `}`];
      }
      return [`bar(${place.x}, ${place.y}, ${place.w}, ${place.h}, ${track}, ${color}, ${color})`];
    }
    case "shape": {
      const thick = Math.max(1, Math.round(item.thickness ?? 2));
      if (item.kind === "circle") {
        return [`ring(${place.cx16}, ${place.cy16}, ${place.r * 16}, 0, ${color})`];
      }
      if (item.kind === "ring") {
        return [`ring(${place.cx16}, ${place.cy16}, ${place.r * 16}, ${Math.max(0, place.r - thick) * 16}, ${color})`];
      }
      if (item.kind === "line") {
        return [`rect(${place.x}, ${place.y} + ${Math.floor((place.h - thick) / 2)}, ${place.w}, ${thick}, ${color})`];
      }
      if (item.kind === "rounded") {
        return [`rounded(${place.x}, ${place.y}, ${place.w}, ${place.h}, ${Math.max(0, Math.round(item.radius ?? 6))}, ${color})`];
      }
      return [`rect(${place.x}, ${place.y}, ${place.w}, ${place.h}, ${color})`];
    }
    case "playmark":
      return [`icon(badge != none ? badge : (playing ? none : pause), ${place.x}, ${place.y}, ${place.w}, ${place.h}, ${color})`];
    case "spinner": {
      const length = Math.max(1, Math.round(item.length ?? 4));
      const speed = Math.max(1, Math.min(16, Math.round(item.speed ?? 3)));
      return [`spoke(${place.cx}, ${place.cy}, (tick * ${speed}) & 255, 32, ${Math.max(0, place.r - length)}, ${place.r}, ${color})`];
    }
    default:
      return [];
  }
}

function backgroundLines(design, theme) {
  const background = design.background ?? {};
  if (background.style === "wallpaper") {
    return ["backdrop(x, y, width, height)"];
  }
  if (background.style === "gradient") {
    return [`gradient(x, y, width, height, ${colorOf(background.top, theme)}, ${colorOf(background.bottom, theme)})`];
  }
  return [`rect(x, y, width, height, ${colorOf(background.color, theme)})`];
}

export function designScript(design, theme) {
  const side = (shape) => design.layers
    .filter((item) => !item.hidden && item.frames?.[shape])
    .flatMap((item) => ["{", ...layerLines(item, shape, theme).map((line) => `    ${line}`), "}"]);
  const blocks = (lines) => lines.map((line) => `    ${line}`);
  return [...backgroundLines(design, theme), "if width > height {", ...blocks(side("wide")), "} else {",
    ...blocks(side("tall")), "}", ""].join("\n").replace(/^(\s*)\{$/gm, "$1if 1 {");
}
