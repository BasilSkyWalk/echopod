import { element, emptyDrawing, newElement, readDrawing } from "./drawing.js";

const UNITS_TO_DEGREES = 360 / 256;
const LIGHTS = 48;

function at(x, y, w, h, align) {
  return align ? { x, y, w, h, align } : { x, y, w, h };
}

function same(x, y, w, h, align) {
  return [at(x, y, w, h, align), at(x, y, w, h, align)];
}

function fill(source) {
  return { w: { source, from: 0, to: 100 } };
}

function when(on, extra = {}) {
  return { when: { on, ...extra } };
}

export function standardDrawing(screen) {
  if (screen === "home") {
    return {
      version: 2,
      screen,
      elements: [
        element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Background", locked: true, fill: "wallpaper" }),
        element("menu", at(0, 0, 176, 146), at(0, 0, 170, 152), { name: "Menu" }),
        element("box", at(176, 0, 144, 146), at(0, 152, 170, 128), { name: "Art panel", color: "#08080a" }),
        element("box", at(176, 0, 1, 146), at(0, 152, 170, 1), { name: "Divider", color: "@divider" }),
        element("cover", at(192, 8, 112, 138), at(29, 160, 112, 120), { reflection: true, placeholder: "#08080a" }),
      ],
    };
  }
  return {
    version: 2,
    screen,
    elements: [
      element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Background", locked: true, color: "@background" }),
      element("cover", at(10, 8, 112, 112), at(29, 8, 112, 112), { placeholder: "@divider" }),
      element("text", at(132, 8, 180, 22), at(8, 128, 154, 22, "center"), { source: "song", size: "large", color: "@text" }),
      element("text", at(132, 32, 180, 20), at(8, 152, 154, 20, "center"), { name: "Messages", source: "feedback", color: "@text_secondary", bind: { show: when("feedback") } }),
      element("text", at(132, 32, 180, 20), at(8, 152, 154, 20, "center"), { source: "lyric", color: "@text_secondary", bind: { show: when("feedback", { not: true }) } }),
      element("text", at(132, 54, 180, 20), at(8, 174, 154, 20, "center"), { source: "album", color: "@text_tertiary" }),
      element("text", at(132, 80, 180, 16), at(8, 200, 154, 16, "center"), { source: "format", size: "small", color: "@text_secondary", scroll: false }),
      element("box", at(10, 128, 300, 3), at(8, 236, 154, 3), { name: "Progress track", color: "@divider" }),
      element("box", at(10, 128, 300, 3), at(8, 236, 154, 3), { name: "Progress", color: "@selection_background", bind: fill("progress_volume") }),
      element("text", at(10, 132, 300, 14), at(8, 242, 154, 14), { source: "elapsed", size: "small", color: "@text_secondary", scroll: false }),
      element("text", at(10, 132, 300, 14, "right"), at(8, 242, 154, 14, "right"), { source: "remaining", size: "small", color: "@text_tertiary", scroll: false }),
    ],
  };
}

const TILE_ICON = 24;

function tiles(spot, frame, columns, [w, h], name) {
  const gap = 4;
  return element("group", ...spot(...frame), { name, repeat: { count: "items", columns, dx: w + gap, dy: h + gap, turn: 0, start: 0, spin: 0 }, children: [
    element("box", ...spot(0, 0, w, h), { name: "Highlight", color: "@text", bind: { show: when("highlighted") } }),
    element("box", ...spot(2, 2, w - 4, h - 4), { name: "Tile", color: "@selection_background" }),
    element("symbol", ...spot(Math.floor((w - TILE_ICON) / 2), Math.floor((h - TILE_ICON) / 2) - 7, TILE_ICON, TILE_ICON), { name: "Icon", mark: "item", color: "@selection_text" }),
    element("text", ...spot(7, h - 23, w - 14, 16), { name: "Name", source: "item", size: "small", color: "@selection_text" }),
  ] });
}

const TILE_FRAMES = { wide: { x: 2, y: 3, w: 316, h: 143 }, tall: { x: 3, y: 4, w: 164, h: 275 } };
const TILE_GAP = 4;
const TILE_TALL = 90;

export function tilesFor(frames = TILE_FRAMES) {
  return ["wide", "tall"].filter((shape) => frames[shape]).map((shape) => {
    const frame = frames[shape];
    const columns = frame.w >= 240 ? 4 : frame.w >= 110 ? 2 : 1;
    const rows = Math.max(1, Math.round(frame.h / TILE_TALL));
    const size = [Math.floor((frame.w - TILE_GAP * (columns - 1)) / columns), Math.floor((frame.h + 1) / rows) - TILE_GAP];
    return tiles(oneWay(shape), [frame.x, frame.y, frame.w, frame.h], columns, size, shape === "wide" ? "Menu tiles, sideways" : "Menu tiles, upright");
  });
}

export function tilesDrawing() {
  return {
    version: 2,
    screen: "home",
    elements: [
      element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Background", locked: true, fill: "wallpaper" }),
      ...tilesFor(),
    ],
  };
}

function degrees(units) {
  return units * UNITS_TO_DEGREES;
}

function meter(c, wide, tall, modes) {
  const ink = c.INK;
  const red = c.RED;
  const pivot = [70, 109];
  const on = (test, x, y, w, h, align) => [wide && test(modes.wide) ? at(x, y, w, h, align) : null, tall && test(modes.tall) ? at(x, y, w, h, align) : null];
  const any = () => true;
  const single = (mode) => (shown) => shown === mode;
  const both = single("both");
  const marks = [
    [233, 93, ink, "20", -53, -91], [243, 93, ink, "10", -31, -102], [247, 91, ink], [250, 93, ink, "5", -15, -106],
    [0, 93, ink, "3", 0, -107], [3, 91, ink], [7, 91, ink], [12, 93, ink, "0", 29, -103], [17, 91, red], [21, 91, red],
    [25, 93, red, "+3", 58, -89],
  ];
  const dial = [
    element("group", ...on(any, 22, -3, 74, 72), { name: "Black arc", clip: true, children: [
      element("circle", ...same(-40, 24, 176, 176), { color: ink, thickness: 2 }),
    ] }),
    element("group", ...on(any, 96, -3, 28, 72), { name: "Red arc", clip: true, children: [
      element("circle", ...same(-117, 21, 182, 182), { color: red, thickness: 5 }),
    ] }),
  ];
  for (const [units, reach, colour, label, across, down] of marks) {
    dial.push(element("line", ...on(any, pivot[0], pivot[1], reach - 88, 0), { name: "Tick", angle: degrees(units), start: 88, thickness: 1.25, color: colour }));
    if (label) {
      dial.push(element("text", ...on(any, pivot[0] + across - 15, pivot[1] + down, 31, 15, "center"), { source: "custom", custom: label, size: "small", color: colour, scroll: false }));
    }
  }
  dial.push(element("text", ...on(any, 0, 35, 140, 18, "center"), { source: "custom", custom: "VU", size: "large", color: ink, scroll: false }));
  const needle = (source, colour, test) => element("line", ...on(test, pivot[0], pivot[1], 95, 0),
    { name: "Needle", angle: degrees(-28), start: 0, thickness: 1.5, color: colour,
      bind: { angle: { source, from: degrees(-28), to: degrees(26) } } });
  const lamp = (source) => ({ name: "Peak lamp", color: c.TRACK, bind: { color: { when: { on: "above", source, level: 74 }, color: c.AMBER } } });
  const letters = [];
  for (const side of ["left", "right"]) {
    const test = single(side);
    if (test(modes.wide) || test(modes.tall)) {
      dial.push(element("circle", ...on(test, 125, 43, 6, 6), lamp(side)), needle(side, ink, test));
      letters.push(element("text", ...on(test, 8, 57, 12, 12), { source: "custom", custom: side === "right" ? "R" : "L", size: "small", color: c.MUTED, scroll: false }));
    }
  }
  if (both(modes.wide) || both(modes.tall)) {
    dial.push(
      element("text", ...on(both, 7, 37, 10, 16), { source: "custom", custom: "L", size: "small", color: ink, scroll: false }),
      element("text", ...on(both, 123, 37, 10, 16, "right"), { source: "custom", custom: "R", size: "small", color: red, scroll: false }),
      needle("left", ink, both),
      needle("right", red, both),
    );
  }
  const children = [
    element("box", ...on(any, 0, 0, 146, 72), { name: "Frame", color: c.BEZEL, radius: 4 }),
    element("box", ...on(any, 3, 3, 140, 54), { name: "Face", fill: "gradient", top: c.FACE_TOP, bottom: c.FACE_BOTTOM }),
    ...letters,
    element("group", ...on(any, 3, 3, 140, 54), { name: "Dial", clip: true, children: dial }),
  ];
  if (both(modes.wide) || both(modes.tall)) {
    children.push(element("circle", ...on(both, 70, 60, 6, 6), lamp("left")));
  }
  return element("group", wide, tall, { name: "VU meter", children });
}

export function vuDrawing(c) {
  return {
    version: 2,
    screen: "playing",
    elements: [
      element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Background", locked: true, color: c.PLATE }),
      meter(c, at(10, 4, 146, 72), at(12, 31, 146, 72), { wide: "left", tall: "both" }),
      meter(c, at(164, 4, 146, 72), null, { wide: "right", tall: null }),
      element("text", at(10, 78, 300, 20, "center"), at(10, 134, 150, 22, "center"), { source: "song", size: "large", color: c.CREAM }),
      element("text", at(10, 97, 300, 20, "center"), at(10, 156, 150, 22, "center"), { name: "Messages", source: "feedback", color: c.CREAM, bind: { show: when("feedback") } }),
      element("text", at(10, 97, 300, 20, "center"), at(10, 156, 150, 22, "center"), { source: "lyric", color: c.MUTED, bind: { show: when("feedback", { not: true }) } }),
      element("text", null, at(10, 178, 150, 22, "center"), { source: "album", color: c.MUTED }),
      element("text", null, at(10, 204, 150, 16, "center"), { source: "format", size: "small", color: c.MUTED, scroll: false }),
      element("text", null, at(10, 238, 150, 16, "center"), { source: "modes", size: "small", color: c.MUTED, scroll: false }),
      element("box", at(84, 130, 170, 3), at(10, 257, 150, 3), { name: "Progress track", color: c.TRACK }),
      element("box", at(84, 130, 170, 3), at(10, 257, 150, 3), { name: "Progress", color: c.AMBER, bind: fill("progress_volume") }),
      element("symbol", at(66, 126, 10, 10), at(80, 265, 10, 10), { mark: "playpause", color: c.MUTED, bind: { color: { when: { on: "playing" }, color: c.AMBER } } }),
      element("text", at(10, 123, 300, 16), at(10, 262, 150, 16), { source: "elapsed", size: "small", color: c.CREAM, scroll: false }),
      element("text", at(10, 123, 300, 16, "right"), at(10, 262, 150, 16, "right"), { source: "remaining", size: "small", color: c.MUTED, scroll: false }),
    ],
  };
}

function sweep(from, units) {
  return { angle: { source: "progress", from: degrees(from), to: degrees(from + units) } };
}

function recordPlayer(c, wide, tall) {
  const ring = (r, thickness, colour, props = {}) => element("circle", ...same(67 - r, 67 - r, 2 * r, 2 * r), { color: colour, thickness, ...props });
  const arm = (start, length, thickness, colour, from, props = {}) => element("line", ...same(52, 22, length, 0),
    { start, thickness, color: colour, angle: degrees(from), bind: sweep(from, 12), ...props });
  return element("group", wide, tall, { name: "Record player", children: [
    element("group", ...same(0, 0, 134, 134), { name: "Record", children: [
      ring(67, 3, c.ALU, { name: "Rim" }),
      ring(64, 0, c.VINYL, { name: "Vinyl" }),
      element("cover", ...same(3, 3, 128, 128), { shape: "circle", placeholder: c.VINYL, bind: { show: when("cover") } }),
      ring(67, 3, c.ALU, { name: "Rim over the cover", bind: { show: when("cover") } }),
      ring(5, 0, c.VINYL, { name: "Hole", bind: { show: when("cover") } }),
      element("group", ...same(0, 0, 134, 134), { name: "Label", bind: { show: when("nocover") }, children: [
        ring(28, 0, c.LABEL ?? "#b02420", { name: "Label" }),
        ring(23, 1, c.CREAM, { name: "Label ring" }),
        ring(46, 1, c.ALU_DARK, { name: "Groove" }),
        ring(53, 1, c.ALU_DARK, { name: "Groove" }),
        ring(60, 1, c.ALU_DARK, { name: "Groove" }),
      ] }),
      element("line", ...same(67, 67, 14, 0), { name: "Turning mark", start: 44, thickness: 2, color: c.ALU, bind: { angle: { source: "spin", speed: 7 } } }),
      ring(3, 0, c.ALU, { name: "Spindle" }),
    ] }),
    element("group", ...same(90, -2, 60, 120), { name: "Tonearm", children: [
      arm(7, 14, 10, c.ALU_DARK, 268, { name: "Counterweight" }),
      element("circle", ...same(42, 12, 20, 20), { name: "Pivot", color: c.ALU_DARK }),
      element("circle", ...same(45, 15, 14, 14), { name: "Pivot", color: c.ALU }),
      arm(0, 72, 4, c.ALU, 140, { name: "Arm" }),
      arm(70, 17, 7, c.ALU, 139, { name: "Headshell" }),
      arm(73, 11, 3, c.VINYL, 139, { name: "Stylus" }),
      element("circle", ...same(49, 19, 6, 6), { name: "Pivot cap", color: c.ALU_DARK }),
    ] }),
    element("circle", ...same(144, 123, 6, 6), { name: "Lamp", color: c.ALU_DARK, bind: { color: { when: { on: "playing" }, color: c.AMBER } } }),
  ] });
}

export function turntableDrawing(c) {
  const right = (wideTop, tallTop, h) => [at(170, wideTop, 140, h), tallTop === null ? null : at(10, tallTop, 150, h, "center")];
  const words = (source, wideTop, tallTop, h, size, colour, props = {}) => element("text", ...right(wideTop, tallTop, h), { source, size, color: colour, ...props });
  return {
    version: 2,
    screen: "playing",
    elements: [
      element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Plinth", fill: "gradient", top: "#482c18", bottom: "#281810" }),
      recordPlayer(c, ...same(10, 6, 153, 135)),
      words("song", 4, 144, 22, "large", c.CREAM, { scroll: true }),
      words("artist", 26, 166, 22, "medium", c.TAN, { scroll: true }),
      words("lyric", 46, 188, 22, "medium", c.CREAM, { scroll: true, bind: { show: when("lyric") } }),
      words("album", 46, 188, 22, "medium", c.TAN_DIM, { scroll: true, bind: { show: when("lyric", { not: true }) } }),
      words("format", 73, 212, 16, "small", c.TAN, { scroll: false }),
      words("detail", 92, null, 16, "small", c.TAN_DIM, { scroll: false }),
      words("feedback", 107, 238, 16, "small", c.CREAM, { name: "Messages", scroll: false, bind: { show: when("feedback") } }),
      words("modes", 107, 238, 16, "small", c.TAN_DIM, { scroll: false, bind: { show: when("feedback", { not: true }) } }),
      element("box", at(170, 124, 140, 3), at(10, 257, 150, 3), { name: "Progress track", color: c.ALU_DARK }),
      element("box", at(170, 124, 140, 3), at(10, 257, 150, 3), { name: "Progress", color: c.CREAM,
        bind: { ...fill("progress_volume"), color: { when: { on: "volume" }, color: c.AMBER } } }),
      element("text", at(170, 129, 140, 16), at(10, 262, 150, 16), { source: "elapsed", size: "small", color: c.TAN, scroll: false }),
      element("text", at(170, 129, 140, 16, "right"), at(10, 262, 150, 16, "right"), { source: "remaining", size: "small", color: c.TAN, scroll: false }),
    ],
  };
}

function oneWay(shape) {
  return (x, y, w, h, align) => shape === "wide" ? [at(x, y, w, h, align), null] : [null, at(x, y, w, h, align)];
}

function screws(spot, size, dx, dy, c) {
  return element("group", ...spot(4, 4, size, size), { name: "Screws", repeat: { count: 4, columns: 2, dx, dy, turn: 0, start: 0, spin: 0 }, children: [
    element("circle", ...spot(0, 0, size, size), { name: "Screw", color: c.RULE }),
    element("box", ...spot(1, Math.floor(size / 2), size - 2, 1), { name: "Slot", color: c.DEEP }),
  ] });
}

function reels(spot, box, reel, c) {
  const { gap, tape, end, hub, hole, tooth, reach } = reel;
  const one = (centre, from, to) => {
    const g = Math.ceil(reach);
    const tip = Math.round(g - reach);
    const round = (side) => spot(centre[0] - Math.floor(side / 2), centre[1] - Math.floor(side / 2), side, side);
    return [
      element("circle", ...round(tape), { name: "Tape", color: c.TAPE,
        bind: { w: { source: "progress", from, to }, h: { source: "progress", from, to } } }),
      element("circle", ...round(hub), { name: "Hub", color: c.LABEL }),
      element("circle", ...round(hole), { name: "Hole", color: c.DEEP }),
      element("group", ...spot(centre[0] - g, centre[1] - g, 2 * g + 1, 2 * g + 1), { name: "Teeth", repeat: { count: 3, columns: 1, dx: 0, dy: 0, turn: 120, start: 0, spin: -8 }, children: [
        element("circle", ...spot(g - Math.floor(tooth / 2), tip - Math.floor(tooth / 2), tooth, tooth), { name: "Tooth", color: c.LABEL }),
      ] }),
    ];
  };
  return element("group", ...spot(...box), { name: "Reels", clip: true, children: [
    ...one(reel.left, 100, end),
    ...one([reel.left[0] + gap, reel.left[1]], end, 100),
  ] });
}

function cassetteLarge(c, spot, x, y, writing = true) {
  const words = writing ? [
    element("text", ...spot(8, 1, 253, 20), { source: "song", size: "large", color: c.INK }),
    element("text", ...spot(8, 24, 253, 20), { name: "Messages", source: "feedback", color: c.INK, scroll: false, bind: { show: when("feedback") } }),
    element("text", ...spot(8, 24, 253, 20), { source: "lyric", color: c.INK_SOFT, bind: { show: when("feedback", { not: true }), color: { when: { on: "lyric" }, color: c.INK } } }),
  ] : [];
  return element("group", ...spot(x, y, 297, 140), { name: "Cassette", children: [
    element("box", ...spot(0, 0, 297, 140), { name: "Shell", color: c.SHELL, radius: 6 }),
    element("box", ...spot(6, 1, 285, 1), { name: "Shine", color: c.SHELL_HI }),
    screws(spot, 7, 282, 125, c),
    element("group", ...spot(14, 5, 269, 92), { name: "Label", children: [
      element("box", ...spot(0, 0, 269, 92), { name: "Paper", color: c.LABEL }),
      element("box", ...spot(8, 22, 253, 1), { name: "Rule", color: c.RULE }),
      element("box", ...spot(8, 45, 253, 1), { name: "Rule", color: c.RULE }),
      element("box", ...spot(0, 48, 269, 4), { name: "Stripe", color: c.AMBER }),
      element("box", ...spot(0, 52, 269, 31), { name: "Band", color: c.RED }),
      element("box", ...spot(0, 83, 269, 4), { name: "Stripe", color: c.AMBER }),
      element("text", ...spot(0, 52, 40, 31, "center"), { source: "custom", custom: "A", size: "large", color: c.LABEL, scroll: false }),
      element("text", ...spot(229, 52, 40, 31, "center"), { source: "codec", size: "small", color: c.LABEL, scroll: false }),
      ...words,
    ] }),
    element("group", ...spot(54, 53, 189, 39), { name: "Window", children: [
      element("box", ...spot(0, 0, 189, 39), { name: "Frame", color: c.SHELL, radius: 5 }),
      element("box", ...spot(2, 2, 185, 35), { name: "Glass", color: c.DEEP, radius: 4 }),
      reels(spot, [2, 2, 185, 35], { left: [32, 17], gap: 120, tape: 53, end: 48, hub: 21, hole: 13, tooth: 4, reach: 5.5 }, c),
      element("symbol", ...spot(86, 10, 18, 18), { mark: "state", color: c.LABEL }),
    ] }),
    element("box", ...spot(60, 102, 177, 38), { name: "Head", color: c.SHELL_HI, taper: 12 }),
    element("circle", ...spot(88, 111, 9, 9), { name: "Guide", color: c.DEEP }),
    element("circle", ...spot(200, 111, 9, 9), { name: "Guide", color: c.DEEP }),
    element("box", ...spot(76, 126, 145, 14), { name: "Slot", color: c.DEEP }),
    element("box", ...spot(76, 130, 145, 3), { name: "Progress track", color: c.TAPE }),
    element("box", ...spot(76, 130, 145, 3), { name: "Progress", color: c.AMBER, bind: { ...fill("progress_volume"), color: { when: { on: "volume" }, color: c.LABEL } } }),
    element("text", ...spot(6, 102, 286, 16), { source: "elapsed", size: "small", color: c.LABEL, scroll: false }),
    element("text", ...spot(6, 102, 286, 16, "right"), { source: "remaining", size: "small", color: c.LABEL, scroll: false }),
  ] });
}

function cassetteSmall(c, spot, x, y) {
  return element("group", ...spot(x, y, 153, 100), { name: "Cassette", children: [
    element("box", ...spot(0, 0, 153, 100), { name: "Shell", color: c.SHELL, radius: 6 }),
    element("box", ...spot(6, 1, 141, 1), { name: "Shine", color: c.SHELL_HI }),
    screws(spot, 5, 140, 86, c),
    element("group", ...spot(12, 5, 129, 63), { name: "Label", children: [
      element("box", ...spot(0, 0, 129, 63), { name: "Paper", color: c.LABEL }),
      element("box", ...spot(5, 23, 119, 1), { name: "Rule", color: c.RULE }),
      element("box", ...spot(0, 26, 129, 3), { name: "Stripe", color: c.AMBER }),
      element("box", ...spot(0, 29, 129, 27), { name: "Band", color: c.RED }),
      element("box", ...spot(0, 56, 129, 3), { name: "Stripe", color: c.AMBER }),
      element("text", ...spot(5, 1, 14, 20), { source: "custom", custom: "A", size: "large", color: c.RED, scroll: false }),
      element("text", ...spot(80, 5, 44, 16, "right"), { source: "codec", size: "small", color: c.INK_SOFT, scroll: false }),
    ] }),
    element("group", ...spot(26, 30, 101, 35), { name: "Window", children: [
      element("box", ...spot(0, 0, 101, 35), { name: "Frame", color: c.SHELL, radius: 5 }),
      element("box", ...spot(2, 2, 97, 31), { name: "Glass", color: c.DEEP, radius: 4 }),
      reels(spot, [2, 2, 97, 31], { left: [22, 15], gap: 52, tape: 39, end: 49, hub: 17, hole: 11, tooth: 3, reach: 4.5 }, c),
      element("symbol", ...spot(42, 8, 18, 18), { mark: "state", color: c.LABEL }),
    ] }),
    element("box", ...spot(26, 72, 101, 28), { name: "Head", color: c.SHELL_HI, taper: 8 }),
    element("circle", ...spot(47, 79, 7, 7), { name: "Guide", color: c.DEEP }),
    element("circle", ...spot(99, 79, 7, 7), { name: "Guide", color: c.DEEP }),
    element("box", ...spot(38, 89, 77, 11), { name: "Slot", color: c.DEEP }),
    element("box", ...spot(38, 92, 77, 3), { name: "Progress track", color: c.TAPE }),
    element("box", ...spot(38, 92, 77, 3), { name: "Progress", color: c.AMBER, bind: { ...fill("progress_volume"), color: { when: { on: "volume" }, color: c.LABEL } } }),
  ] });
}

function cassetteCard(c, spot, x, y) {
  return element("group", ...spot(x, y, 153, 109), { name: "Card", children: [
    element("box", ...spot(0, 0, 153, 109), { name: "Paper", color: c.LABEL }),
    element("box", ...spot(0, 0, 153, 2), { name: "Edge", color: c.RED }),
    element("group", ...spot(8, 24, 137, 1), { name: "Rules", repeat: { count: 3, columns: 1, dx: 0, dy: 23, turn: 0, start: 0, spin: 0 }, children: [
      element("box", ...spot(0, 0, 137, 1), { name: "Rule", color: c.RULE }),
    ] }),
    element("text", ...spot(8, 3, 137, 20, "center"), { source: "song", size: "large", color: c.INK }),
    element("text", ...spot(8, 26, 137, 20, "center"), { source: "artist", color: c.INK_SOFT }),
    element("text", ...spot(8, 49, 137, 20, "center"), { source: "lyric", color: c.INK, bind: { show: when("lyric") } }),
    element("text", ...spot(8, 49, 137, 20, "center"), { source: "album", color: c.INK_SOFT, bind: { show: when("lyric", { not: true }) } }),
    element("text", ...spot(8, 73, 137, 16, "center"), { name: "Messages", source: "feedback", size: "small", color: c.INK, scroll: false, bind: { show: when("feedback") } }),
    element("text", ...spot(8, 73, 137, 16, "center"), { source: "stream", size: "small", color: c.INK_SOFT, scroll: false, bind: { show: when("feedback", { not: true }) } }),
    element("text", ...spot(8, 90, 137, 16), { source: "elapsed", size: "small", color: c.INK, scroll: false }),
    element("text", ...spot(8, 90, 137, 16, "right"), { source: "remaining", size: "small", color: c.INK, scroll: false }),
  ] });
}

export function cassetteDrawing(c) {
  return {
    version: 2,
    screen: "playing",
    elements: [
      element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Background", locked: true, color: c.DEEP }),
      cassetteLarge(c, oneWay("wide"), 11, 3),
      cassetteSmall(c, oneWay("tall"), 8, 31),
      cassetteCard(c, oneWay("tall"), 8, 139),
    ],
  };
}

function lit(colour, on, extra = {}) {
  return { color: { when: { on, ...extra }, color: colour } };
}

function corners(spot, x, y, w, h, colour) {
  return element("group", ...spot(x, y, w, h), { name: "Corners", repeat: { count: 4, columns: 2, dx: w - 1, dy: h - 1, turn: 0, start: 0, spin: 0 }, children: [
    element("box", ...spot(0, 0, 1, 1), { name: "Corner", color: colour }),
  ] });
}

function levelBars(spot, x, bottom, bands, c, cells = 8, badge = true) {
  const rise = 5 * (cells - 1);
  return element("group", ...spot(x, bottom - rise, 9 * bands - 2, rise + 3), { name: "Level bars", bind: badge ? { show: when("badge", { not: true }) } : {},
    repeat: { count: bands, columns: bands, dx: 9, dy: 0, turn: 0, start: 0, spin: 0 }, children: [
      element("group", ...spot(0, rise, 7, 3), { name: "Bar", repeat: { count: cells, columns: 1, dx: 0, dy: -5, turn: 0, start: 0, spin: 0 }, children: [
        element("box", ...spot(0, 0, 7, 3), { name: "Cell", color: c.GHOST, bind: lit(c.INK, "lit", { source: "beat" }) }),
      ] }),
    ] });
}

function progressCells(spot, x, y, count, c) {
  return element("group", ...spot(x, y, 4 * count - 1, 4), { name: "Progress", repeat: { count, columns: count, dx: 4, dy: 0, turn: 0, start: 0, spin: 0 }, children: [
    element("box", ...spot(0, 0, 3, 4), { name: "Cell", color: c.GHOST, bind: lit(c.INK, "lit", { source: "progress_volume" }) }),
  ] });
}

export function lcdDrawing(c) {
  const wide = oneWay("wide");
  const tall = oneWay("tall");
  const word = (custom, w, t, width, on) => element("text", at(w[0], w[1], width, 15), at(t[0], t[1], width, 15),
    { source: "custom", custom, size: "small", color: c.GHOST, scroll: false, bind: lit(c.INK, on) });
  return {
    version: 2,
    screen: "playing",
    elements: [
      element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), { name: "Bezel", color: c.BEZEL }),
      element("box", at(5, 3, 310, 140), at(4, 3, 162, 274), { name: "Glass", color: c.GLASS }),
      element("box", at(5, 3, 310, 2), at(4, 3, 162, 2), { name: "Shadow", color: c.SHADOW }),
      corners(wide, 5, 3, 310, 140, c.BEZEL),
      corners(tall, 4, 3, 162, 274, c.BEZEL),
      element("symbol", at(12, 10, 8, 9), at(10, 10, 8, 9), { mark: "play", color: c.GHOST, bind: lit(c.INK, "playing") }),
      element("symbol", at(25, 10, 8, 9), at(23, 10, 8, 9), { mark: "pause", color: c.GHOST, bind: lit(c.INK, "paused") }),
      word("SHUFFLE", [41, 7], [40, 7], 56, "shuffle"),
      word("ALBUM", [101, 7], [100, 7], 50, "shuffle_album"),
      word("REPEAT", [156, 7], [10, 24], 50, "repeat"),
      word("1", [208, 7], [62, 24], 10, "repeat_one"),
      word("ALL", [219, 7], [73, 24], 30, "repeat_all"),
      element("text", at(240, 7, 68, 15, "right"), at(100, 24, 60, 15, "right"), { source: "codecbox", size: "small", color: c.INK, scroll: false }),
      element("text", at(12, 26, 50, 15), at(37, 58, 50, 15), { source: "custom", custom: "TRACK", size: "small", color: c.INK, scroll: false }),
      element("number", at(12, 42, 46, 24), at(85, 47, 46, 24), { show: "track", lit: c.INK, dim: c.GHOST, lean: 8 }),
      element("number", at(68, 26, 134, 40), at(16, 79, 134, 40), { show: "time_volume", lit: c.INK, dim: c.GHOST, lean: 8 }),
      element("symbol", at(255, 38, 18, 18), at(76, 137, 18, 18), { mark: "badge", color: c.INK, bind: { show: when("badge") } }),
      levelBars(wide, 220, 63, 10, c),
      levelBars(tall, 14, 163, 16, c),
      element("box", at(12, 71, 296, 20), at(10, 174, 150, 20), { name: "Title glass", color: c.GHOST, grid: 3, gridColor: c.GLASS }),
      element("text", at(12, 71, 296, 20), at(10, 174, 150, 20, "center"), { source: "song", size: "large", color: c.INK }),
      element("text", ...wide(12, 93, 296, 20), { source: "lyric", color: c.INK }),
      element("text", ...tall(10, 197, 150, 20, "center"), { source: "artist", color: c.INK }),
      element("text", ...tall(10, 217, 150, 20, "center"), { source: "lyric", color: c.INK, bind: { show: when("lyric") } }),
      element("text", ...tall(10, 217, 150, 20, "center"), { source: "album", color: c.INK, bind: { show: when("lyric", { not: true }) } }),
      progressCells(wide, 12, 118, 74, c),
      progressCells(tall, 11, 244, 37, c),
      element("text", at(12, 125, 296, 15), at(10, 251, 150, 15, "center"), { name: "Messages", source: "feedback", size: "small", color: c.INK, scroll: false, bind: { show: when("feedback") } }),
      element("text", at(12, 125, 296, 15, "right"), at(10, 251, 150, 15, "right"), { source: "remaining", size: "small", color: c.INK, scroll: false, bind: { show: when("feedback", { not: true }) } }),
      element("text", at(12, 125, 240, 15), at(10, 251, 100, 15), { source: "stream", size: "small", color: c.INK, scroll: false, bind: { show: when("feedback", { not: true }) } }),
    ],
  };
}

function frameOf(layer, shape) {
  const f = layer.frames?.[shape];
  return f ? at(Math.round(f.x), Math.round(f.y), Math.round(f.w), Math.round(f.h)) : null;
}

function framesOf(layer) {
  return [frameOf(layer, "wide"), frameOf(layer, "tall")];
}

function aligned(layer, frames) {
  const [wide, tall] = frames;
  if (wide) {
    wide.align = layer.align ?? "left";
  }
  if (tall) {
    tall.align = layer.tallAlign ?? layer.align ?? "left";
  }
  return [wide, tall];
}

function perShape(frames, build) {
  const [wide, tall] = frames;
  if (!wide || !tall || (wide.w === tall.w && wide.h === tall.h)) {
    return [build(wide, tall)];
  }
  return [build(wide, null), build(null, tall)];
}

function converted(layer) {
  const frames = framesOf(layer);
  const sizes = { large: "large", medium: "medium", small: "small" };
  switch (layer.type) {
    case "cover":
      return [element("cover", ...frames, { shape: layer.shape === "circle" ? "circle" : "square", placeholder: layer.placeholder ?? "@divider", reflection: Boolean(layer.reflection) })];
    case "text": {
      const text = { source: layer.source === "item" ? "item" : layer.source, custom: layer.custom ?? "", size: sizes[layer.size] ?? "medium", color: layer.color, scroll: Boolean(layer.scroll) };
      const made = element("text", ...aligned(layer, frames), text);
      if (!layer.feedback) {
        return [made];
      }
      made.bind = { show: when("feedback", { not: true }) };
      return [element("text", ...aligned(layer, framesOf(layer)), { ...text, name: "Messages", source: "feedback", bind: { show: when("feedback") } }), made];
    }
    case "times":
      return [
        element("text", ...frames, { source: "elapsed", size: sizes[layer.size] ?? "small", color: layer.color, scroll: false }),
        element("text", ...framesOf(layer).map((f) => f && { ...f, align: "right" }), { source: "remaining", size: sizes[layer.size] ?? "small", color: layer.color2 ?? layer.color, scroll: false }),
      ];
    case "progress": {
      if (layer.style === "ring") {
        const length = Math.max(1, Math.round(layer.thickness ?? 6));
        const ring = (f) => f && at(f.x, f.y, f.w, f.h);
        const half = (f) => f && Math.max(1, Math.floor(Math.min(f.w, f.h) / 2));
        return perShape(frames, (wide, tall) => element("group", ring(wide), ring(tall), { name: "Ring of lights", repeat: { count: LIGHTS, columns: 1, dx: 0, dy: 0, turn: 360 / LIGHTS }, children: [
          element("line", wide && at(Math.floor(wide.w / 2), Math.floor(wide.h / 2), length, 0), tall && at(Math.floor(tall.w / 2), Math.floor(tall.h / 2), length, 0),
            { name: "Light", start: Math.max(0, (half(wide) ?? half(tall)) - length), thickness: 2, color: layer.track ?? "@divider",
              bind: { color: { when: { on: "lit", source: "progress" }, color: layer.color } } }),
        ] }));
      }
      const track = element("box", ...frames, { name: "Progress track", color: layer.track ?? "@divider" });
      if (layer.style === "dots") {
        const count = (f) => f && Math.max(1, Math.floor((f.w + 1) / 4));
        return perShape(frames, (wide, tall) => element("group", wide, tall, { name: "Dots", repeat: { count: count(wide) ?? count(tall), columns: 999, dx: 4, dy: 0 }, children: [
          element("box", wide && at(0, 0, 3, wide.h), tall && at(0, 0, 3, tall.h), { name: "Dot", color: layer.track ?? "@divider",
            bind: { color: { when: { on: "lit", source: "progress_volume" }, color: layer.color } } }),
        ] }));
      }
      return [track, element("box", ...framesOf(layer), { name: "Progress", color: layer.color, bind: fill("progress_volume") })];
    }
    case "shape": {
      if (layer.kind === "circle" || layer.kind === "ring") {
        return [element("circle", ...frames, { color: layer.color, thickness: layer.kind === "ring" ? Math.max(1, Math.round(layer.thickness ?? 2)) : 0 })];
      }
      if (layer.kind === "line") {
        const thick = Math.max(1, Math.round(layer.thickness ?? 2));
        const flat = (f) => f && at(f.x, f.y + Math.floor((f.h - thick) / 2), f.w, thick);
        return [element("box", flat(frames[0]), flat(frames[1]), { color: layer.color })];
      }
      return [element("box", ...frames, { color: layer.color, radius: layer.kind === "rounded" ? Math.round(layer.radius ?? 6) : 0 })];
    }
    case "playmark":
      return [element("symbol", ...frames, { mark: "state", color: layer.color })];
    case "spinner": {
      const pivot = (f) => f && at(Math.round(f.x + f.w / 2), Math.round(f.y + f.h / 2), Math.max(1, Math.round(layer.length ?? 4)), 0);
      const radius = Math.floor(Math.min(...frames.filter(Boolean).map((f) => Math.min(f.w, f.h))) / 2);
      return [element("line", pivot(frames[0]), pivot(frames[1]), { name: "Turning mark", start: Math.max(0, radius - Math.round(layer.length ?? 4)), thickness: 2, color: layer.color, bind: { angle: { source: "spin", speed: Math.round(layer.speed ?? 3) } } })];
    }
    case "menu":
      return perShape(frames, (wide, tall) => menuOf(layer, [wide, tall]));
    default:
      return partOf(layer, frames);
  }
}

function menuOf(layer, frames) {
  if (layer.style !== "tiles") {
    return element("menu", ...frames, { name: "Menu" });
  }
  const columns = Math.max(1, Math.round(layer.columns ?? 2));
  const gap = Math.max(0, Math.round(layer.gap ?? 6));
  const tileHeight = Math.max(8, Math.round(layer.tileHeight ?? 40));
  const tile = (f) => f && at(0, 0, Math.max(1, Math.floor((f.w - gap * (columns - 1)) / columns)), tileHeight);
  const label = (f) => f && at(4, 0, Math.max(1, Math.floor((f.w - gap * (columns - 1)) / columns) - 8), tileHeight, layer.align ?? "center");
  const [wide, tall] = frames;
  const step = (f) => f ? Math.floor((f.w - gap * (columns - 1)) / columns) + gap : 0;
  return element("group", ...frames, { name: "Menu tiles", repeat: { count: "items", columns, dx: step(wide ?? tall), dy: tileHeight + gap }, children: [
    element("box", tile(wide), tile(tall), { name: "Tile", color: layer.tile ?? "@header_top", radius: Math.round(layer.radius ?? 8),
      bind: { color: { when: { on: "highlighted" }, color: layer.tileSelected ?? "@selection_background" } } }),
    element("text", label(wide), label(tall), { source: "item", size: layer.size ?? "medium", color: layer.color ?? "@text", scroll: true,
      bind: { color: { when: { on: "highlighted" }, color: layer.colorSelected ?? "@selection_text" } } }),
  ] });
}

export function fromDesign(design, screen) {
  const background = design.background ?? {};
  const fillOf = background.style === "wallpaper" ? "wallpaper" : background.style === "gradient" ? "gradient" : "solid";
  const elements = [element("box", at(0, 0, 320, 146), at(0, 0, 170, 280), {
    name: "Background", locked: true, fill: fillOf, color: background.color ?? "@background", top: background.top ?? "@header_background", bottom: background.bottom ?? "@background",
  })];
  for (const layer of design.layers ?? []) {
    if (!layer.hidden) {
      elements.push(...converted(layer));
    }
  }
  return { version: 2, screen: design.screen ?? screen, elements };
}

const PART_DEFAULTS = {
  vu: { frame: "#080808", faceTop: "#f8ecc0", faceBottom: "#f0c878", ink: "#281c10", red: "#a01810", label: "#a89880", lamp: "#f8a830", lampOff: "#484038" },
  cassette: {
    shell: "#484850", shellHi: "#606068", deep: "#181418", tape: "#784c30", paper: "#f0e4c8", ink: "#282020",
    inkSoft: "#605048", red: "#b03020", amber: "#e89828", rule: "#b8a888",
  },
  turntable: { vinyl: "#101018", metal: "#c8c8c0", metalDark: "#686860", label: "#b02420", ring: "#f8ecd0", lamp: "#f8a428" },
  clock: { lit: "#182410", dim: "#90a850" },
  tracknumber: { lit: "#182410", dim: "#90a850" },
  levels: { lit: "#182410", dim: "#90a850" },
};
const LEVEL_CELLS_MOST = 16;

function partColours(layer) {
  const p = { ...PART_DEFAULTS[layer.type], ...(layer.colours ?? {}) };
  switch (layer.type) {
    case "vu":
      return { BEZEL: p.frame, FACE_TOP: p.faceTop, FACE_BOTTOM: p.faceBottom, INK: p.ink, RED: p.red, MUTED: p.label, AMBER: p.lamp, TRACK: p.lampOff };
    case "cassette":
      return { SHELL: p.shell, SHELL_HI: p.shellHi, DEEP: p.deep, TAPE: p.tape, LABEL: p.paper, INK: p.ink, INK_SOFT: p.inkSoft, RED: p.red, AMBER: p.amber, RULE: p.rule };
    case "turntable":
      return { VINYL: p.vinyl, ALU: p.metal, ALU_DARK: p.metalDark, LABEL: p.label, CREAM: p.ring, AMBER: p.lamp };
    default:
      return { INK: p.lit, GHOST: p.dim };
  }
}

function sized(frame, w, h) {
  return frame ? at(frame.x, frame.y, w, h) : null;
}

function partOf(layer, frames) {
  const c = partColours(layer);
  const [wide, tall] = frames;
  switch (layer.type) {
    case "vu":
      return [meter(c, sized(wide, 146, 72), sized(tall, 146, 72), {
        wide: wide ? layer.channel ?? "left" : null,
        tall: tall ? layer.tallChannel ?? layer.channel ?? "left" : null,
      })];
    case "turntable":
      return [recordPlayer(c, sized(wide, 153, 135), sized(tall, 153, 135))];
    case "cassette":
      return [["wide", wide], ["tall", tall]].filter(([, frame]) => frame).map(([shape, frame]) => {
        const size = shape === "tall" ? layer.tallSize ?? layer.size ?? "small" : layer.size ?? "large";
        return size === "small"
          ? cassetteSmall(c, oneWay(shape), frame.x, frame.y)
          : cassetteLarge(c, oneWay(shape), frame.x, frame.y, layer.writing !== false);
      });
    case "clock":
      return [element("number", sized(wide, 134, 40), sized(tall, 134, 40), { name: "Clock", show: "time_volume", lit: c.INK, dim: c.GHOST, lean: 8 })];
    case "tracknumber":
      return [element("number", sized(wide, 45, 24), sized(tall, 45, 24), { name: "Track number", show: "track", lit: c.INK, dim: c.GHOST, lean: 8 })];
    case "levels":
      return [["wide", wide], ["tall", tall]].filter(([, frame]) => frame).map(([shape, frame]) => {
        const bands = Math.max(1, Math.floor((frame.w + 2) / 9));
        const cells = Math.max(1, Math.min(LEVEL_CELLS_MOST, Math.floor((frame.h + 2) / 5)));
        return levelBars(oneWay(shape), frame.x, frame.y + frame.h - 3, bands, c, cells, false);
      });
    default:
      return [];
  }
}

const FACE_COLOURS = {
  cassette: {
    DEEP: "#181418", SHELL: "#484850", SHELL_HI: "#606068", TAPE: "#784c30", LABEL: "#f0e4c8",
    INK: "#282020", INK_SOFT: "#605048", RED: "#b03020", AMBER: "#e89828", RULE: "#b8a888",
  },
  lcd: { INK: "#182410", GLASS: "#b0c468", GHOST: "#90a850", SHADOW: "#889c48", BEZEL: "#202028" },
  turntable: { VINYL: "#101018", ALU: "#c8c8c0", ALU_DARK: "#686860", CREAM: "#f8ecd0", TAN: "#d8b888", TAN_DIM: "#c0a078", AMBER: "#f8a428" },
  vu: {
    PLATE: "#201c18", BEZEL: "#080808", FACE_TOP: "#f8ecc0", FACE_BOTTOM: "#f0c878", INK: "#281c10",
    RED: "#a01810", CREAM: "#f0e8d8", MUTED: "#a89880", AMBER: "#f8a830", TRACK: "#484038",
  },
};
const FACE_MARKS = { vu: ["PLATE", "FACE_TOP"], cassette: ["SHELL", "TAPE"], turntable: ["VINYL", "ALU"], lcd: ["GLASS", "GHOST"] };
const MAKERS = { vu: vuDrawing, cassette: cassetteDrawing, turntable: turntableDrawing, lcd: lcdDrawing };

export const STARTS = {
  playing: [["standard", "Plain"], ["vu", "VU meters"], ["cassette", "Cassette"], ["turntable", "Record player"], ["lcd", "LCD deck"], ["empty", "Empty"]],
  home: [["standard", "Plain"], ["tiles", "Tiles"], ["empty", "Empty"]],
};

export function faceOf(consts) {
  return Object.keys(FACE_MARKS).find((face) => FACE_MARKS[face].every((name) => name in consts)) ?? null;
}

export function startDrawing(name, screen, consts = {}) {
  if (MAKERS[name]) {
    return MAKERS[name]({ ...FACE_COLOURS[name], ...consts });
  }
  if (name === "tiles" && screen === "home") {
    return tilesDrawing();
  }
  return name === "empty" ? emptyDrawing(screen) : standardDrawing(screen);
}

export function drawingFrom(text, screen) {
  const drawing = readDrawing(text);
  if (drawing) {
    return drawing;
  }
  try {
    const design = JSON.parse(text);
    return Array.isArray(design?.layers) ? fromDesign(design, screen) : null;
  } catch {
    return null;
  }
}

const THEMED = { GHOST: "@divider", INK: "@text" };

function lightWord(custom, x, w, on) {
  return element("text", ...same(x, 0, w, 15), { source: "custom", custom, size: "small", color: THEMED.GHOST, scroll: false, bind: lit(THEMED.INK, on) });
}

export const READY = {
  song: ["Song, artist and album", () => element("group", ...same(10, 8, 150, 64), { name: "Song details", children: [
    element("text", ...same(0, 0, 150, 22), { source: "song", size: "large", color: "@text" }),
    element("text", ...same(0, 22, 150, 20), { source: "artist", color: "@text_secondary" }),
    element("text", ...same(0, 44, 150, 20), { source: "album", color: "@text_tertiary" }),
  ] })],
  progress: ["Progress bar and times", () => element("group", ...same(10, 110, 150, 20), { name: "Progress and times", children: [
    element("box", ...same(0, 0, 150, 3), { name: "Progress track", color: "@divider" }),
    element("box", ...same(0, 0, 150, 3), { name: "Progress", color: "@selection_background", bind: fill("progress_volume") }),
    element("text", ...same(0, 5, 150, 14), { source: "elapsed", size: "small", color: "@text_secondary", scroll: false }),
    element("text", ...same(0, 5, 150, 14, "right"), { source: "remaining", size: "small", color: "@text_tertiary", scroll: false }),
  ] })],
  play: ["Play button", () => element("group", ...same(10, 10, 28, 28), { name: "Play button", children: [
    element("circle", ...same(0, 0, 28, 28), { color: "@selection_background" }),
    element("symbol", ...same(9, 9, 10, 10), { mark: "playpause", color: "@selection_text" }),
  ] })],
  clock: ["Big time and volume", () => element("number", ...same(10, 10, 134, 40), { name: "Big time", show: "time_volume", lit: "@text", dim: "@divider", lean: 8 })],
  modes: ["Shuffle and repeat lights", () => element("group", ...same(10, 10, 150, 15), { name: "Shuffle and repeat", children: [
    lightWord("SHUFFLE", 0, 52, "shuffle"),
    lightWord("REPEAT", 58, 46, "repeat"),
    lightWord("1", 108, 8, "repeat_one"),
    lightWord("ALL", 120, 30, "repeat_all"),
  ] })],
  levels: ["Level bars", () => levelBars(same, 10, 48, 10, { GHOST: "@divider", INK: "@selection_background" }, 8, false)],
  vu: ["VU meter", () => meter(FACE_COLOURS.vu, ...same(10, 4, 146, 72), { wide: "both", tall: "both" })],
  record: ["Record player", () => recordPlayer(FACE_COLOURS.turntable, ...same(10, 6, 153, 135))],
  cassette: ["Cassette", () => cassetteSmall(FACE_COLOURS.cassette, same, 8, 8)],
  tiles: ["Menu tiles with icons", () => tiles(same, [0, 0, 160, 143], 2, [78, 68], "Menu tiles"), "home"],
};

export const SHELF = [
  ["title", "Song title", () => element("text", ...same(0, 0, 180, 22), { source: "song", size: "large", color: "@text" })],
  ["artist", "Artist", () => element("text", ...same(0, 0, 180, 20), { source: "artist", color: "@text_secondary" })],
  ["album", "Album", () => element("text", ...same(0, 0, 180, 20), { source: "album", color: "@text_tertiary" })],
  ["cover", "Album cover", () => element("cover", ...same(0, 0, 96, 96), { placeholder: "@divider" })],
  ["progress", "Progress bar and times", READY.progress[1]],
  ["play", "Play button", READY.play[1]],
  ["clock", "Big time", READY.clock[1]],
  ["menu", "Menu as a list", () => element("menu", at(0, 0, 176, 146), at(0, 0, 170, 152), { name: "Menu" }), "home"],
  ["tiles", "Menu as tiles", () => tilesFor(), "home"],
  ["words", "My own words", () => element("text", ...same(0, 0, 120, 20), { source: "custom", custom: "Hello", color: "@text" })],
  ["box", "Box", () => newElement("box")],
  ["circle", "Circle or ring", () => newElement("circle")],
  ["line", "Line", () => newElement("line")],
];

export const SHELF_MORE = [
  ["lyrics", "Lyrics", () => element("text", ...same(0, 0, 180, 20), { source: "lyric", color: "@text_secondary" })],
  ["quality", "Audio quality", () => element("text", ...same(0, 0, 180, 16), { source: "format", size: "small", color: "@text_secondary", scroll: false })],
  ["mark", "Play mark", () => newElement("symbol")],
  ["number", "Number", () => newElement("number")],
  ["group", "Group", () => newElement("group")],
  ["modes", READY.modes[0], READY.modes[1]],
  ["levels", READY.levels[0], READY.levels[1]],
  ["vu", READY.vu[0], READY.vu[1]],
  ["record", READY.record[0], READY.record[1]],
  ["cassette", READY.cassette[0], READY.cassette[1]],
];

export const SHELF_ART = {
  title: '<rect x="6" y="9" width="32" height="6" rx="1.5" fill="currentColor"/>',
  artist: '<rect x="6" y="10.5" width="24" height="4" rx="1" fill="currentColor" opacity=".6"/>',
  album: '<rect x="6" y="10.5" width="28" height="4" rx="1" fill="currentColor" opacity=".4"/>',
  cover: '<rect x="12" y="3" width="20" height="20" rx="2" fill="currentColor" opacity=".3"/><circle cx="22" cy="13" r="5.5" fill="currentColor"/>',
  progress: '<rect x="4" y="9" width="36" height="3" rx="1.5" fill="currentColor" opacity=".3"/><rect x="4" y="9" width="15" height="3" rx="1.5" fill="currentColor"/><rect x="4" y="15" width="8" height="2.5" fill="currentColor" opacity=".55"/><rect x="32" y="15" width="8" height="2.5" fill="currentColor" opacity=".55"/>',
  play: '<circle cx="22" cy="13" r="10" fill="currentColor" opacity=".25"/><path d="M19 8v10l8.5-5z" fill="currentColor"/>',
  clock: '<path d="M7 6h6v14H7zM17 6h6v7h-6zM17 13v7h6M27.5 10v.2M27.5 16v.2M32 6h5v14h-5z" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/>',
  menu: '<rect x="4" y="4" width="36" height="5.5" rx="1" fill="currentColor"/><rect x="4" y="11.5" width="24" height="4" rx="1" fill="currentColor" opacity=".4"/><rect x="4" y="17.5" width="29" height="4" rx="1" fill="currentColor" opacity=".4"/>',
  tiles: '<rect x="4" y="5" width="10" height="16" rx="1.5" fill="currentColor"/><rect x="17" y="5" width="10" height="16" rx="1.5" fill="currentColor" opacity=".55"/><rect x="30" y="5" width="10" height="16" rx="1.5" fill="currentColor" opacity=".55"/>',
  words: '<path d="M14 21 22 5l8 16M17 16h10" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round" stroke-linecap="round"/>',
  box: '<rect x="8" y="6" width="28" height="14" rx="2" fill="none" stroke="currentColor" stroke-width="2"/>',
  circle: '<circle cx="22" cy="13" r="8.5" fill="none" stroke="currentColor" stroke-width="2"/>',
  line: '<path d="M9 20 35 6" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/>',
};
