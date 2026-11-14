import { ALIGNS } from "./theme_format.js";

const LABELS = {
  cover: "Cover",
  title: "Title",
  artist: "Artist",
  album: "Album",
  format: "Format",
  detail: "Bit rate and place",
  modes: "Shuffle and repeat",
  progress: "Progress bar",
  times: "Times",
  lyrics: "Lyrics",
};
const NUMBERS = ["x", "y", "width", "height"];
const LINE = "rgba(91, 155, 245, 0.9)";
const PICKED = "rgba(242, 178, 74, 1)";

function placeOf(builtin, entry) {
  const box = builtin;
  if (entry.name === "cover") {
    return { x: box.x, y: box.y, size: box.width };
  }
  return { x: box.x, y: box.y, width: box.width, height: box.height, align: ALIGNS[box.align] ?? "left" };
}

export function effectivePlaces(builtin, places, schema) {
  return schema.layout.map((entry, index) => {
    const place = places[entry.name];
    if (!place) {
      return { entry, given: false, ...placeOf(builtin[index], entry) };
    }
    return { entry, given: true, ...place };
  });
}

function boxOf(place) {
  if (place.hide) {
    return null;
  }
  const width = place.entry.name === "cover" ? place.size : place.width;
  const height = place.entry.name === "cover" ? place.size : place.height;
  return { x: place.x, y: place.y, width, height };
}

export function placeAt(places, x, y) {
  for (let index = places.length - 1; index >= 0; index--) {
    const box = boxOf(places[index]);
    if (box && x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height) {
      return places[index];
    }
  }
  return null;
}

function numberInput(value, change) {
  const input = document.createElement("input");
  Object.assign(input, { type: "number", value });
  input.addEventListener("change", () => change(Math.round(Number(input.value) || 0)));
  return input;
}

class LayoutEditor {
  constructor(host, overlay, context) {
    Object.assign(this, { host, overlay, ...context });
    this.picked = null;
    this.dragging = null;
    this.wireOverlay();
  }

  get key() {
    return this.schema.layouts[this.orientation() === 0 ? 0 : 1];
  }

  get places() {
    return effectivePlaces(this.preview.layout(), this.theme.layouts[this.key], this.schema);
  }

  put(name, place) {
    this.theme.layouts[this.key][name] = place;
    this.changed();
    this.build();
  }

  givenPlace(place) {
    const { entry, given, ...rest } = place;
    return rest;
  }

  row(place) {
    const row = document.createElement("div");
    const name = place.entry.name;
    row.className = `row place${this.picked === name ? " picked" : ""}`;
    const own = document.createElement("input");
    own.type = "checkbox";
    own.checked = place.given;
    own.title = "Set by this theme; untick for the player's own place";
    own.addEventListener("change", () => this.put(name, own.checked ? this.givenPlace(place) : null));
    const label = document.createElement("label");
    label.append(own, LABELS[name] ?? name);
    row.append(label);
    if (!place.given) {
      return row;
    }
    const hide = document.createElement("input");
    hide.type = "checkbox";
    hide.checked = place.hide === true;
    hide.addEventListener("change", () => this.put(name, hide.checked ? { hide: true } : this.givenPlace({ ...place, ...placeOf(this.preview.layout()[this.schema.layout.indexOf(place.entry)], place.entry), hide: undefined })));
    const hidden = document.createElement("label");
    hidden.append(hide, "Hide");
    row.append(hidden);
    if (place.hide) {
      return row;
    }
    const keys = name === "cover" ? ["x", "y", "size"] : NUMBERS;
    for (const key of keys) {
      const field = document.createElement("label");
      field.append(key, numberInput(place[key], (value) => this.put(name, { ...this.givenPlace(place), [key]: value })));
      row.append(field);
    }
    if (name !== "cover" && name !== "progress" && name !== "times") {
      const align = document.createElement("select");
      for (const choice of ALIGNS) {
        align.append(new Option(choice, choice, false, choice === place.align));
      }
      align.addEventListener("change", () => this.put(name, { ...this.givenPlace(place), align: align.value }));
      row.append(align);
    }
    return row;
  }

  build() {
    const start = document.createElement("button");
    const reset = document.createElement("button");
    const boxes = document.createElement("input");
    start.type = reset.type = "button";
    start.textContent = "Start from the player's layout";
    reset.textContent = "Back to the player's layout";
    boxes.type = "checkbox";
    boxes.checked = this.showing ?? true;
    boxes.addEventListener("change", () => {
      this.showing = boxes.checked;
      this.draw();
    });
    start.addEventListener("click", () => {
      for (const place of this.places) {
        this.theme.layouts[this.key][place.entry.name] = this.givenPlace(place);
      }
      this.changed();
      this.build();
    });
    reset.addEventListener("click", () => {
      for (const entry of this.schema.layout) {
        this.theme.layouts[this.key][entry.name] = null;
      }
      this.changed();
      this.build();
    });
    const tools = document.createElement("div");
    tools.className = "row";
    const show = document.createElement("label");
    show.append(boxes, "Show the boxes");
    const which = document.createElement("strong");
    which.textContent = this.orientation() === 0 ? "Landscape" : "Upright";
    tools.append(which, start, reset, show);
    this.host.replaceChildren(tools, ...this.places.map((place) => this.row(place)));
    this.draw();
  }

  visible() {
    return (this.showing ?? true) && this.host.closest("details")?.open !== false;
  }

  draw() {
    const screen = this.screen;
    const zoom = screen.clientWidth / screen.width;
    Object.assign(this.overlay, { width: screen.clientWidth, height: screen.clientHeight });
    this.overlay.style.pointerEvents = this.visible() ? "auto" : "none";
    const context = this.overlay.getContext("2d");
    context.clearRect(0, 0, this.overlay.width, this.overlay.height);
    if (!this.visible()) {
      return;
    }
    context.font = "11px system-ui, sans-serif";
    for (const place of this.places) {
      const box = boxOf(place);
      if (!box) {
        continue;
      }
      const picked = place.entry.name === this.picked;
      context.strokeStyle = context.fillStyle = picked ? PICKED : LINE;
      context.lineWidth = picked ? 2 : 1;
      context.strokeRect(box.x * zoom + 0.5, box.y * zoom + 0.5, box.width * zoom - 1, box.height * zoom - 1);
      context.fillText(LABELS[place.entry.name], box.x * zoom + 3, box.y * zoom + 11);
    }
  }

  point(event) {
    const zoom = this.screen.clientWidth / this.screen.width;
    const frame = this.overlay.getBoundingClientRect();
    return { x: (event.clientX - frame.left) / zoom, y: (event.clientY - frame.top) / zoom };
  }

  wireOverlay() {
    this.overlay.addEventListener("pointerdown", (event) => {
      const at = this.point(event);
      const place = placeAt(this.places, at.x, at.y);
      if (!place) {
        return;
      }
      this.overlay.setPointerCapture(event.pointerId);
      this.picked = place.entry.name;
      this.dragging = { place: this.givenPlace(place), from: at };
      this.build();
    });
    this.overlay.addEventListener("pointermove", (event) => {
      if (!this.dragging) {
        return;
      }
      const at = this.point(event);
      const { place, from } = this.dragging;
      const moved = { ...place, x: place.x + Math.round(at.x - from.x), y: place.y + Math.round(at.y - from.y) };
      this.theme.layouts[this.key][this.picked] = moved;
      this.changed();
      this.draw();
    });
    this.overlay.addEventListener("pointerup", () => {
      if (this.dragging) {
        this.dragging = null;
        this.build();
      }
    });
  }
}

export function buildLayout(host, theme, context) {
  const overlay = document.getElementById("overlay");
  const editor = new LayoutEditor(host, overlay, { theme, ...context });
  editor.build();
  return editor;
}
