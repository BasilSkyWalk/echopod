export const KEY = { down: 0, up: 1, play: 2, menu: 3, back: 4 };
export const STEP_MS = 16;

const TAP_MS = 60;
const SETTLE_MS = 380;
const LONG_MS = 800;
const HELD_MS = 600000;
const THEME_BUFFER = 160 * 1024;
const TO_ALBUMS = [tap(KEY.play), tap(KEY.down), tap(KEY.down), tap(KEY.down), tap(KEY.play)];
const TO_ALL_SONGS = [tap(KEY.play), ...Array(6).fill(tap(KEY.down)), tap(KEY.play)];

export const SCREENS = {
  home: [],
  list: TO_ALBUMS,
  playing: [...TO_ALBUMS, tap(KEY.play), tap(KEY.play)],
  toast: [...TO_ALBUMS, hold(KEY.play, LONG_MS)],
  letters: [...TO_ALL_SONGS, hold(KEY.down, HELD_MS, 0)],
  settings: [tap(KEY.down), tap(KEY.play)],
};

function tap(key) {
  return { key, down: TAP_MS, after: SETTLE_MS };
}

function hold(key, down, after = SETTLE_MS) {
  return { key, down, after };
}

export class Preview {
  constructor(wasm) {
    this.wasm = wasm;
    this.script = [];
    this.waitMs = 0;
    this.turned = false;
    this.themePointer = wasm.preview_alloc(THEME_BUFFER);
    this.themeBytes = null;
  }

  static async load(read, schema) {
    const module = await WebAssembly.instantiate(await read("echo_preview.wasm"), {});
    const preview = new Preview(module.instance.exports);
    await preview.addFile("/.echo/library.db", await read("library.db"));
    await preview.addFile("/.echo/covers.bin", await read("covers.bin"));
    for (const face of schema.faces) {
      const bytes = await read(face.file);
      preview.wasm.preview_add_face(face.index, preview.put(bytes), bytes.length);
    }
    return preview;
  }

  put(bytes) {
    const pointer = this.wasm.preview_alloc(bytes.length);
    new Uint8Array(this.wasm.memory.buffer, pointer, bytes.length).set(bytes);
    return pointer;
  }

  addFile(cardPath, bytes) {
    const path = this.put(new TextEncoder().encode(cardPath + "\0"));
    this.wasm.preview_add_file(path, this.put(bytes), bytes.length);
  }

  wear(bytes) {
    this.themeBytes = bytes;
    new Uint8Array(this.wasm.memory.buffer, this.themePointer, bytes.length).set(bytes);
    return this.wasm.preview_theme(this.themePointer, bytes.length) === 1;
  }

  rocker(key) {
    const swapped = { [KEY.down]: KEY.up, [KEY.up]: KEY.down };
    return this.turned && key in swapped ? swapped[key] : key;
  }

  start(orientation, screen, accent = 0, face = 0) {
    this.turned = orientation !== 0;
    this.wasm.preview_start(orientation, face, accent);
    if (this.themeBytes) {
      this.wear(this.themeBytes);
    }
    this.script = SCREENS[screen].map((step) => ({ ...step, key: this.rocker(step.key), pressed: false }));
    this.waitMs = SETTLE_MS;
  }

  key(key, pressed) {
    this.script = [];
    this.wasm.preview_key(this.rocker(key), pressed ? 1 : 0);
  }

  get busy() {
    return this.script.length > 0;
  }

  runScript(elapsed) {
    this.waitMs -= elapsed;
    if (this.waitMs > 0 || this.script.length === 0) {
      return;
    }
    const step = this.script[0];
    if (!step.pressed) {
      this.wasm.preview_key(step.key, 1);
      step.pressed = true;
      this.waitMs = step.down;
    } else {
      this.wasm.preview_key(step.key, 0);
      this.script.shift();
      this.waitMs = step.after;
    }
  }

  advance(milliseconds) {
    let behind = milliseconds;
    do {
      const slice = Math.min(behind, STEP_MS);
      this.runScript(slice);
      this.wasm.preview_step(slice);
      behind -= slice;
    } while (behind > 0);
  }

  layout() {
    this.layoutPointer ??= this.wasm.preview_alloc(2 * 5 * 16);
    const count = this.wasm.preview_layout(this.layoutPointer);
    const values = new Int16Array(this.wasm.memory.buffer, this.layoutPointer, 5 * count);
    return Array.from({ length: count }, (_, id) => ({
      x: values[5 * id],
      y: values[5 * id + 1],
      width: values[5 * id + 2],
      height: values[5 * id + 3],
      align: values[5 * id + 4],
    }));
  }

  frame() {
    const width = this.wasm.preview_width();
    const height = this.wasm.preview_height();
    const pointer = this.wasm.preview_pixels();
    const pixels = new Uint8ClampedArray(this.wasm.memory.buffer, pointer, width * height * 4);
    return { width, height, pixels };
  }
}
