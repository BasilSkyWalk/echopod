export const PART_TYPES = {
  vu: "VU meter",
  cassette: "Cassette",
  turntable: "Record player",
  clock: "Segment clock",
  tracknumber: "Track number",
  levels: "Level bars",
};

export const PART_COLOURS = {
  vu: [
    ["frame", "Frame", "#080808"], ["faceTop", "Face, top", "#f8ecc0"], ["faceBottom", "Face, bottom", "#f0c878"],
    ["ink", "Markings and needle", "#281c10"], ["red", "Red zone", "#a01810"], ["label", "L and R", "#a89880"],
    ["lamp", "Peak lamp", "#f8a830"], ["lampOff", "Peak lamp, off", "#484038"],
  ],
  cassette: [
    ["shell", "Shell", "#484850"], ["shellHi", "Shell edge", "#606068"], ["deep", "Window and holes", "#181418"],
    ["tape", "Tape", "#784c30"], ["paper", "Label", "#f0e4c8"], ["ink", "Writing", "#282020"],
    ["inkSoft", "Second line", "#605048"], ["red", "Stripe", "#b03020"], ["amber", "Stripe edges", "#e89828"],
    ["rule", "Lines and screws", "#b8a888"],
  ],
  turntable: [
    ["vinyl", "Record", "#101018"], ["metal", "Metal", "#c8c8c0"], ["metalDark", "Dark metal", "#686860"],
    ["label", "Label, no cover", "#b02420"], ["ring", "Label ring", "#f8ecd0"], ["lamp", "Lamp", "#f8a428"],
  ],
  clock: [["lit", "Lit", "#182410"], ["dim", "Unlit", "#90a850"]],
  tracknumber: [["lit", "Lit", "#182410"], ["dim", "Unlit", "#90a850"]],
  levels: [["lit", "Lit", "#182410"], ["dim", "Unlit", "#90a850"]],
};

const FIXED = {
  vu: { w: 146, h: 72 },
  turntable: { w: 153, h: 135 },
  clock: { w: 134, h: 40 },
  tracknumber: { w: 45, h: 24 },
};
const CASSETTES = { large: { w: 297, h: 140 }, small: { w: 153, h: 100 } };
const LEVEL_CELLS_MOST = 16;

export function isPart(type) {
  return type in PART_TYPES;
}

export function cassetteSize(item, shape) {
  return shape === "tall" ? item.tallSize ?? item.size ?? "small" : item.size ?? "large";
}

export function partSize(item, shape = "wide") {
  if (item.type === "cassette") {
    return CASSETTES[cassetteSize(item, shape)] ?? CASSETTES.large;
  }
  return FIXED[item.type] ?? null;
}

export function partProps(type) {
  const colours = Object.fromEntries(PART_COLOURS[type].map(([key, , value]) => [key, value]));
  const extra = {
    vu: { channel: "left", tallChannel: "left" },
    cassette: { size: "large", tallSize: "small", writing: true },
  }[type] ?? {};
  return { colours, ...extra };
}

function painter(item, colorOf, theme) {
  const defaults = Object.fromEntries(PART_COLOURS[item.type].map(([key, , value]) => [key, value]));
  return (key) => colorOf(item.colours?.[key] ?? defaults[key], theme);
}

function vuLines(item, place, c, shape) {
  const channel = shape === "tall" ? item.tallChannel ?? item.channel ?? "left" : item.channel ?? "left";
  const ink = c("ink");
  const red = c("red");
  const lines = [
    `let ox = ${place.x}`,
    `let oy = ${place.y}`,
    "let n0 = 0",
    "let n1 = 0",
    "if playing {",
    "    repeat ch from 0 to 2 {",
    "        let sum = 7680 + 11 * sin(tick * (ch == 0 ? 3 : 4) + (ch == 0 ? 0 : 96))",
    "        sum = sum + 7 * sin(tick * (ch == 0 ? 7 : 6) + (ch == 0 ? 64 : 160))",
    "        sum = sum + 3 * sin(tick * (ch == 0 ? 17 : 13) + (ch == 0 ? 0 : 48))",
    "        let t = tick + 29 * ch",
    "        let kick = ((t >>> 1) % 53 * 2 + (t & 1)) % 53 < 2 ? 10 : 0",
    "        let v = min(sum / 256 + kick, 62)",
    "        if ch == 0 {",
    "            n0 = v",
    "        } else {",
    "            n1 = v",
    "        }",
    "    }",
    "}",
    "let px = ox + 73",
    "let py = oy + 112",
    "let cx = px * 16 + 8",
    "let cy = py * 16 + 8",
    `rounded(ox, oy, 146, 72, 4, ${c("frame")})`,
    `gradient(ox + 3, oy + 3, 140, 54, ${c("faceTop")}, ${c("faceBottom")})`,
    "clip(ox + 3, oy + 3, 140, 54)",
    "clip(px - 48, oy, 74, 72)",
    `ring(cx, cy, 1408, 1376, ${ink})`,
    "unclip()",
    "clip(px + 26, oy, 28, 72)",
    `ring(cx, cy, 1456, 1376, ${red})`,
    "unclip()",
  ];
  const marks = [
    [233, 93, ink, "20", -53, -91], [243, 93, ink, "10", -31, -102], [247, 91, ink], [250, 93, ink, "5", -15, -106],
    [0, 93, ink, "3", 0, -107], [3, 91, ink], [7, 91, ink], [12, 93, ink, "0", 29, -103], [17, 91, red], [21, 91, red],
    [25, 93, red, "+3", 58, -89],
  ];
  lines.push("let w = 0");
  for (const [angle, reach, colour, label, across, down] of marks) {
    lines.push(`spoke(px, py, ${angle}, 20, 88, ${reach}, ${colour})`);
    if (label) {
      lines.push(`w = width("${label}", small)`, `plain("${label}", small, px + ${across} - w / 2, py - ${-down}, w, 15, ${colour})`);
    }
  }
  lines.push(`line("VU", title, ox + 3, oy + 38, 140, 18, center, ${ink})`);
  if (channel === "both") {
    lines.push(
      `line("L", small, ox + 10, oy + 40, 10, 16, left, ${ink})`,
      `line("R", small, ox + 126, oy + 40, 10, 16, right, ${red})`,
      "unclip()",
      `ring(cx, (oy + 63) * 16 + 8, 48, 0, n0 >= 46 || n1 >= 46 ? ${c("lamp")} : ${c("lampOff")})`,
      "clip(ox + 3, oy + 3, 140, 54)",
      `spoke(px, py, (n0 * 7 / 8 - 28) & 255, 24, 0, 95, ${ink})`,
      `spoke(px, py, (n1 * 7 / 8 - 28) & 255, 24, 0, 95, ${red})`,
    );
  } else {
    const value = channel === "right" ? "n1" : "n0";
    lines.push(
      "unclip()",
      `line("${channel === "right" ? "R" : "L"}", small, ox + 8, oy + 57, 12, 12, left, ${c("label")})`,
      "clip(ox + 3, oy + 3, 140, 54)",
      `ring((ox + 131) * 16 + 8, (oy + 49) * 16 + 8, 48, 0, ${value} >= 46 ? ${c("lamp")} : ${c("lampOff")})`,
      `spoke(px, py, (${value} * 7 / 8 - 28) & 255, 24, 0, 95, ${ink})`,
    );
  }
  lines.push("unclip()");
  return lines;
}

function reelLines(c, geometry) {
  const { ox, oy, left, right, middle, tape, hub, hole, spoke, dot, clipBox } = geometry;
  return [
    `let played = progress(${tape[0]} * ${tape[0]} - ${tape[1]} * ${tape[1]})`,
    `let left_pack = isqrt(${tape[0]} * ${tape[0]} - played) & -16`,
    `let right_pack = isqrt(${tape[1]} * ${tape[1]} + played) & -16`,
    `clip(${clipBox})`,
    "repeat reel from 0 to 2 {",
    `    let cx = (reel == 0 ? ${ox} + ${left} : ${ox} + ${right}) * 16 + 8`,
    `    let cy = (${oy} + ${middle}) * 16 + 8`,
    `    ring(cx, cy, reel == 0 ? left_pack : right_pack, 0, ${c("tape")})`,
    `    ring(cx, cy, ${hub}, 0, ${c("paper")})`,
    `    ring(cx, cy, ${hole}, 0, ${c("deep")})`,
    "    let a = turn",
    `    ring(cx + ${spoke} * sin(a) / 255, cy - ${spoke} * cos(a) / 255, ${dot}, 0, ${c("paper")})`,
    "    a = (turn + 85) & 255",
    `    ring(cx + ${spoke} * sin(a) / 255, cy - ${spoke} * cos(a) / 255, ${dot}, 0, ${c("paper")})`,
    "    a = (turn + 171) & 255",
    `    ring(cx + ${spoke} * sin(a) / 255, cy - ${spoke} * cos(a) / 255, ${dot}, 0, ${c("paper")})`,
    "}",
    "unclip()",
  ];
}

function screwLines(c, places, size) {
  return places.flatMap(([across, down]) => [
    `ring((ox + ${across}) * 16 + 8, (oy + ${down}) * 16 + 8, ${size * 16 + 8}, 0, ${c("rule")})`,
    `rect(ox + ${across - (size === 3 ? 2 : 1)}, oy + ${down}, ${size === 3 ? 5 : 3}, 1, ${c("deep")})`,
  ]);
}

function largeCassette(item, c) {
  const lines = [
    `rounded(ox, oy, 297, 140, 6, ${c("shell")})`,
    `rect(ox + 6, oy + 1, 285, 1, ${c("shellHi")})`,
    ...screwLines(c, [[7, 7], [289, 7], [7, 132], [289, 132]], 3),
    `rect(ox + 14, oy + 5, 269, 92, ${c("paper")})`,
    `rect(ox + 22, oy + 27, 253, 1, ${c("rule")})`,
    `rect(ox + 22, oy + 50, 253, 1, ${c("rule")})`,
    `rect(ox + 14, oy + 53, 269, 4, ${c("amber")})`,
    `rect(ox + 14, oy + 57, 269, 31, ${c("red")})`,
    `rect(ox + 14, oy + 88, 269, 4, ${c("amber")})`,
    `text("A", title, ox + 14, oy + 57, 40, 31, center, ${c("paper")})`,
    `text(codec, small, ox + 243, oy + 57, 40, 31, center, ${c("paper")})`,
    `rounded(ox + 54, oy + 53, 189, 39, 5, ${c("shell")})`,
    `rounded(ox + 56, oy + 55, 185, 35, 4, ${c("deep")})`,
    ...reelLines(c, { ox: "ox", oy: "oy", left: 88, right: 208, middle: 72, tape: [424, 200], hub: 168, hole: 104,
      spoke: 88, dot: 32, clipBox: "ox + 56, oy + 55, 185, 35" }),
    `icon(icon_kind, ox + 140, oy + 63, 18, 18, ${c("paper")})`,
    `trapezoid(ox + 60, oy + 102, 177, 38, 12, ${c("shellHi")})`,
    `ring((ox + 92) * 16 + 8, (oy + 116) * 16 + 8, 72, 0, ${c("deep")})`,
    `ring((ox + 204) * 16 + 8, (oy + 116) * 16 + 8, 72, 0, ${c("deep")})`,
    `rect(ox + 76, oy + 126, 145, 14, ${c("deep")})`,
    `bar(ox + 76, oy + 130, 145, 3, ${c("tape")}, ${c("amber")}, ${c("paper")})`,
    `times(small, ox + 6, oy + 102, 286, 16, ${c("paper")}, ${c("paper")})`,
  ];
  if (item.writing !== false) {
    lines.push(
      `line(song, title, ox + 22, oy + 6, 253, 20, left, ${c("ink")})`,
      "if present(feedback) {",
      `    text(feedback, body, ox + 22, oy + 29, 253, 20, left, ${c("ink")})`,
      "} else if present(lyric) {",
      `    line(lyric, body, ox + 22, oy + 29, 253, 20, left, ${c("ink")})`,
      "} else {",
      `    line(artist, body, ox + 22, oy + 29, 253, 20, left, ${c("inkSoft")})`,
      "}",
    );
  }
  return lines;
}

function smallCassette(c) {
  return [
    `rounded(ox, oy, 153, 100, 6, ${c("shell")})`,
    `rect(ox + 6, oy + 1, 141, 1, ${c("shellHi")})`,
    ...screwLines(c, [[6, 6], [146, 6], [6, 92], [146, 92]], 2),
    `rect(ox + 12, oy + 5, 129, 63, ${c("paper")})`,
    `rect(ox + 17, oy + 28, 119, 1, ${c("rule")})`,
    `rect(ox + 12, oy + 31, 129, 3, ${c("amber")})`,
    `rect(ox + 12, oy + 34, 129, 27, ${c("red")})`,
    `rect(ox + 12, oy + 61, 129, 3, ${c("amber")})`,
    `text("A", title, ox + 17, oy + 6, 14, 20, left, ${c("red")})`,
    `text(codec, small, ox + 92, oy + 10, 44, 16, right, ${c("inkSoft")})`,
    `rounded(ox + 26, oy + 30, 101, 35, 5, ${c("shell")})`,
    `rounded(ox + 28, oy + 32, 97, 31, 4, ${c("deep")})`,
    ...reelLines(c, { ox: "ox", oy: "oy", left: 50, right: 102, middle: 47, tape: [312, 152], hub: 136, hole: 88,
      spoke: 72, dot: 24, clipBox: "ox + 28, oy + 32, 97, 31" }),
    `icon(icon_kind, ox + 68, oy + 38, 18, 18, ${c("paper")})`,
    `trapezoid(ox + 26, oy + 72, 101, 28, 8, ${c("shellHi")})`,
    `ring((ox + 50) * 16 + 8, (oy + 82) * 16 + 8, 56, 0, ${c("deep")})`,
    `ring((ox + 102) * 16 + 8, (oy + 82) * 16 + 8, 56, 0, ${c("deep")})`,
    `rect(ox + 38, oy + 89, 77, 11, ${c("deep")})`,
    `bar(ox + 38, oy + 92, 77, 3, ${c("tape")}, ${c("amber")}, ${c("paper")})`,
  ];
}

function cassetteLines(item, place, c, shape) {
  const head = [
    `let ox = ${place.x}`,
    `let oy = ${place.y}`,
    "let icon_kind = badge == none && !playing ? pause : badge",
    "let turn = (0 - tick * 8) & 255",
  ];
  return [...head, ...(cassetteSize(item, shape) === "small" ? smallCassette(c) : largeCassette(item, c))];
}

function turntableLines(place, c) {
  return [
    `let rx = ${place.x} + 67`,
    `let ry = ${place.y} + 67`,
    "let qx = rx * 16 + 8",
    "let qy = ry * 16 + 8",
    `ring(qx, qy, 1072, 1024, ${c("metal")})`,
    `ring(qx, qy, 1024, 0, ${c("vinyl")})`,
    "if has_cover {",
    "    round_cover(rx, ry, 64)",
    `    ring(qx, qy, 1072, 1024, ${c("metal")})`,
    `    ring(qx, qy, 80, 0, ${c("vinyl")})`,
    "} else {",
    `    ring(qx, qy, 448, 0, ${c("label")})`,
    `    ring(qx, qy, 368, 352, ${c("ring")})`,
    "    repeat groove from 0 to 3 {",
    "        let radius = 720 + 112 * groove",
    `        ring(qx, qy, radius + 16, radius, ${c("metalDark")})`,
    "    }",
    "}",
    `spoke(rx, ry, ((tick >>> 1) % 9 * 2 + (tick & 1)) * 256 / 18, 32, 44, 58, ${c("metal")})`,
    `ring(qx, qy, 48, 0, ${c("metal")})`,
    "if badge != none {",
    `    ring(qx, qy, 320, 0, ${c("vinyl")})`,
    `    icon(badge, rx - 9, ry - 9, 18, 18, ${c("ring")})`,
    "}",
    "let angle = 140 + progress(12)",
    "let head = angle + 10",
    "let px = rx + 75",
    "let py = ry - 47",
    "let s = 84 * sin(angle)",
    "let c = 84 * cos(angle)",
    "let sx = px + (s + (s >= 0 ? 127 : -127)) / 255",
    "let sy = py - (c + (c >= 0 ? 127 : -127)) / 255",
    "let pqx = px * 16 + 8",
    "let pqy = py * 16 + 8",
    `spoke(px, py, angle + 128, 160, 7, 21, ${c("metalDark")})`,
    `ring(pqx, pqy, 160, 0, ${c("metalDark")})`,
    `ring(pqx, pqy, 112, 0, ${c("metal")})`,
    `spoke(px, py, angle, 64, 0, 72, ${c("metal")})`,
    `spoke(sx, sy, head, 112, -14, 3, ${c("metal")})`,
    `spoke(sx, sy, head, 48, -11, 0, ${c("vinyl")})`,
    `ring(pqx, pqy, 48, 0, ${c("metalDark")})`,
    `ring((rx + 80) * 16 + 8, (ry + 59) * 16 + 8, 48, 0, playing ? ${c("lamp")} : ${c("metalDark")})`,
  ];
}

function clockLines(place, c) {
  return [
    "let secs = min((shown >>> 3) / 125, 35999)",
    "let hours = secs / 3600",
    "let minutes = secs / 60 % 60",
    "let rest = secs % 60",
    "let level = min(volume, 99)",
    "let vs = volume_shown",
    `let bx = ${place.x}`,
    `let by = ${place.y}`,
    `segment_style(22, 40, 5, 8, ${c("lit")}, ${c("dim")})`,
    "digit(bx, by, vs ? 0 : hours > 0 ? segments(hours % 10) : 0)",
    "digit(bx + 30, by, vs ? 0 : minutes >= 10 || hours > 0 ? segments(minutes / 10 % 10) : 0)",
    "digit(bx + 56, by, vs ? 0 : segments(minutes % 10))",
    "digit(bx + 86, by, vs ? (level >= 10 ? segments(level / 10 % 10) : 0) : segments(rest / 10 % 10))",
    "digit(bx + 112, by, segments(vs ? level % 10 : rest % 10))",
    "colon(bx + 22, by, !vs && hours > 0)",
    "colon(bx + 78, by, !vs)",
  ];
}

function trackLines(place, c) {
  return [
    `let tx = ${place.x}`,
    `let ty = ${place.y}`,
    `segment_style(13, 24, 3, 8, ${c("lit")}, ${c("dim")})`,
    "let number = min(track, 999)",
    "digit(tx, ty, number >= 100 ? segments(number / 100 % 10) : 0)",
    "digit(tx + 16, ty, number >= 10 ? segments(number / 10 % 10) : 0)",
    "digit(tx + 32, ty, segments(number % 10))",
  ];
}

function levelLines(place, c) {
  const bands = Math.max(1, Math.floor((place.w + 2) / 9));
  const cells = Math.max(1, Math.min(LEVEL_CELLS_MOST, Math.floor((place.h + 2) / 5)));
  return [
    `let lx = ${place.x}`,
    `let ly = ${place.y} + ${place.h - 3}`,
    `repeat band from 0 to ${bands} {`,
    "    let lit = 0",
    "    if playing {",
    "        lit = -100",
    "        repeat back from 0 to 3 {",
    "            let f = tick - back",
    "            let v = f * 0x9E3779B1 + band * 0x85EBCA6B",
    "            v = v ^ (v >>> 15)",
    "            v = v * 0x2C1B3C6D",
    "            v = v ^ (v >>> 12)",
    "            let phase = (f >>> 1) % 3 * 2 + (f & 1)",
    "            let beat = phase == 0 ? 3 : phase == 1 ? 1 : 0",
    `            lit = max(lit, 5 - 4 * band / ${bands} + (band < ${bands} / 2 ? beat : 0) + (v >>> 30) - ((v >>> 28) & 3) / 2 - 2 * back)`,
    "        }",
    "        lit = min(max(lit, 0), 8)",
    "    }",
    `    repeat cell from 0 to ${cells} {`,
    `        rect(lx + 9 * band, ly - 5 * cell, 7, 3, cell * 8 < lit * ${cells} ? ${c("lit")} : ${c("dim")})`,
    "    }",
    "}",
  ];
}

export function partLines(item, place, colorOf, theme, shape) {
  const c = painter(item, colorOf, theme);
  switch (item.type) {
    case "vu":
      return vuLines(item, place, c, shape);
    case "cassette":
      return cassetteLines(item, place, c, shape);
    case "turntable":
      return turntableLines(place, c);
    case "clock":
      return clockLines(place, c);
    case "tracknumber":
      return trackLines(place, c);
    case "levels":
      return levelLines(place, c);
    default:
      return [];
  }
}

export function dotLines(place, color, track) {
  const count = Math.max(1, Math.floor((place.w + 1) / 4));
  return [
    `let filled = volume_shown ? volume_width(${count}) : progress(${count})`,
    `repeat cell from 0 to ${count} {`,
    `    rect(${place.x} + 4 * cell, ${place.y}, 3, ${place.h}, cell < filled ? ${color} : ${track})`,
    "}",
  ];
}

const FACE_MARKS = { vu: ["PLATE", "FACE_TOP"], cassette: ["SHELL", "TAPE"], turntable: ["VINYL", "ALU"], lcd: ["GLASS", "GHOST"] };

export function faceOf(script) {
  const named = new Set([...script.matchAll(/^const ([A-Z][A-Z0-9_]*) = #/gm)].map((found) => found[1]));
  return Object.keys(FACE_MARKS).find((face) => FACE_MARKS[face].every((name) => named.has(name))) ?? null;
}

function colours(type, picks, consts) {
  return Object.fromEntries(PART_COLOURS[type].map(([key, , value]) => [key, consts[picks[key]] ?? value]));
}

export function faceDesign(face, consts, make) {
  const f = (x, y, w, h) => ({ x, y, w, h });
  const text = (wide, tall, props) => make("text", wide, tall, { scroll: true, feedback: false, align: "left", tallAlign: "center", ...props });
  if (face === "vu") {
    const meter = colours("vu", { frame: "BEZEL", faceTop: "FACE_TOP", faceBottom: "FACE_BOTTOM", ink: "INK", red: "RED", label: "MUTED", lamp: "AMBER", lampOff: "TRACK" }, consts);
    const [cream, muted, amber, track] = [consts.CREAM, consts.MUTED, consts.AMBER, consts.TRACK];
    return {
      version: 1,
      background: { style: "solid", color: consts.PLATE, top: consts.PLATE, bottom: consts.PLATE },
      layers: [
        make("vu", f(10, 4, 146, 72), f(12, 31, 146, 72), { colours: meter, channel: "left", tallChannel: "both" }),
        make("vu", f(164, 4, 146, 72), null, { colours: meter, channel: "right", tallChannel: "right" }),
        text(f(10, 78, 300, 20), f(10, 134, 150, 22), { source: "song", size: "large", align: "center", color: cream }),
        text(f(10, 97, 300, 20), f(10, 156, 150, 22), { source: "lyric", size: "medium", align: "center", color: muted, feedback: true }),
        text(null, f(10, 204, 150, 16), { source: "format", size: "small", align: "center", color: muted, scroll: false }),
        text(null, f(10, 238, 150, 16), { source: "modes", size: "small", align: "center", color: muted, scroll: false }),
        make("progress", f(84, 130, 170, 3), f(10, 257, 150, 3), { style: "bar", color: amber, track, thickness: 3 }),
        make("playmark", f(66, 126, 10, 10), f(80, 265, 10, 10), { color: amber }),
        make("times", f(10, 123, 300, 16), f(10, 262, 150, 16), { size: "small", color: cream, color2: muted }),
      ],
    };
  }
  if (face === "turntable") {
    const deck = colours("turntable", { vinyl: "VINYL", metal: "ALU", metalDark: "ALU_DARK", ring: "CREAM", lamp: "AMBER" }, consts);
    const [cream, tan, dim] = [consts.CREAM, consts.TAN, consts.TAN_DIM];
    return {
      version: 1,
      background: { style: "gradient", color: "#281810", top: "#482c18", bottom: "#281810" },
      layers: [
        make("turntable", f(10, 6, 153, 135), f(10, 6, 153, 135), { colours: deck }),
        text(f(170, 4, 140, 22), f(10, 144, 150, 22), { source: "song", size: "large", color: cream }),
        text(f(170, 26, 140, 20), f(10, 166, 150, 22), { source: "artist", size: "medium", color: tan }),
        text(f(170, 46, 140, 20), f(10, 188, 150, 22), { source: "album", size: "medium", color: dim }),
        text(f(170, 73, 140, 16), f(10, 212, 150, 16), { source: "format", size: "small", color: tan, scroll: false }),
        text(f(170, 92, 140, 16), null, { source: "detail", size: "small", color: dim, scroll: false }),
        text(f(170, 107, 140, 16), f(10, 238, 150, 16), { source: "modes", size: "small", color: dim, scroll: false, feedback: true }),
        make("progress", f(170, 124, 140, 3), f(10, 257, 150, 3), { style: "bar", color: cream, track: consts.ALU_DARK, thickness: 3 }),
        make("times", f(170, 129, 140, 16), f(10, 262, 150, 16), { size: "small", color: tan, color2: tan }),
      ],
    };
  }
  if (face === "cassette") {
    const shell = colours("cassette", { shell: "SHELL", shellHi: "SHELL_HI", deep: "DEEP", tape: "TAPE", paper: "LABEL", ink: "INK", inkSoft: "INK_SOFT", red: "RED", amber: "AMBER", rule: "RULE" }, consts);
    const [paper, ink, soft] = [consts.LABEL, consts.INK, consts.INK_SOFT];
    return {
      version: 1,
      background: { style: "solid", color: consts.DEEP, top: consts.DEEP, bottom: consts.DEEP },
      layers: [
        make("cassette", f(11, 3, 297, 140), f(8, 31, 153, 100), { colours: shell, size: "large", tallSize: "small", writing: true }),
        make("shape", null, f(8, 139, 153, 109), { kind: "rect", color: paper, radius: 0, thickness: 2 }),
        make("shape", null, f(8, 139, 153, 2), { kind: "rect", color: consts.RED, radius: 0, thickness: 2 }),
        text(null, f(16, 142, 137, 20), { source: "song", size: "large", color: ink }),
        text(null, f(16, 165, 137, 20), { source: "artist", size: "medium", color: soft }),
        text(null, f(16, 188, 137, 20), { source: "album", size: "medium", color: soft }),
        text(null, f(16, 212, 137, 16), { source: "stream", size: "small", color: soft, scroll: false, feedback: true }),
        make("times", null, f(16, 229, 137, 16), { size: "small", color: ink, color2: ink }),
      ],
    };
  }
  if (face === "lcd") {
    const lit = { lit: consts.INK, dim: consts.GHOST };
    const ink = consts.INK;
    return {
      version: 1,
      background: { style: "solid", color: consts.BEZEL, top: consts.BEZEL, bottom: consts.BEZEL },
      layers: [
        make("shape", f(5, 3, 310, 140), f(4, 3, 162, 274), { kind: "rect", color: consts.GLASS, radius: 0, thickness: 2 }),
        make("playmark", f(12, 10, 9, 9), f(10, 10, 9, 9), { color: ink }),
        text(f(41, 7, 260, 16), f(40, 7, 120, 16), { source: "modes", size: "small", color: ink, scroll: false }),
        text(f(12, 26, 60, 16), f(37, 58, 60, 16), { source: "custom", custom: "TRACK", size: "small", color: ink, scroll: false, tallAlign: "left" }),
        make("tracknumber", f(12, 42, 45, 24), f(85, 47, 45, 24), { colours: lit }),
        make("clock", f(68, 26, 134, 40), f(16, 79, 134, 40), { colours: lit }),
        make("levels", f(220, 28, 88, 38), f(14, 128, 142, 38), { colours: lit }),
        make("shape", f(12, 71, 296, 20), f(10, 174, 150, 20), { kind: "rect", color: consts.GHOST, radius: 0, thickness: 2 }),
        text(f(12, 71, 296, 20), f(10, 174, 150, 20), { source: "song", size: "large", color: ink, tallAlign: "center" }),
        text(f(12, 93, 296, 20), f(10, 197, 150, 20), { source: "lyric", size: "medium", color: ink, feedback: true }),
        text(null, f(10, 217, 150, 20), { source: "album", size: "medium", color: ink }),
        make("progress", f(12, 118, 296, 4), f(11, 244, 148, 4), { style: "dots", color: ink, track: consts.GHOST, thickness: 4 }),
        make("times", f(12, 125, 296, 16), f(10, 251, 150, 16), { size: "small", color: ink, color2: ink }),
      ],
    };
  }
  return null;
}
