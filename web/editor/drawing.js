export const AREAS = {
  wide: { top: 24, width: 320, height: 146 },
  tall: { top: 40, width: 170, height: 280 },
};

export const KINDS = {
  box: "Box",
  circle: "Circle or ring",
  line: "Line",
  text: "Text",
  cover: "Album cover",
  symbol: "Play mark",
  number: "Number",
  menu: "Menu",
  group: "Group",
};

export const TEXT_SOURCES = {
  song: "Song title",
  artist: "Artist",
  album: "Album",
  lyric: "Lyrics, or the artist",
  feedback: "Seek and volume messages",
  stream: "Audio format",
  format: "Audio format with codec badge",
  codec: "Codec name",
  codecbox: "Codec name in a box",
  detail: "Bit rate and track number",
  modes: "Shuffle and repeat",
  elapsed: "Time played",
  remaining: "Time left",
  item: "Menu item",
  custom: "My own words",
};

export const VALUES = {
  progress: "the song's position",
  progress_volume: "the song's position, or the volume while it changes",
  volume: "the volume",
  left: "the left level",
  right: "the right level",
  beat: "a beat for each copy",
  copy: "the copy's number",
};

export const CONDITIONS = {
  playing: "playing",
  paused: "paused",
  cover: "the song has a cover",
  nocover: "the song has no cover",
  song: "a song is loaded",
  lyric: "there are lyrics",
  feedback: "seeking or changing volume",
  volume: "the volume is changing",
  shuffle: "shuffle is on",
  shuffle_album: "shuffling albums",
  repeat: "repeat is on",
  repeat_one: "repeating one song",
  repeat_all: "repeating everything",
  badge: "a skip, play or pause mark is showing",
  highlighted: "this menu item is highlighted",
  lit: "the copy is lit by",
  above: "a level is above",
};

export const TEXT_SIZES = { large: "title", medium: "body", small: "small" };
export const MARKS = {
  state: "Pause when paused, skip and play marks",
  playpause: "Play while playing, pause while paused",
  play: "Play",
  pause: "Pause",
  next: "Next",
  previous: "Previous",
  badge: "The skip, play or pause mark, while it shows",
};
export const NUMBERS = { time: "Time played", time_volume: "Time played, or the volume while it changes", track: "Track number", volume: "Volume" };
const FULL = 1000;
const TURN = 256;
const LEVEL_TOP = 62;
const BEAT_TOP = 8;

let nextId = 1;

function box(x, y, w, h) {
  return { x, y, w, h };
}

export function element(kind, wide, tall, props = {}) {
  return { id: nextId++, kind, name: "", hidden: false, at: { wide, tall }, bind: {}, ...defaults(kind), ...props };
}

function defaults(kind) {
  return {
    box: { fill: "solid", color: "@header_background", top: "@header_top", bottom: "@header_background", radius: 0, border: 0, borderColor: "@divider", taper: 0, grid: 0, gridColor: "@background" },
    circle: { color: "@selection_background", thickness: 0 },
    line: { color: "@text", angle: 0, start: 0, thickness: 2 },
    text: { source: "song", custom: "Hello", size: "medium", align: "left", color: "@text", scroll: true },
    cover: { shape: "square", placeholder: "@divider", reflection: false },
    symbol: { mark: "state", color: "@text" },
    number: { show: "time", lit: "@text", dim: "@divider", lean: 8 },
    menu: {},
    group: { children: [], clip: false, repeat: null },
  }[kind];
}

export function newElement(kind, screen = "playing") {
  const sizes = {
    box: [100, 40], circle: [60, 60], line: [40, 0], text: [140, 20], cover: [80, 80], symbol: [18, 18],
    number: [100, 32], menu: [200, 120], group: [120, 80],
  }[kind];
  const at = {};
  for (const [shape, area] of Object.entries(AREAS)) {
    const [w, h] = sizes;
    at[shape] = box(Math.round((area.width - Math.min(w, area.width)) / 2), Math.round((area.height - Math.max(h, 2)) / 2), Math.min(w, area.width), h);
  }
  const made = element(kind, at.wide, at.tall);
  if (kind === "text" && screen === "home") {
    made.source = "item";
  }
  return made;
}

export function readDrawing(text) {
  try {
    const drawing = JSON.parse(text);
    if (drawing?.version === 2 && Array.isArray(drawing.elements)) {
      bumpIds(drawing.elements);
      return drawing;
    }
  } catch {
    return null;
  }
  return null;
}

function bumpIds(elements) {
  for (const item of elements) {
    nextId = Math.max(nextId, (item.id ?? 0) + 1);
    if (item.kind === "group") {
      bumpIds(item.children ?? []);
    }
  }
}

export function walk(elements, visit, parent = null) {
  for (const item of elements) {
    visit(item, parent);
    if (item.kind === "group") {
      walk(item.children ?? [], visit, item);
    }
  }
}

export function findElement(drawing, id) {
  let found = null;
  walk(drawing.elements, (item, parent) => {
    if (item.id === id) {
      found = { item, parent };
    }
  });
  return found;
}

export function colorOf(value, theme) {
  if (typeof value === "string" && value.startsWith("@")) {
    return theme.colors[value.slice(1)] ?? "#ffffff";
  }
  return typeof value === "string" && /^#[0-9a-fA-F]{6}$/.test(value) ? value.toLowerCase() : "#ffffff";
}

function whole(value, fallback = 0) {
  const number = Math.round(Number(value));
  return Number.isFinite(number) ? number : fallback;
}

const NUMBER = /^-?\d+$/;

const TAIL = /^([A-Za-z_][A-Za-z0-9_]*) ([+-]) (\d+)$/;

function sum(a, b) {
  let [first, second] = [String(a), String(b)];
  if (NUMBER.test(first) && !NUMBER.test(second)) {
    [first, second] = [second, first];
  }
  if (NUMBER.test(first) && NUMBER.test(second)) {
    return String(Number(first) + Number(second));
  }
  if (!NUMBER.test(second)) {
    return `${first} + ${second}`;
  }
  let base = first;
  let total = Number(second);
  const tail = TAIL.exec(first);
  if (tail) {
    base = tail[1];
    total += tail[2] === "-" ? -Number(tail[3]) : Number(tail[3]);
  }
  if (total === 0) {
    return base;
  }
  return total < 0 ? `${base} - ${-total}` : `${base} + ${total}`;
}

function less(a, n) {
  return n === 0 ? String(a) : sum(a, String(-n));
}

const ARGUMENT = /([(,] ?)([A-Za-z_][A-Za-z0-9_]* [+-] \d+)(?=[,)])/g;
const MOST_HOISTED = 6;

function baseOf(value) {
  return /^[A-Za-z_][A-Za-z0-9_]*/.exec(String(value))?.[0] ?? null;
}

function hoisted(lines, origins, tag) {
  const bases = new Set(origins.map(baseOf).filter(Boolean));
  const counts = new Map();
  for (const line of lines) {
    for (const found of line.matchAll(ARGUMENT)) {
      if (bases.has(baseOf(found[2]))) {
        counts.set(found[2], (counts.get(found[2]) ?? 0) + 1);
      }
    }
  }
  const chosen = [...counts].filter(([, count]) => count > 1).sort((a, b) => b[1] - a[1]).slice(0, MOST_HOISTED);
  if (chosen.length === 0) {
    return lines;
  }
  const names = new Map(chosen.map(([expression], index) => [expression, `p${tag}_${index}`]));
  const replaced = lines.map((line) => line.replace(ARGUMENT, (whole, before, expression) =>
    names.has(expression) ? `${before}${names.get(expression)}` : whole));
  return [...[...names].map(([expression, name]) => `let ${name} = ${expression}`), ...replaced];
}

function quoted(text) {
  return `"${String(text).replace(/[\\"]/g, (mark) => `\\${mark}`).replace(/\n/g, " ")}"`;
}

function angleUnits(degrees) {
  return ((Math.round((Number(degrees) || 0) * TURN / 360) % TURN) + TURN) % TURN;
}

function valueOf(source, scope) {
  switch (source) {
    case "progress":
      return `progress(${FULL})`;
    case "progress_volume":
      return `(volume_shown ? volume_width(${FULL}) : progress(${FULL}))`;
    case "volume":
      return `volume_width(${FULL})`;
    case "left":
      scope.needs.levels = true;
      return `(level_l * ${FULL} / ${LEVEL_TOP})`;
    case "right":
      scope.needs.levels = true;
      return `(level_r * ${FULL} / ${LEVEL_TOP})`;
    case "beat":
      return scope.copy ? `(${scope.copy.beat()} * ${FULL / BEAT_TOP})` : "0";
    case "copy":
      return scope.copy ? `(${scope.copy.name} * ${FULL} / ${Math.max(1, scope.copy.last)})` : "0";
    default:
      return "0";
  }
}

function conditionOf(when, scope) {
  const test = when?.on ? plainCondition(when, scope) : null;
  return test && when.not ? `!(${test})` : test;
}

function plainCondition(when, scope) {
  switch (when.on) {
    case "playing":
      return "playing";
    case "paused":
      return "paused";
    case "cover":
      return "has_cover";
    case "nocover":
      return "!has_cover";
    case "song":
      return "present(song)";
    case "lyric":
      return "present(lyric)";
    case "feedback":
      return "present(feedback)";
    case "volume":
      return "volume_shown";
    case "shuffle":
      return "shuffle_mode != 0";
    case "shuffle_album":
      return "shuffle_mode == 2";
    case "repeat":
      return "repeat_mode != 0";
    case "repeat_one":
      return "repeat_mode == 1";
    case "repeat_all":
      return "repeat_mode == 2";
    case "badge":
      return "badge != none";
    case "highlighted":
      return scope.copy?.items ? `${scope.copy.name} == chosen_${scope.copy.depth}` : "0";
    case "lit": {
      if (!scope.copy) {
        return "0";
      }
      const source = when.source ?? "progress";
      const reach = `${valueOf(source, scope)} * ${scope.copy.count} / ${FULL}`;
      if (source === "beat" || source === "copy") {
        return `${scope.copy.name} < ${reach}`;
      }
      const name = `lit_${scope.copy.depth}_${source}`;
      scope.copy.before.set(name, reach);
      return `${scope.copy.name} < ${name}`;
    }
    case "above":
      return `${valueOf(when.source ?? "left", scope)} >= ${Math.round(Math.max(0, Math.min(100, Number(when.level) || 0)) * FULL / 100)}`;
    default:
      return null;
  }
}

function numberOf(item, key, base, scope) {
  const rule = item.bind?.[key];
  if (!rule?.source || rule.source === "spin") {
    return String(base);
  }
  if (key === "w" || key === "h") {
    const from = Math.max(0, Math.min(100, whole(rule.from, 0)));
    const to = Math.max(0, Math.min(100, whole(rule.to, 100)));
    const value = valueOf(rule.source, scope);
    if (from === 0 && to === 100) {
      return `(${base} * ${value} / ${FULL})`;
    }
    return `(${base} * (${from * FULL} + ${to - from} * ${value}) / ${100 * FULL})`;
  }
  const from = whole(rule.from, base);
  const to = whole(rule.to, base);
  return `(${from} + ${to - from} * ${valueOf(rule.source, scope)} / ${FULL})`;
}

function colourOf(item, key, theme, scope) {
  const plain = colorOf(item[key], theme);
  const rule = item.bind?.[key];
  const test = rule ? conditionOf(rule.when, scope) : null;
  return test ? `(${test} ? ${colorOf(rule.color, theme)} : ${plain})` : plain;
}

function anchor(scope, ax, ay) {
  if (!scope.turn) {
    return [sum(scope.ox, ax), sum(scope.oy, ay)];
  }
  const { px, py, angle } = scope.turn;
  const known = NUMBER.test(String(ax)) && NUMBER.test(String(ay));
  const [rx, ry] = known ? [String(Number(ax) - px), String(Number(ay) - py)] : [`(${ax} - ${px})`, `(${ay} - ${py})`];
  const term = (factor, wave) => factor === "0" ? null : `${factor} * ${wave}(${angle})`;
  const turned = (first, second, joiner) => {
    const parts = [first, second].filter(Boolean);
    if (parts.length === 0) {
      return null;
    }
    const joined = parts.length === 2 ? `${parts[0]} ${joiner} ${parts[1]}` : joiner === "-" && !first ? `0 - ${parts[0]}` : parts[0];
    return `(${joined}) / 255`;
  };
  const across = turned(term(rx, "cos"), term(ry, "sin"), "-");
  const down = turned(term(rx, "sin"), term(ry, "cos"), "+");
  const centreX = sum(scope.ox, px);
  const centreY = sum(scope.oy, py);
  return [across ? `${centreX} + ${across}` : centreX, down ? `${centreY} + ${down}` : centreY];
}

function fillsScreen(item, at, scope) {
  const area = AREAS[scope.shape];
  return item.kind === "box" && scope.depth === 0 && !scope.copy && !item.bind?.w && !item.bind?.h
    && whole(at.x) === 0 && whole(at.y) === 0 && whole(at.w) === area.width && whole(at.h) === area.height;
}

function placed(item, scope) {
  const at = item.at[scope.shape];
  if (fillsScreen(item, at, scope)) {
    return { at, x: "0", y: "0", w: "width", h: "height" };
  }
  const x = numberOf(item, "x", whole(at.x), scope);
  const y = numberOf(item, "y", whole(at.y), scope);
  const w = numberOf(item, "w", Math.max(0, whole(at.w)), scope);
  const h = numberOf(item, "h", Math.max(0, whole(at.h)), scope);
  return { at, x, y, w, h };
}

function gridLines(item, theme, scope, [ix, iy, iw, ih]) {
  const grid = Math.max(0, whole(item.grid));
  return grid > 1 ? [`lattice(${ix}, ${iy}, ${iw}, ${ih}, ${grid}, ${colourOf(item, "gridColor", theme, scope)})`] : [];
}

function boxLines(item, scope, theme) {
  const { x, y, w, h } = placed(item, scope);
  const [left, top] = anchor(scope, x, y);
  const lines = [];
  const border = Math.max(0, whole(item.border));
  const radius = Math.max(0, whole(item.radius));
  const inset = border;
  let [bx, by, bw, bh] = [left, top, w, h];
  if (border > 0) {
    const edge = colourOf(item, "borderColor", theme, scope);
    lines.push(`let bx = ${left}`, `let by = ${top}`, `let bw = ${w}`, `let bh = ${h}`);
    [bx, by, bw, bh] = ["bx", "by", "bw", "bh"];
    lines.push(radius > 0 ? `rounded(bx, by, bw, bh, ${radius}, ${edge})` : `rect(bx, by, bw, bh, ${edge})`);
  }
  const ix = sum(bx, inset);
  const iy = sum(by, inset);
  const iw = less(bw, 2 * inset);
  const ih = less(bh, 2 * inset);
  const painted = item.fill === "none" ? null : item.fill;
  if (!painted) {
    return [...lines, ...gridLines(item, theme, scope, [ix, iy, iw, ih])];
  }
  if (painted === "wallpaper") {
    lines.push(`backdrop(${ix}, ${iy}, ${iw}, ${ih})`);
  } else if (item.fill === "gradient") {
    lines.push(`gradient(${ix}, ${iy}, ${iw}, ${ih}, ${colourOf(item, "top", theme, scope)}, ${colourOf(item, "bottom", theme, scope)})`);
  } else if (whole(item.taper) > 0) {
    lines.push(`trapezoid(${ix}, ${iy}, ${iw}, ${ih}, ${whole(item.taper)}, ${colourOf(item, "color", theme, scope)})`);
  } else if (Math.max(0, radius - inset) > 0) {
    lines.push(`rounded(${ix}, ${iy}, ${iw}, ${ih}, ${Math.max(0, radius - inset)}, ${colourOf(item, "color", theme, scope)})`);
  } else {
    lines.push(`rect(${ix}, ${iy}, ${iw}, ${ih}, ${colourOf(item, "color", theme, scope)})`);
  }
  return [...lines, ...gridLines(item, theme, scope, [ix, iy, iw, ih])];
}

function circleLines(item, scope, theme) {
  const { at, w, h } = placed(item, scope);
  const [cx, cy] = anchor(scope, whole(at.x) + Math.floor(whole(at.w) / 2), whole(at.y) + Math.floor(whole(at.h) / 2));
  const side = Math.max(1, Math.min(whole(at.w), whole(at.h)));
  const bound = item.bind?.w?.source || item.bind?.h?.source;
  const thickness = Math.max(0, whole(item.thickness));
  const centre = (value) => NUMBER.test(value) ? String(Number(value) * 16 + 8) : `(${value}) * 16 + 8`;
  const colour = colourOf(item, "color", theme, scope);
  if (!bound) {
    const radius = side * 8;
    return [`ring(${centre(cx)}, ${centre(cy)}, ${radius}, ${thickness > 0 ? Math.max(0, radius - thickness * 16) : 0}, ${colour})`];
  }
  return [
    `let cr = min(${w}, ${h}) * 8`,
    `ring(${centre(cx)}, ${centre(cy)}, cr, ${thickness > 0 ? `max(0, cr - ${thickness * 16})` : "0"}, ${colour})`,
  ];
}

function lineLines(item, scope, theme) {
  const at = item.at[scope.shape];
  const [px, py] = anchor(scope, numberOf(item, "x", whole(at.x), scope), numberOf(item, "y", whole(at.y), scope));
  const rule = item.bind?.angle;
  const base = angleUnits(rule?.from ?? item.angle);
  let angle = String(base);
  if (rule?.source === "spin") {
    angle = `(${base} + tick * ${Math.max(1, Math.min(16, whole(rule.speed, 3)))})`;
  } else if (rule?.source) {
    const to = angleUnits(rule.to);
    const span = ((to - base + TURN + TURN / 2) % TURN) - TURN / 2;
    angle = `(${base} + ${span} * ${valueOf(rule.source, scope)} / ${FULL})`;
  }
  let wrapped = false;
  if (scope.turn) {
    wrapped = angle === "0";
    angle = wrapped ? scope.turn.angle : `(${angle} + ${scope.turn.angle})`;
  }
  const start = Math.max(0, whole(item.start));
  const length = numberOf(item, "w", Math.max(1, whole(at.w)), scope);
  const thick = Math.max(1, Math.round((Number(item.thickness) || 1) * 16));
  const turned = NUMBER.test(angle) || wrapped ? angle : `(${angle}) & 255`;
  return [`spoke(${px}, ${py}, ${turned}, ${thick}, ${start}, ${sum(start, length)}, ${colourOf(item, "color", theme, scope)})`];
}

function textLines(item, scope, theme) {
  const { at, x, y, w } = placed(item, scope);
  const [left, top] = anchor(scope, x, y);
  const height = Math.max(1, whole(at.h));
  const font = TEXT_SIZES[item.size] ?? "body";
  const colour = colourOf(item, "color", theme, scope);
  const where = `${left}, ${top}, ${w}, ${height}`;
  const align = at.align ?? item.align ?? "left";
  if (item.source === "format") {
    return [`format(${font}, ${where}, ${align}, ${colour})`];
  }
  if (item.source === "codecbox") {
    const from = { center: `${left} + (${w} - cw) / 2`, right: `${sum(left, w)} - cw` }[align] ?? left;
    return [`let cw = min(width(codec, ${font}) + 8, ${w})`, `codec(${font}, ${from}, ${top}, cw, ${height}, ${colour})`];
  }
  const verb = item.scroll ? "line" : "text";
  let what = item.source;
  if (item.source === "custom") {
    what = quoted(item.custom ?? "");
  } else if (item.source === "item") {
    what = scope.copy?.items ? `item(${scope.copy.name})` : "item(selected())";
  } else if (item.source === "lyric") {
    return ["if present(lyric) {", `    ${verb}(lyric, ${font}, ${where}, ${align}, ${colour})`, "} else {",
      `    ${verb}(artist, ${font}, ${where}, ${align}, ${colour})`, "}"];
  } else if (!TEXT_SOURCES[item.source]) {
    what = "song";
  }
  return [`${verb}(${what}, ${font}, ${where}, ${align}, ${colour})`];
}

function coverLines(item, scope, theme) {
  const at = item.at[scope.shape];
  const side = Math.max(1, Math.min(whole(at.w), whole(at.h)));
  const empty = colourOf(item, "placeholder", theme, scope);
  if (item.shape === "circle") {
    const r = Math.floor(side / 2);
    const [cx, cy] = anchor(scope, whole(at.x) + r, whole(at.y) + r);
    return ["if has_cover {", `    round_cover(${cx}, ${cy}, ${r})`, "} else {",
      `    ring((${cx}) * 16 + 8, (${cy}) * 16 + 8, ${r * 16}, 0, ${empty})`, "}"];
  }
  const [left, top] = anchor(scope, whole(at.x), whole(at.y));
  const rows = item.reflection ? Math.max(0, whole(at.h) - side) : 0;
  const shown = [`    cover(${left}, ${top}, ${side})`];
  if (rows > 0) {
    shown.push(`    reflect(${left}, ${top}, ${side}, ${rows})`);
  }
  return ["if has_cover {", ...shown, "} else {", `    rect(${left}, ${top}, ${side}, ${side}, ${empty})`, "}"];
}

function symbolLines(item, scope, theme) {
  const { x, y, w, h } = placed(item, scope);
  const [left, top] = anchor(scope, x, y);
  const marks = { state: "badge != none ? badge : (playing ? none : pause)", playpause: "playing ? play : pause", badge: "badge" };
  const mark = marks[item.mark] ?? (MARKS[item.mark] ? item.mark : marks.state);
  return [`icon(${mark}, ${left}, ${top}, ${w}, ${h}, ${colourOf(item, "color", theme, scope)})`];
}

function numberLines(item, scope, theme) {
  const at = item.at[scope.shape];
  const [left, top] = anchor(scope, whole(at.x), whole(at.y));
  const tall = Math.max(8, whole(at.h));
  const wide = Math.max(5, Math.round(tall * 0.55));
  const thick = Math.max(2, Math.round(tall / 8));
  const lean = Math.max(0, whole(item.lean));
  const step = wide + Math.round(wide * 3 / 13);
  const plainName = (value) => /^[A-Za-z_][A-Za-z0-9_]*$/.test(value);
  const lines = [];
  const nx = plainName(left) ? left : "nx";
  const ny = plainName(top) ? top : "ny";
  if (nx === "nx") {
    lines.push(`let nx = ${left}`);
  }
  if (ny === "ny") {
    lines.push(`let ny = ${top}`);
  }
  lines.push(`segment_style(${wide}, ${tall}, ${thick}, ${lean}, ${colourOf(item, "lit", theme, scope)}, ${colourOf(item, "dim", theme, scope)})`);
  if (item.show === "track" || item.show === "volume") {
    const value = item.show === "track" ? "min(track, 999)" : "min(volume, 99)";
    const digits = item.show === "track" ? 3 : 2;
    lines.push(`let number = ${value}`);
    for (let index = 0; index < digits; index++) {
      const place = 10 ** (digits - 1 - index);
      const shown = place === 1 ? `segments(number % 10)` : `number >= ${place} ? segments(number / ${place} % 10) : 0`;
      lines.push(`digit(${sum(nx, index * step)}, ${ny}, ${shown})`);
    }
    return lines;
  }
  const both = item.show === "time_volume";
  const wider = Math.round(wide * 8 / 22);
  const narrow = Math.round(wide * 4 / 22);
  const xs = [0, wide + wider, 2 * wide + wider + narrow, 3 * wide + 2 * wider + narrow, 4 * wide + 2 * wider + 2 * narrow];
  lines.push(
    "let secs = min((shown >>> 3) / 125, 35999)",
    "let hours = secs / 3600",
    "let minutes = secs / 60 % 60",
    "let rest = secs % 60",
  );
  if (!both) {
    lines.push(
      `digit(${sum(nx, xs[0])}, ${ny}, hours > 0 ? segments(hours % 10) : 0)`,
      `digit(${sum(nx, xs[1])}, ${ny}, minutes >= 10 || hours > 0 ? segments(minutes / 10 % 10) : 0)`,
      `digit(${sum(nx, xs[2])}, ${ny}, segments(minutes % 10))`,
      `digit(${sum(nx, xs[3])}, ${ny}, segments(rest / 10 % 10))`,
      `digit(${sum(nx, xs[4])}, ${ny}, segments(rest % 10))`,
      `colon(${sum(nx, xs[0] + wide)}, ${ny}, hours > 0)`,
      `colon(${sum(nx, xs[2] + wide)}, ${ny}, 1)`,
    );
    return lines;
  }
  lines.push(
    "let level = min(volume, 99)",
    "let vs = volume_shown",
    `digit(${sum(nx, xs[0])}, ${ny}, vs ? 0 : hours > 0 ? segments(hours % 10) : 0)`,
    `digit(${sum(nx, xs[1])}, ${ny}, vs ? 0 : minutes >= 10 || hours > 0 ? segments(minutes / 10 % 10) : 0)`,
    `digit(${sum(nx, xs[2])}, ${ny}, vs ? 0 : segments(minutes % 10))`,
    `digit(${sum(nx, xs[3])}, ${ny}, vs ? (level >= 10 ? segments(level / 10 % 10) : 0) : segments(rest / 10 % 10))`,
    `digit(${sum(nx, xs[4])}, ${ny}, segments(vs ? level % 10 : rest % 10))`,
    `colon(${sum(nx, xs[0] + wide)}, ${ny}, !vs && hours > 0)`,
    `colon(${sum(nx, xs[2] + wide)}, ${ny}, !vs)`,
  );
  return lines;
}

function menuLines(item, scope) {
  const { x, y, w, h } = placed(item, scope);
  const [left, top] = anchor(scope, x, y);
  return [`menu(${left}, ${top}, ${w}, ${h})`];
}

function beatOf(name, count) {
  return [
    `let beat_${name} = 0`,
    "if playing {",
    `    beat_${name} = -100`,
    `    repeat back_${name} from 0 to 3 {`,
    `        let f = tick - back_${name}`,
    `        let v = f * 0x9E3779B1 + ${name} * 0x85EBCA6B`,
    "        v = v ^ (v >>> 15)",
    "        v = v * 0x2C1B3C6D",
    "        v = v ^ (v >>> 12)",
    "        let phase = (f >>> 1) % 3 * 2 + (f & 1)",
    "        let pulse = phase == 0 ? 3 : phase == 1 ? 1 : 0",
    `        beat_${name} = max(beat_${name}, 5 - 4 * ${name} / ${count} + (${name} < ${count} / 2 ? pulse : 0) + (v >>> 30) - ((v >>> 28) & 3) / 2 - 2 * back_${name})`,
    "    }",
    `    beat_${name} = min(max(beat_${name}, 0), ${BEAT_TOP})`,
    "}",
  ];
}

function groupLines(item, scope, theme) {
  const { at } = placed(item, scope);
  const [gx, gy] = anchor(scope, numberOf(item, "x", whole(at.x), scope), numberOf(item, "y", whole(at.y), scope));
  const depth = (scope.depth ?? 0) + 1;
  const simple = (value) => NUMBER.test(value) || TAIL.test(value) || /^[A-Za-z_][A-Za-z0-9_]*$/.test(value);
  const atZero = (key) => (item.children ?? []).some((child) => child.at?.[scope.shape] && whole(child.at[scope.shape][key]) === 0);
  const nameX = !simple(gx) || (atZero("x") && TAIL.test(gx));
  const nameY = !simple(gy) || (atZero("y") && TAIL.test(gy));
  const lines = [];
  if (nameX) {
    lines.push(`let gx_${depth} = ${gx}`);
  }
  if (nameY) {
    lines.push(`let gy_${depth} = ${gy}`);
  }
  const [ox, oy] = [nameX ? `gx_${depth}` : gx, nameY ? `gy_${depth}` : gy];
  if (item.clip) {
    lines.push(`clip(${ox}, ${oy}, ${Math.max(1, whole(at.w))}, ${Math.max(1, whole(at.h))})`);
  }
  const inner = { ...scope, ox, oy, depth, turn: null };
  const repeat = item.repeat;
  if (!repeat) {
    lines.push(...hoisted(childLines(item.children, inner, theme), [ox, oy], depth));
  } else {
    lines.push(...repeatLines(item, repeat, inner, theme));
  }
  if (item.clip) {
    lines.push("unclip()");
  }
  return lines;
}

function repeatLines(item, repeat, inner, theme) {
  const depth = inner.depth;
  const name = `copy_${depth}`;
  const at = item.at[inner.shape];
  const items = repeat.count === "items";
  const columns = Math.max(1, whole(repeat.columns, 1));
  const dx = whole(repeat.dx);
  const dy = whole(repeat.dy);
  const lines = [];
  let first = "0";
  let last;
  let count;
  let header;
  if (items) {
    const rows = dy > 0 ? Math.max(1, Math.floor((whole(at.h) + 1) / dy)) : 1;
    const shown = columns * rows;
    lines.push(`let chosen_${depth} = selected()`, `let first_${depth} = max(0, chosen_${depth} / ${columns} - ${rows - 1}) * ${columns}`);
    first = `first_${depth}`;
    count = shown;
    last = shown - 1;
    header = `repeat ${name} from first_${depth} to min(items(), first_${depth} + ${shown}) {`;
  } else {
    count = Math.max(1, Math.min(96, whole(repeat.count, 1)));
    last = count - 1;
    header = `repeat ${name} from 0 to ${count} {`;
  }
  const spot = items ? `(${name} - ${first})` : name;
  const body = [];
  const turnStep = Math.round((Number(repeat.turn) || 0) * TURN * 1000 / 360);
  const spin = whole(repeat.spin) ? ` + tick * ${Math.max(-16, Math.min(16, whole(repeat.spin)))}` : "";
  const startTurn = angleUnits(repeat.start ?? 0);
  const turning = turnStep !== 0 || spin;
  const owner = inner.copy?.beatOwner ?? null;
  const copy = { name, depth, items, count, last, before: new Map(), beatOwner: owner, beat: () => `beat_${owner ?? name}` };
  const across = dx === 0 || columns === 1 ? null : count <= columns ? `${spot} * ${dx}` : `${spot} % ${columns} * ${dx}`;
  const down = dy === 0 || count <= columns ? null : columns === 1 ? `${spot} * ${dy}` : `${spot} / ${columns} * ${dy}`;
  if (across) {
    body.push(`let cx_${depth} = ${inner.ox} + ${across}`);
  }
  if (down) {
    body.push(`let cy_${depth} = ${inner.oy} + ${down}`);
  }
  const uses = JSON.stringify(item.children);
  if (uses.includes("\"beat\"") && !owner) {
    body.push(...beatOf(name, count));
    copy.beatOwner = name;
  }
  const scope = { ...inner, ox: across ? `cx_${depth}` : inner.ox, oy: down ? `cy_${depth}` : inner.oy, copy };
  if (turning) {
    const offset = startTurn ? `${startTurn} + ` : "";
    body.push(`let turn_${depth} = (${offset}${name} * ${turnStep} / 1000${spin}) & 255`);
    scope.turn = { px: Math.floor(whole(at.w) / 2), py: Math.floor(whole(at.h) / 2), angle: `turn_${depth}` };
  }
  body.push(...hoisted(childLines(item.children, scope, theme), [scope.ox, scope.oy], depth));
  lines.push(...[...copy.before].map(([litName, reach]) => `let ${litName} = ${reach}`), header, ...indented(body), "}");
  return lines;
}

function indented(lines) {
  return lines.map((line) => `    ${line}`);
}

const EMITTERS = {
  box: boxLines,
  circle: circleLines,
  line: lineLines,
  text: textLines,
  cover: coverLines,
  symbol: symbolLines,
  number: numberLines,
  menu: menuLines,
  group: groupLines,
};

function elementLines(item, scope, theme) {
  const emit = EMITTERS[item.kind];
  if (!emit || item.hidden || !item.at?.[scope.shape]) {
    return [];
  }
  const lines = emit(item, scope, theme);
  const show = conditionOf(item.bind?.show?.when, scope);
  if (show) {
    return [`if ${show} {`, ...indented(lines), "}"];
  }
  return lines.some((line) => /^let /.test(line)) ? ["{", ...indented(lines), "}"] : lines;
}

function childLines(children, scope, theme) {
  return (children ?? []).flatMap((child) => elementLines(child, scope, theme));
}

function levelLines() {
  return [
    "let level_l = 0",
    "let level_r = 0",
    "if playing {",
    "    repeat channel from 0 to 2 {",
    "        let sum = 7680 + 11 * sin(tick * (channel == 0 ? 3 : 4) + (channel == 0 ? 0 : 96))",
    "        sum = sum + 7 * sin(tick * (channel == 0 ? 7 : 6) + (channel == 0 ? 64 : 160))",
    "        sum = sum + 3 * sin(tick * (channel == 0 ? 17 : 13) + (channel == 0 ? 0 : 48))",
    "        let t = tick + 29 * channel",
    "        let kick = ((t >>> 1) % 53 * 2 + (t & 1)) % 53 < 2 ? 10 : 0",
    `        let v = min(sum / 256 + kick, ${LEVEL_TOP})`,
    "        if channel == 0 {",
    "            level_l = v",
    "        } else {",
    "            level_r = v",
    "        }",
    "    }",
    "}",
  ];
}

function sideLines(drawing, shape, theme) {
  const scope = { shape, ox: "x", oy: "y", depth: 0, turn: null, copy: null, needs: { levels: false } };
  const lines = childLines(drawing.elements, scope, theme);
  return { lines, levels: scope.needs.levels };
}

const SHARED_LEAST = 6;

function sameBothWays(item, theme) {
  const { wide, tall } = item.at ?? {};
  if (!wide || !tall || item.hidden) {
    return null;
  }
  const plain = (shape) => ({ shape, ox: "x", oy: "y", depth: 0, turn: null, copy: null, needs: { levels: false } });
  const [first, second] = [plain("wide"), plain("tall")];
  const once = elementLines(item, first, theme);
  if (once.join("\n") === elementLines(item, second, theme).join("\n")) {
    return { lines: once, levels: first.needs.levels };
  }
  if (whole(wide.w) !== whole(tall.w) || whole(wide.h) !== whole(tall.h)) {
    return null;
  }
  const scope = (shape) => ({ shape, ox: "at_x", oy: "at_y", depth: 0, turn: null, copy: null, needs: { levels: false } });
  const shifted = (shape) => ({ ...item, at: { ...item.at, [shape]: { ...item.at[shape], x: 0, y: 0 } } });
  const a = scope("wide");
  const b = scope("tall");
  const sideways = elementLines(shifted("wide"), a, theme);
  const upright = elementLines(shifted("tall"), b, theme);
  if (sideways.join("\n") !== upright.join("\n") || sideways.length < SHARED_LEAST) {
    return null;
  }
  const pick = (base, a, b) => whole(a) === whole(b) ? sum(base, whole(a)) : `width > height ? ${sum(base, whole(a))} : ${sum(base, whole(b))}`;
  return {
    lines: ["{", `    let at_x = ${pick("x", wide.x, tall.x)}`,
      `    let at_y = ${pick("y", wide.y, tall.y)}`, ...indented(hoisted(sideways, ["at_x", "at_y"], 0)), "}"],
    levels: a.needs.levels,
  };
}

export function drawingScript(drawing, theme) {
  const body = [];
  let levels = false;
  let run = { wide: [], tall: [] };
  const flush = () => {
    const [sideways, upright] = [hoisted(run.wide, ["x", "y"], 0), hoisted(run.tall, ["x", "y"], 0)];
    if (sideways.length && upright.length) {
      body.push("if width > height {", ...indented(sideways), "} else {", ...indented(upright), "}");
    } else if (sideways.length) {
      body.push("if width > height {", ...indented(sideways), "}");
    } else if (upright.length) {
      body.push("if width <= height {", ...indented(upright), "}");
    }
    run = { wide: [], tall: [] };
  };
  for (const item of drawing.elements) {
    const shared = sameBothWays(item, theme);
    if (shared) {
      flush();
      body.push(...shared.lines);
      levels ||= shared.levels;
      continue;
    }
    const sideways = sideLines({ elements: [item] }, "wide", theme);
    const upright = sideLines({ elements: [item] }, "tall", theme);
    levels ||= sideways.levels || upright.levels;
    run.wide.push(...sideways.lines);
    run.tall.push(...upright.lines);
  }
  flush();
  return [...(levels ? levelLines() : []), ...body, ""].join("\n").replace(/^(\s*)\{$/gm, "$1if 1 {");
}

export function emptyDrawing(screen) {
  return {
    version: 2,
    screen,
    elements: [element("box", box(0, 0, 320, 146), box(0, 0, 170, 280), { name: "Background", fill: screen === "home" ? "wallpaper" : "solid", color: "@background" })],
  };
}
