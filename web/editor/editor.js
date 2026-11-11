import {
  BOX_COLOR_FIELDS,
  EDGES,
  ThemeError,
  defaultTheme,
  pack,
  toToml,
  unpack,
} from "./theme_format.js";

const KEY_DOWN = 0;
const KEY_UP = 1;
const KEY_PLAY = 2;
const KEY_MENU = 3;
const KEY_BACK = 4;
const KEYBOARD = {
  ArrowDown: KEY_DOWN,
  ArrowUp: KEY_UP,
  Enter: KEY_PLAY,
  " ": KEY_PLAY,
  Backspace: KEY_BACK,
  Escape: KEY_BACK,
  m: KEY_MENU,
};
const TAP_MS = 60;
const SETTLE_MS = 380;
const LONG_MS = 800;
const HELD_MS = 600000;
const STEP_MS = 16;
const MAX_CATCH_UP_MS = 1000;
const THEME_BUFFER = 512;
const TO_ALBUMS = [tap(KEY_PLAY), tap(KEY_DOWN), tap(KEY_DOWN), tap(KEY_DOWN), tap(KEY_PLAY)];
const SCREENS = {
  home: [],
  list: TO_ALBUMS,
  playing: [...TO_ALBUMS, tap(KEY_PLAY), tap(KEY_PLAY)],
  toast: [...TO_ALBUMS, hold(KEY_PLAY, LONG_MS)],
  letters: [tap(KEY_PLAY), ...Array(6).fill(tap(KEY_DOWN)), tap(KEY_PLAY), hold(KEY_DOWN, HELD_MS, 0)],
  settings: [tap(KEY_DOWN), tap(KEY_PLAY)],
};
const BOX_NUMBERS = [
  ["border_width", "Border width", 8],
  ["shine_height", "Shine height", 8],
  ["inset_across", "Inset across", 40],
  ["inset_down", "Inset down", 12],
  ["radius", "Corner radius", 32],
];
const COLOR_LABELS = {
  fill_top: "Fill, top",
  fill_bottom: "Fill, bottom",
  border: "Border",
  shine: "Shine",
  ink: "Text on it",
};

const status = document.getElementById("status");
const canvas = document.getElementById("screen");
const context = canvas.getContext("2d");
let schema;
let wasm;
let theme;
let themePointer;
let script = [];
let waitMs = 0;
let lastFrame;
let currentScreen = "list";

function tap(key) {
  return { key, down: TAP_MS, after: SETTLE_MS };
}

function hold(key, down, after = SETTLE_MS) {
  return { key, down, after };
}

function label(name) {
  const words = name.replaceAll("_", " ");
  return words[0].toUpperCase() + words.slice(1);
}

function say(text, bad = false) {
  status.textContent = text;
  status.classList.toggle("bad", bad);
}

function put(bytes) {
  const pointer = wasm.preview_alloc(bytes.length);
  new Uint8Array(wasm.memory.buffer, pointer, bytes.length).set(bytes);
  return pointer;
}

async function fetchBytes(path) {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`${path}: ${response.status}`);
  }
  return new Uint8Array(await response.arrayBuffer());
}

async function addFile(cardPath, sitePath) {
  const bytes = await fetchBytes(sitePath);
  wasm.preview_add_file(put(new TextEncoder().encode(cardPath + "\0")), put(bytes), bytes.length);
}

function applyTheme() {
  try {
    const bytes = pack(theme, schema);
    new Uint8Array(wasm.memory.buffer, themePointer, bytes.length).set(bytes);
    const accepted = wasm.preview_theme(themePointer, bytes.length) === 1;
    say(accepted ? `${bytes.length} bytes` : "The player refused this theme", !accepted);
    return bytes;
  } catch (error) {
    if (!(error instanceof ThemeError)) {
      throw error;
    }
    say(error.message, true);
    return null;
  }
}

function restart() {
  const orientation = Number(document.getElementById("orientation").value);
  wasm.preview_start(orientation, 0);
  applyTheme();
  script = SCREENS[currentScreen].map((step) => ({ ...step, pressed: false }));
  waitMs = SETTLE_MS;
}

function runScript(elapsed) {
  waitMs -= elapsed;
  if (waitMs > 0 || script.length === 0) {
    return;
  }
  const step = script[0];
  if (!step.pressed) {
    wasm.preview_key(step.key, 1);
    step.pressed = true;
    waitMs = step.down;
  } else {
    wasm.preview_key(step.key, 0);
    script.shift();
    waitMs = step.after;
  }
}

function resize(width, height) {
  const zoom = Number(document.getElementById("zoom").value);
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
  canvas.style.width = `${width * zoom}px`;
  canvas.style.height = `${height * zoom}px`;
}

function draw() {
  const width = wasm.preview_width();
  const height = wasm.preview_height();
  resize(width, height);
  const pixels = new Uint8ClampedArray(wasm.memory.buffer, wasm.preview_pixels(), width * height * 4);
  context.putImageData(new ImageData(pixels, width, height), 0, 0);
}

function frame(now) {
  let behind = Math.min(Math.round(now - (lastFrame ?? now)), MAX_CATCH_UP_MS);
  lastFrame = now;
  do {
    const slice = Math.min(behind, STEP_MS);
    runScript(slice);
    wasm.preview_step(slice);
    behind -= slice;
  } while (behind > 0);
  draw();
  requestAnimationFrame(frame);
}

function field(text, control, className) {
  const wrap = document.createElement("label");
  wrap.append(text, control);
  if (className) {
    wrap.className = className;
  }
  return wrap;
}

function colorSelect(box, name) {
  const select = document.createElement("select");
  for (const color of schema.colors) {
    select.append(new Option(label(color), color, false, color === box[name]));
  }
  select.addEventListener("change", () => {
    box[name] = select.value;
    applyTheme();
  });
  return select;
}

function numberInput(box, name, most) {
  const input = document.createElement("input");
  Object.assign(input, { type: "number", min: 0, max: most, value: box[name] });
  input.addEventListener("input", () => {
    box[name] = Math.max(0, Math.min(most, Math.round(Number(input.value) || 0)));
    applyTheme();
  });
  return input;
}

function edgeBoxes(box) {
  const wrap = document.createElement("div");
  wrap.className = "row edges";
  wrap.append("Border on");
  for (const [edge, bit] of Object.entries(EDGES)) {
    const check = document.createElement("input");
    check.type = "checkbox";
    check.checked = (box.border_edges & bit) !== 0;
    check.addEventListener("change", () => {
      box.border_edges = check.checked ? box.border_edges | bit : box.border_edges & ~bit;
      applyTheme();
    });
    wrap.append(field(label(edge), check));
  }
  return wrap;
}

function buildBoxes() {
  const host = document.getElementById("boxes");
  host.replaceChildren();
  for (const name of schema.boxes) {
    const box = theme.boxes[name];
    const details = document.createElement("details");
    const summary = document.createElement("summary");
    const grid = document.createElement("div");
    summary.textContent = label(name);
    grid.className = "box";
    for (const color of BOX_COLOR_FIELDS) {
      grid.append(field(COLOR_LABELS[color], colorSelect(box, color)));
    }
    for (const [number, text, most] of BOX_NUMBERS) {
      grid.append(field(text, numberInput(box, number, most)));
    }
    grid.append(edgeBoxes(box));
    details.append(summary, grid);
    host.append(details);
  }
}

function buildColors() {
  const host = document.getElementById("colors");
  host.replaceChildren();
  for (const name of schema.colors) {
    const input = document.createElement("input");
    input.type = "color";
    input.value = theme.colors[name];
    input.addEventListener("input", () => {
      theme.colors[name] = input.value;
      applyTheme();
    });
    host.append(field(label(name), input));
  }
}

function buildSpacing() {
  const host = document.getElementById("spacing");
  host.replaceChildren();
  for (const entry of schema.spacing) {
    const custom = document.createElement("input");
    const slider = document.createElement("input");
    const output = document.createElement("output");
    custom.type = "checkbox";
    custom.checked = theme.spacing[entry.name] !== null;
    Object.assign(slider, { type: "range", min: entry.least, max: entry.most });
    slider.value = theme.spacing[entry.name] ?? entry.least;
    const update = () => {
      theme.spacing[entry.name] = custom.checked ? Number(slider.value) : null;
      slider.disabled = !custom.checked;
      output.textContent = custom.checked ? `${slider.value} px` : "player's";
      applyTheme();
    };
    custom.addEventListener("change", update);
    slider.addEventListener("input", update);
    slider.disabled = !custom.checked;
    output.textContent = custom.checked ? `${slider.value} px` : "player's";
    const row = field(label(entry.name), custom);
    row.append(slider, output);
    host.append(row);
  }
}

function buildSelection() {
  const select = document.getElementById("selection");
  select.replaceChildren();
  for (const name of schema.boxes) {
    select.append(new Option(label(name), name, false, name === theme.selection));
  }
}

function buildPanel() {
  document.getElementById("name").value = theme.name;
  buildColors();
  buildSpacing();
  buildSelection();
  buildBoxes();
  applyTheme();
}

function download(name, bytes, type) {
  const link = document.createElement("a");
  link.href = URL.createObjectURL(new Blob([bytes], { type }));
  link.download = name;
  link.click();
  URL.revokeObjectURL(link.href);
}

function wear(bytes) {
  try {
    theme = unpack(bytes, schema);
    buildPanel();
  } catch (error) {
    if (!(error instanceof ThemeError)) {
      throw error;
    }
    say(error.message, true);
  }
}

function press(key, pressed) {
  script = [];
  wasm.preview_key(key, pressed ? 1 : 0);
}

function wireKeys() {
  for (const button of document.querySelectorAll("#keys button")) {
    const key = Number(button.dataset.key);
    button.addEventListener("pointerdown", () => press(key, true));
    button.addEventListener("pointerup", () => press(key, false));
    button.addEventListener("pointerleave", () => press(key, false));
  }
  canvas.addEventListener("keydown", (event) => {
    if (event.key in KEYBOARD && !event.repeat) {
      press(KEYBOARD[event.key], true);
    }
    if (event.key in KEYBOARD) {
      event.preventDefault();
    }
  });
  canvas.addEventListener("keyup", (event) => {
    if (event.key in KEYBOARD) {
      press(KEYBOARD[event.key], false);
    }
  });
}

function wirePanel() {
  document.getElementById("name").addEventListener("input", (event) => {
    theme.name = event.target.value;
    applyTheme();
  });
  document.getElementById("selection").addEventListener("change", (event) => {
    theme.selection = event.target.value;
    applyTheme();
  });
  document.getElementById("save").addEventListener("click", () => {
    const bytes = applyTheme();
    if (bytes) {
      download(`${theme.name}.echotheme`, bytes, "application/octet-stream");
    }
  });
  document.getElementById("save-source").addEventListener("click", () => {
    download(`${theme.name}.toml`, toToml(theme, schema), "text/plain");
  });
  const picker = document.getElementById("picker");
  document.getElementById("open").addEventListener("click", () => picker.click());
  picker.addEventListener("change", async () => {
    if (picker.files.length > 0) {
      wear(new Uint8Array(await picker.files[0].arrayBuffer()));
      picker.value = "";
    }
  });
}

function wireStage() {
  for (const button of document.querySelectorAll("#screens button")) {
    button.addEventListener("click", () => {
      currentScreen = button.dataset.screen;
      restart();
    });
  }
  document.getElementById("orientation").addEventListener("change", restart);
  const samples = document.getElementById("samples");
  samples.append(new Option("Start from…", ""));
  for (const sample of schema.samples) {
    samples.append(new Option(label(sample), sample));
  }
  samples.addEventListener("change", async () => {
    if (samples.value) {
      wear(await fetchBytes(`themes/${samples.value}.echotheme`));
      samples.value = "";
    }
  });
}

async function main() {
  schema = await (await fetch("theme_schema.json")).json();
  const loaded = await WebAssembly.instantiateStreaming(fetch("echo_preview.wasm"), {});
  wasm = loaded.instance.exports;
  await addFile("/.echo/library.db", "library.db");
  await addFile("/.echo/covers.bin", "covers.bin");
  for (const face of schema.faces) {
    const bytes = await fetchBytes(face.file);
    wasm.preview_add_face(face.index, put(bytes), bytes.length);
  }
  themePointer = wasm.preview_alloc(THEME_BUFFER);
  theme = defaultTheme(schema);
  wireKeys();
  wirePanel();
  wireStage();
  buildPanel();
  restart();
  requestAnimationFrame(frame);
}

main().catch((error) => say(`Could not start: ${error.message}`, true));
