import { buildAccents, shownAccent } from "./accent_editor.js";
import { buildFonts } from "./font_editor.js";
import { buildIcons } from "./icon_editor.js";
import { AREAS, colorOf, drawingScript, findElement, walk } from "./drawing.js";
import { drawingEditor } from "./drawing_editor.js";
import { STARTS, drawingFrom, faceOf, standardDrawing, startDrawing } from "./drawings.js";
import { KEY, Preview } from "./preview_host.js";
import {
  ACCENT_SHADES,
  ThemeError,
  assetMemory,
  defaultTheme,
  lint,
  opacityPercent,
  pack,
  seeThroughOf,
  toToml,
  unpack,
} from "./theme_format.js";
import { buildWallpapers } from "./wallpaper_editor.js";

const STEPS = [
  { id: "start", label: "Start", icon: "M4 10h12M10 4v12" },
  { id: "colours", label: "Colours", icon: "M10 3a7 7 0 1 0 0 14c1 0 1.5-.8 1.2-1.6-.4-1 .2-2.4 1.6-2.4H15a2 2 0 0 0 2-2 7 7 0 0 0-7-8z" },
  { id: "shapes", label: "Lists & shapes", icon: "M4 5.5h12M4 10h12M4 14.5h8" },
  { id: "home", label: "Home", icon: "M3.5 9.5 10 4l6.5 5.5V16h-4.5v-4h-4v4H3.5z" },
  { id: "playing", label: "Now Playing", icon: "M4 4h12v12H4zM8 7.5v5l4-2.5z" },
  { id: "wallpaper", label: "Wallpaper", icon: "M3.5 4.5h13v11h-13zM3.5 13l4-4 3 3 2-2 4 4" },
  { id: "fonts", label: "Fonts", icon: "M5 15 9.5 4h1L15 15M6.8 11h6.4" },
  { id: "icons", label: "Icons", icon: "M10 3.5l2 4.2 4.5.6-3.3 3.1.8 4.5L10 13.7 6 15.9l.8-4.5-3.3-3.1 4.5-.6z" },
  { id: "share", label: "Save & share", icon: "M10 3v9M6 8l4 4 4-4M4 15.5h12" },
];
const SCREENS = [
  ["home", "Home"],
  ["list", "Lists"],
  ["playing", "Now Playing"],
  ["settings", "Settings"],
  ["toast", "Message"],
];
const DESIGNS = {
  home: {
    screen: "home",
    key: "home_scene",
    title: "Home",
    lead: "Draw the first screen from boxes, circles, lines, words and the menu. Drag things on the screen to move them, and drag a corner to resize. The buttons still move through the menu from top to bottom.",
  },
  playing: {
    screen: "playing",
    key: "now_playing_scene",
    title: "Now Playing",
    lead: "Draw the Now Playing screen from boxes, circles, lines and words, and make any of them move with the music. Drag things on the screen to move them, and drag a corner to resize.",
  },
};
const COLOUR_GROUPS = [
  ["Screen", [
    ["background", "Background"], ["text", "Text"], ["text_secondary", "Secondary text"],
    ["text_tertiary", "Faint text"], ["divider", "Lines"], ["chevron", "Row arrows"], ["scrollbar_thumb", "Scrollbar"],
  ]],
  ["Selected row", [
    ["selection_background", "Fill"], ["selection_top", "Top of the fill"], ["selection_shine", "Shine"],
    ["selection_text", "Text on it"],
  ]],
  ["Title bar", [["header_background", "Fill"], ["header_top", "Top of the fill"], ["header_text", "Title"]]],
  ["Battery", [["danger", "Running low"], ["success", "Charging"]]],
];
const ROLE_LABELS = Object.fromEntries(COLOUR_GROUPS.flatMap(([, roles]) => roles));
const COLOUR_NAMES = Object.fromEntries(COLOUR_GROUPS.flatMap(([title, roles]) =>
  roles.map(([role, name]) => [role, title === "Screen" ? name : `${title}, ${name.toLowerCase()}`])));
const EDGE_BITS = [["top", "Top", 1], ["bottom", "Bottom", 2], ["left", "Left", 4], ["right", "Right", 8]];
const MORE_BOXES = [
  ["scrollbar_thumb", "Scrollbar"], ["progress_track", "Bars, empty part"], ["progress_fill", "Bars, filled part"],
  ["progress_active", "Bars, while changing"],
];
const SPACINGS = {
  row_height: "Row height", padding: "Side margin", scrollbar_width: "Scrollbar width",
  scrollbar_inset: "Scrollbar gap", detail_gap: "Gap before values",
};
const HISTORY_LIMIT = 80;
const SETTLE_MS = 350;
const DRAFT = "echopod-theme-draft";
const NAME_BYTES = 23;
const SLUG_LETTERS = 23;
const MERGED_PARTS = ["colors", "boxes", "spacing", "icons", "wallpapers", "fonts"];
const SKIP_STEP_MS = 100;
const ZOOM_STEPS = 4;
const COLOUR_SLACK = 24;
const GRADIENT_SLACK = 12;
const GRADIENT_LEAST = 30;
const SKIP_LIMIT_MS = 20000;
const SKIP_SETTLE_MS = 700;
const SCRIPT_COLOUR = /^const ([A-Z][A-Z0-9_]*) = (#[0-9a-fA-F]{6})[ \t]*$/gm;
const SCRIPT_WORDS = { HI: "highlight", ALU: "metal", DIM: "dim", BG: "background", SOFT: "soft" };
const CHROME_ROLES = [["header_top", "Title bar, top"], ["header_background", "Title bar"], ["header_text", "Title"]];

const $ = (id) => document.getElementById(id);
const canvas = $("screen");
const overlay = $("overlay");
const context = canvas.getContext("2d");
const inspector = $("inspector");

const state = {
  schema: null,
  preview: null,
  theme: null,
  designs: { playing: null, home: null },
  stash: {},
  open: new Set(),
  draftFailed: false,
  step: "start",
  screen: "home",
  turn: 0,
  accent: 0,
  picked: null,
  history: [],
  future: [],
  zoom: 2,
  zoomChoice: null,
  gallery: [],
  lastFrame: null,
  remember: null,
};

function el(tag, attributes = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(attributes)) {
    if (key === "class") {
      node.className = value;
    } else if (key.startsWith("on")) {
      node.addEventListener(key.slice(2), value);
    } else if (value !== undefined && value !== null && value !== false) {
      node.setAttribute(key, value === true ? "" : value);
    }
  }
  node.append(...children.flat().filter((child) => child !== null && child !== undefined));
  return node;
}

function icon(path) {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 20 20");
  svg.setAttribute("aria-hidden", "true");
  const line = document.createElementNS("http://www.w3.org/2000/svg", "path");
  line.setAttribute("d", path);
  svg.append(line);
  return svg;
}

let toastTimer;
function say(text, bad = false) {
  const toast = $("status");
  toast.textContent = text;
  toast.classList.toggle("bad", bad);
  toast.classList.add("shown");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove("shown"), bad ? 6000 : 2200);
}

function snapshot() {
  return JSON.stringify({ theme: state.theme, designs: state.designs });
}

function mergedTheme(saved) {
  const base = defaultTheme(state.schema);
  const theme = { ...base, ...saved };
  for (const part of MERGED_PARTS) {
    theme[part] = { ...base[part], ...(saved[part] ?? {}) };
  }
  for (const name of Object.keys(base.boxes)) {
    theme.boxes[name] = { ...base.boxes[name], ...(saved.boxes?.[name] ?? {}) };
  }
  theme.layouts = Object.fromEntries(Object.keys(base.layouts).map((key) =>
    [key, { ...base.layouts[key], ...(saved.layouts?.[key] ?? {}) }]));
  return theme;
}

function revived(design, screen) {
  return design ? drawingFrom(JSON.stringify(design), screen) : null;
}

function restore(text) {
  const saved = JSON.parse(text);
  state.theme = mergedTheme(saved.theme ?? {});
  const designs = saved.designs ?? { playing: saved.design ?? null, home: null };
  state.designs = { playing: revived(designs.playing, "playing"), home: revived(designs.home, "home") };
  state.picked = currentDesign() && findElement(currentDesign(), state.picked) ? state.picked : null;
}

function currentDesign() {
  return DESIGNS[state.step] ? state.designs[state.step] : null;
}

function designScreen() {
  return DESIGNS[state.step]?.screen ?? "playing";
}

function record() {
  const now = snapshot();
  if (state.history[state.history.length - 1] !== now) {
    state.history.push(now);
    state.history.splice(0, Math.max(0, state.history.length - HISTORY_LIMIT));
    state.future = [];
    saveDraft(now);
  }
  refreshUndo();
}

function remember() {
  clearTimeout(state.remember);
  state.remember = setTimeout(() => {
    state.remember = null;
    record();
  }, SETTLE_MS);
}

function settleHistory() {
  if (state.remember) {
    clearTimeout(state.remember);
    state.remember = null;
    record();
  }
}

function refreshUndo() {
  $("undo").disabled = state.history.length < 2;
  $("redo").disabled = state.future.length === 0;
}

function travel(back) {
  settleHistory();
  if (back) {
    if (state.history.length < 2) {
      return;
    }
    state.future.push(state.history.pop());
  } else {
    if (state.future.length === 0) {
      return;
    }
    state.history.push(state.future.pop());
  }
  restore(state.history[state.history.length - 1]);
  saveDraft(state.history[state.history.length - 1]);
  restart();
  render();
  refreshUndo();
}

function saveDraft(text) {
  try {
    localStorage.setItem(DRAFT, text);
  } catch {
    if (!state.draftFailed) {
      state.draftFailed = true;
      say("This browser won't keep your work between visits. Download it before you close the page.", true);
    }
  }
}

function clearDraft() {
  try {
    localStorage.removeItem(DRAFT);
  } catch {
    return;
  }
}

function loadDraft() {
  try {
    return localStorage.getItem(DRAFT);
  } catch {
    return null;
  }
}

function syncScene() {
  for (const [step, { key }] of Object.entries(DESIGNS)) {
    const design = state.designs[step];
    if (design) {
      state.theme[key] = { script: drawingScript(design, state.theme), chrome: null, design: JSON.stringify(design) };
    }
  }
}

function apply() {
  syncScene();
  try {
    const bytes = pack(state.theme, state.schema);
    if (!state.preview.wear(bytes)) {
      say("The player couldn't read this theme", true);
    }
    return bytes;
  } catch (error) {
    if (!(error instanceof ThemeError)) {
      throw error;
    }
    say(error.message, true);
    return null;
  }
}

function fits() {
  syncScene();
  try {
    pack(state.theme, state.schema);
    return true;
  } catch (error) {
    if (!(error instanceof ThemeError)) {
      throw error;
    }
    return false;
  }
}

function changed({ rebuild = false } = {}) {
  apply();
  remember();
  if (rebuild) {
    render();
  } else {
    refreshNotices();
  }
}

function refreshNotices() {
  const box = inspector.querySelector("[data-notice]");
  if (box) {
    box.replaceWith(noticeBox());
  }
}

function noticeBox() {
  const found = lint(state.theme, state.schema);
  if (found.length === 0) {
    return el("div", { "data-notice": "" });
  }
  return el("div", { class: "notice", "data-notice": "" },
    found.length === 1 ? "One thing may be hard to read:" : `${found.length} things may be hard to read:`,
    el("ul", {}, found.map((text) => el("li", {}, text))));
}

function field(label, control, note) {
  return el("label", { class: "field" }, el("span", {}, label, note ? el("small", {}, note) : null), control);
}

function group(...fields) {
  return el("div", { class: "group" }, ...fields);
}

function colourInput(value, set) {
  const picker = el("input", { type: "color", value });
  const hex = el("input", { class: "hex", value, maxlength: 7, spellcheck: "false", "aria-label": "Colour code" });
  picker.addEventListener("input", () => {
    hex.value = picker.value;
    set(picker.value);
  });
  hex.addEventListener("change", () => {
    const tidy = hex.value.trim().replace(/^#?/, "#").toLowerCase();
    if (/^#[0-9a-f]{6}$/.test(tidy)) {
      picker.value = hex.value = tidy;
      set(tidy);
    } else {
      hex.value = picker.value;
    }
  });
  return el("span", { class: "color-control" }, picker, hex);
}

function shownAccentOf(role) {
  const accents = state.theme.accents ?? [];
  return ACCENT_SHADES.includes(role) && accents.length > 0 ? accents[Math.min(state.accent, accents.length - 1)] : null;
}

function shownColour(role) {
  return shownAccentOf(role)?.[role] ?? state.theme.colors[role];
}

function roleColour(role) {
  const control = colourInput(shownColour(role), (value) => {
    const accent = shownAccentOf(role);
    if (accent) {
      accent[role] = value;
    } else {
      state.theme.colors[role] = value;
    }
    changed();
  });
  const row = field(ROLE_LABELS[role], control);
  row.dataset.role = role;
  return row;
}

function choiceColour(value, set) {
  const roles = COLOUR_GROUPS.flatMap(([, list]) => list.map(([role]) => role));
  const select = el("select", { "aria-label": "Colour" },
    roles.map((role) => new Option(COLOUR_NAMES[role], `@${role}`)),
    new Option("My own colour", "custom"));
  select.value = typeof value === "string" && value.startsWith("@") ? value : "custom";
  const picker = el("input", { type: "color", value: colorOf(value, state.theme) });
  picker.addEventListener("input", () => {
    select.value = "custom";
    set(picker.value);
  });
  select.addEventListener("change", () => {
    if (select.value === "custom") {
      set(picker.value);
    } else {
      picker.value = colorOf(select.value, state.theme);
      set(select.value);
    }
  });
  return el("span", { class: "color-control" }, picker, select);
}

function range(value, least, most, set, unit = "px") {
  const input = el("input", { type: "range", min: least, max: most, value });
  const output = el("output", {}, `${value}${unit}`);
  input.addEventListener("input", () => {
    output.textContent = `${input.value}${unit}`;
    set(Number(input.value));
  });
  return el("span", { class: "range" }, input, output);
}

function chips(options, current, set) {
  return el("div", { class: "chips" }, options.map(([value, label]) =>
    el("button", { type: "button", "aria-pressed": String(value === current), onclick: () => set(value) }, label)));
}

function toggle(checked, set, label) {
  const box = el("input", { type: "checkbox", "aria-label": label, title: label });
  box.checked = checked;
  box.addEventListener("change", () => set(box.checked));
  return box;
}

const drawer = drawingEditor({
  state, el, field, group, chips, range, toggle, choiceColour, icon, inspector,
  drawing: () => currentDesign(),
  shape: () => shape(),
  render: () => render(),
  apply: () => apply(),
  changed: (options) => changed(options),
  fits: () => fits(),
  say: (text, bad) => say(text, bad),
  remember: () => remember(),
});

function roleSelect(role, set) {
  const roles = Object.keys(state.theme.colors);
  const select = el("select", { "aria-label": "Colour" }, roles.map((name) => new Option(COLOUR_NAMES[name] ?? name, name)));
  select.value = role;
  const swatch = el("span", { class: "swatch", style: `background: ${shownColour(role)}` });
  select.addEventListener("change", () => {
    swatch.style.background = shownColour(select.value);
    set(select.value);
  });
  return el("span", { class: "color-control" }, swatch, select);
}

function boxFields(box, { ink = true } = {}) {
  const set = (key, rebuild = false) => (value) => {
    box[key] = value;
    changed({ rebuild });
  };
  const shaded = box.fill_top !== box.fill_bottom;
  const fields = [
    field("Fill", chips([["plain", "One colour"], ["shaded", "Top to bottom"]], shaded ? "shaded" : "plain", (kind) => {
      box.fill_top = kind === "shaded" ? (box.fill_top === box.fill_bottom ? "header_top" : box.fill_top) : box.fill_bottom;
      changed({ rebuild: true });
    })),
  ];
  if (shaded) {
    fields.push(field("Top", roleSelect(box.fill_top, set("fill_top"))), field("Bottom", roleSelect(box.fill_bottom, set("fill_bottom"))));
  } else {
    fields.push(field("Colour", roleSelect(box.fill_bottom, (value) => {
      box.fill_top = box.fill_bottom = value;
      changed();
    })));
  }
  if (ink) {
    fields.push(field("Text on it", roleSelect(box.ink, set("ink"))));
  }
  fields.push(field("Border", range(box.border_width ?? 0, 0, 6, (width) => {
    const was = box.border_width ?? 0;
    box.border_width = width;
    if (width > 0 && !box.border_edges) {
      box.border_edges = 15;
    }
    if (width > 0 && was === 0 && box.border === box.fill_bottom) {
      box.border = box.fill_bottom === "divider" ? "text" : "divider";
    }
    changed({ rebuild: (was > 0) !== (width > 0) });
  })));
  if ((box.border_width ?? 0) > 0) {
    const edges = el("span", { class: "chips" }, EDGE_BITS.map(([, label, bit]) => el("button", {
      type: "button",
      "aria-pressed": String(Boolean(box.border_edges & bit)),
      onclick: () => {
        box.border_edges ^= bit;
        changed({ rebuild: true });
      },
    }, label)));
    fields.push(field("Border colour", roleSelect(box.border, set("border"))), field("Border on", edges));
  }
  fields.push(field("Shine along the top", range(box.shine_height ?? 0, 0, 6, (height) => {
    const was = box.shine_height ?? 0;
    box.shine_height = height;
    changed({ rebuild: (was > 0) !== (height > 0) });
  })));
  if ((box.shine_height ?? 0) > 0) {
    fields.push(field("Shine colour", roleSelect(box.shine, set("shine"))));
  }
  fields.push(
    field("Rounded corners", range(box.radius ?? 0, 0, 32, set("radius"))),
    field("Gap at the sides", range(box.inset_across ?? 0, 0, 16, set("inset_across"))),
    field("Gap above and below", range(box.inset_down ?? 0, 0, 8, set("inset_down"))),
    field("See-through", range(100 - opacityPercent(box.see_through ?? 0), 0, 100, (shown) => {
      box.see_through = seeThroughOf(100 - shown);
      changed();
    }, "%"), "Lets the wallpaper show through"),
  );
  return fields;
}

function sampleName(sample) {
  const listed = state.gallery.find((entry) => entry.file === sample);
  if (listed?.name) {
    return listed.name;
  }
  const words = sample.replaceAll("_", " ");
  return words[0].toUpperCase() + words.slice(1);
}

function fitBytes(input, most) {
  const encoder = new TextEncoder();
  while (encoder.encode(input.value).length > most) {
    input.value = Array.from(input.value).slice(0, -1).join("");
  }
  return input.value;
}

function slug(name) {
  const tidy = name.toLowerCase().normalize("NFKD").replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
  return (tidy || "theme").slice(0, SLUG_LETTERS).replace(/-+$/, "");
}

function panelStart() {
  const name = el("input", { value: state.theme.name, maxlength: NAME_BYTES });
  name.addEventListener("input", () => {
    state.theme.name = fitBytes(name, NAME_BYTES);
    $("name").value = name.value;
    changed();
  });
  const about = el("input", { value: state.theme.description ?? "", maxlength: 120, placeholder: "A line about it" });
  about.addEventListener("input", () => {
    state.theme.description = about.value;
    remember();
  });
  const author = el("input", { value: state.theme.author ?? "", maxlength: 40, placeholder: "Your name" });
  author.addEventListener("input", () => {
    state.theme.author = author.value;
    remember();
  });
  const starts = el("div", { class: "starts" }, state.schema.samples.map((sample) =>
    el("button", { type: "button", onclick: () => openSample(sample) },
      el("span", { class: "thumb", style: `background-image: url("previews/${sample}.png")` }),
      el("span", {}, sampleName(sample)))));
  return [
    el("h2", {}, "Make a theme"),
    el("p", { class: "lead" }, "Everything you change shows on the player straight away. When you're happy, download it and copy it to the Themes folder on your card."),
    group(field("Name", name), field("Made by", author, "Shown in the gallery if you share it"),
      field("Description", about, "One line for the gallery")),
    el("h3", {}, "Begin from"),
    starts,
    el("div", { class: "actions" },
      el("button", { type: "button", onclick: () => $("picker").click() }, "Open a theme file…"),
      el("button", { type: "button", onclick: startOver }, "Start over")),
  ];
}

function panelColours() {
  const accent = shownAccentOf("selection_background");
  const groups = COLOUR_GROUPS.map(([title, roles]) => [
    el("h3", {}, title),
    title === "Selected row" && accent
      ? el("p", { class: "hint" }, `These are the “${accent.name}” accent's colours, the one the preview shows. People pick an accent on the player; change the list under Accent choices below.`)
      : null,
    group(...roles.map(([role]) => roleColour(role))),
  ].filter(Boolean));
  const cover = toggle(state.theme.cover_flow === "dark", (dark) => {
    state.theme.cover_flow = dark ? "dark" : "theme";
    changed();
  });
  const accents = el("div", { class: "legacy" });
  buildAccents(accents, state.theme, {
    changed: () => changed(),
    shown: () => state.accent,
    show: (index) => {
      state.accent = index;
      restart();
    },
  });
  return [
    el("h2", {}, "Colours"),
    el("p", { class: "lead" }, "Click something on the screen to jump to its colour."),
    noticeBox(),
    ...groups.flat(),
    el("h3", {}, "Cover Flow"),
    group(field("Keep it dark", cover, "Good for light themes: the album wall stays on black")),
    el("h3", {}, "Accent choices"),
    el("p", { class: "hint" }, "Colours people can pick for the selected row under Settings, Display, Accent color. Leave the list empty to keep yours."),
    accents,
  ];
}

function panelShapes() {
  const spacing = Object.entries(SPACINGS).map(([name, label]) => {
    const entry = state.schema.spacing.find((item) => item.name === name);
    const current = state.theme.spacing[name];
    const custom = toggle(current !== null, (on) => {
      state.theme.spacing[name] = on ? entry.least + Math.round((entry.most - entry.least) / 2) : null;
      changed({ rebuild: true });
    }, "Use my own spacing");
    const control = current === null
      ? el("span", { class: "hint" }, "Player's own")
      : range(current, entry.least, entry.most, (value) => {
        state.theme.spacing[name] = value;
        changed();
      });
    return field(label, el("span", { class: "color-control" }, control, custom));
  });
  const boxes = state.theme.boxes;
  const more = el("details", { class: "more" }, el("summary", {}, "Scrollbar and bars"),
    ...MORE_BOXES.flatMap(([name, title]) => [el("h3", {}, title), group(...boxFields(boxes[name], { ink: false }))]));
  more.open = state.open.has("shapes:more");
  more.addEventListener("toggle", () => {
    if (more.open) {
      state.open.add("shapes:more");
    } else {
      state.open.delete("shapes:more");
    }
  });
  return [
    el("h2", {}, "Lists & shapes"),
    el("p", { class: "lead" }, "The boxes the player draws its lists, title bar and messages with. Colours come from the Colours step."),
    el("h3", {}, "Selected row"),
    group(...boxFields(boxes[state.theme.selection] ?? boxes.selection)),
    el("h3", {}, "Title bar"),
    group(...boxFields(boxes.header)),
    el("h3", {}, "Messages"),
    group(...boxFields(boxes.toast)),
    el("h3", {}, "Letter bubble"),
    el("p", { class: "hint" }, "The big letter that shows while you scroll a long list."),
    group(...boxFields(boxes.section)),
    el("h3", {}, "Home"),
    state.designs.home
      ? el("p", { class: "hint" }, "Your own Home drawing decides what goes on the Home screen. Change it in the Home step.")
      : group(field("Album art beside the menu", toggle(state.theme.home_art !== false, (shown) => {
        state.theme.home_art = shown;
        changed();
      }), "Turn it off to give the menu the whole screen")),
    el("h3", {}, "Spacing"),
    group(...spacing),
    more,
  ];
}

function panelLegacy(title, lead, build) {
  const host = el("div", { class: "legacy" });
  build(host, state.theme, state.schema, () => changed());
  host.querySelectorAll("details").forEach((details, index) => {
    const key = `${state.step}:${index}`;
    details.open = state.open.has(key);
    details.addEventListener("toggle", () => {
      if (details.open) {
        state.open.add(key);
      } else {
        state.open.delete(key);
      }
    });
  });
  return [el("h2", {}, title), el("p", { class: "lead" }, lead), host];
}

function panelShare() {
  const bytes = apply();
  const memory = assetMemory(state.theme, state.schema);
  const size = bytes ? `This theme is ${bytes.length.toLocaleString("en")} bytes` : "";
  const used = memory ? `, and uses ${memory.toLocaleString("en")} of the player's ${state.schema.asset_memory.toLocaleString("en")} bytes for pictures and fonts` : "";
  return [
    el("h2", {}, "Save & share"),
    el("p", { class: "lead" }, "Put the theme on your player, or share it with everyone."),
    noticeBox(),
    el("h3", {}, "On your player"),
    el("ol", { class: "hint" },
      el("li", {}, "Download the theme file."),
      el("li", {}, "Connect the player and copy the file into a folder called Themes on the microSD card."),
      el("li", {}, "On the player, open Settings, Display, Theme.")),
    el("div", { class: "actions" },
      el("button", { type: "button", class: "primary", onclick: download }, "Download for my player"),
      el("button", { type: "button", onclick: downloadSource }, "Download source")),
    el("p", { class: "hint" }, bytes ? `${size}${used}.` : ""),
    el("h3", {}, "With everyone"),
    el("p", { class: "hint" }, "Download the source and add it to the themes folder of the EchoPod project in a pull request. Once it's merged, it appears in the gallery."),
  ];
}

function shape() {
  return state.turn === 0 ? "wide" : "tall";
}

function handWritten(step) {
  return !state.designs[step] && typeof state.theme[DESIGNS[step].key]?.script === "string";
}

function setMode(mode) {
  const step = state.step;
  const { key, screen } = DESIGNS[step];
  if (handWritten(step)) {
    state.stash[step] = state.theme[key];
  }
  state.designs[step] = mode === "design" ? state.designs[step] ?? designFrom(step) ?? standardDrawing(screen) : null;
  if (mode === "standard") {
    state.theme[key] = null;
  } else if (mode === "script") {
    state.theme[key] = state.stash[step] ?? null;
  }
  state.picked = null;
  changed({ rebuild: true });
  restart();
}

function stashColours(step) {
  const script = state.stash[step]?.script;
  return typeof script === "string" ? Object.fromEntries(scriptColours(script).map(({ name, value }) => [name, value])) : {};
}

function designFrom(step) {
  const consts = stashColours(step);
  const face = faceOf(consts);
  return face && STARTS[DESIGNS[step].screen].some(([name]) => name === face) ? startDrawing(face, DESIGNS[step].screen, consts) : null;
}

function startFrom(step, name, label) {
  const consts = stashColours(step);
  state.designs[step] = startDrawing(name, DESIGNS[step].screen, faceOf(consts) === name ? consts : {});
  state.picked = null;
  if (!fits()) {
    say("That drawing doesn't fit with the rest of this theme. Make room by removing a picture or a font.", true);
  } else {
    say(`Started again from ${label}. Undo brings back your drawing.`);
  }
  changed({ rebuild: true });
  restart();
}

function scriptColours(script) {
  return [...script.matchAll(SCRIPT_COLOUR)].map((found) => ({ name: found[1], value: found[2].toLowerCase() }));
}

function friendlyName(name) {
  const words = name.split("_").map((word) => SCRIPT_WORDS[word] ?? word.toLowerCase()).join(" ");
  return words[0].toUpperCase() + words.slice(1);
}

function setScriptColour(step, name, value) {
  const scene = state.theme[DESIGNS[step].key];
  scene.script = scene.script.replace(new RegExp(`^const ${name} = #[0-9a-fA-F]{6}`, "m"), `const ${name} = ${value}`);
  changed();
}

function scriptPanel(step) {
  const scene = state.theme[DESIGNS[step].key];
  const colours = scriptColours(scene.script).map(({ name, value }) => {
    const row = field(friendlyName(name), colourInput(value, (picked) => setScriptColour(step, name, picked)));
    row.dataset.script = name;
    return row;
  });
  const parts = [
    el("p", { class: "hint" }, "This screen comes with the theme. Change its colours here, or choose “My own drawing” to draw a screen of your own."),
  ];
  if (colours.length > 0) {
    parts.push(el("h3", {}, "Its colours"), group(...colours));
  }
  if (scene.chrome) {
    const titles = CHROME_ROLES.map(([role, label]) => {
      const row = field(label, colourInput(scene.chrome[role] ?? state.theme.colors[role], (picked) => {
        scene.chrome[role] = picked;
        changed();
      }));
      row.dataset.chrome = role;
      return row;
    });
    parts.push(el("h3", {}, "Title bar on this screen"), group(...titles));
  }
  return parts;
}

function panelDesign(step) {
  const config = DESIGNS[step];
  const design = state.designs[step];
  const scripted = handWritten(step);
  const mode = design ? "design" : scripted ? "script" : "standard";
  const modes = [["standard", "The player's own"]];
  if (scripted || state.stash[step]) {
    modes.push(["script", "This theme's own"]);
  }
  modes.push(["design", "My own drawing"]);
  const parts = [
    el("h2", {}, config.title),
    el("p", { class: "lead" }, config.lead),
    group(el("div", { class: "field stack" }, chips(modes, mode, setMode))),
  ];
  if (scripted) {
    return [...parts, ...scriptPanel(step)];
  }
  if (!design) {
    return parts;
  }
  let menu = false;
  walk(design.elements, (item) => {
    menu ||= !item.hidden && (item.kind === "menu" || item.repeat?.count === "items");
  });
  if (config.screen === "home" && !menu) {
    parts.push(el("p", { class: "notice" }, "This Home screen shows no menu. The buttons still move through Music, Settings and the rest, so add a Menu."));
  }
  parts.push(
    ...drawer.panel(config.screen),
    el("h3", {}, "Start again from"),
    el("p", { class: "hint" }, "Replaces this drawing. Undo brings it back."),
    el("div", { class: "actions" }, STARTS[config.screen].map(([name, label]) =>
      el("button", { type: "button", onclick: () => startFrom(step, name, label) }, label))),
  );
  return parts;
}

function panelFor(step) {
  switch (step) {
    case "start":
      return panelStart();
    case "colours":
      return panelColours();
    case "shapes":
      return panelShapes();
    case "home":
    case "playing":
      return panelDesign(step);
    case "wallpaper":
      return panelLegacy("Wallpaper", "A picture behind the lists. It's cut to 16 colours; fade it until the text reads well.", buildWallpapers);
    case "fonts":
      return panelLegacy("Fonts", "Your own lettering for titles, lists and small text. Only share fonts whose licence allows it.", buildFonts);
    case "icons":
      return panelLegacy("Icons", "The small pictures in the title bar and the lists. Drag to draw and right-drag to erase; the theme's colours tint them.", buildIcons);
    default:
      return panelShare();
  }
}

function renderSteps() {
  $("steps").replaceChildren(...STEPS.map((step) => el("button", {
    type: "button",
    "aria-current": step.id === state.step ? "step" : null,
    title: step.label,
    onclick: () => goTo(step.id),
  }, icon(step.icon), el("span", {}, step.label))));
}

function renderScreens() {
  $("screens").replaceChildren(...SCREENS.map(([id, label]) => el("button", {
    type: "button",
    role: "tab",
    "aria-selected": String(id === state.screen),
    onclick: () => {
      state.screen = id;
      restart();
      render();
    },
  }, label)));
  for (const button of $("turn").querySelectorAll("button")) {
    button.setAttribute("aria-pressed", String(Number(button.dataset.turn) === state.turn));
  }
}

function render() {
  renderSteps();
  renderScreens();
  $("name").value = state.theme.name;
  inspector.replaceChildren(...panelFor(state.step));
  $("hint").textContent = designing()
    ? "Drag to move, drag the corner to resize. Arrow keys nudge the picked part."
    : "Click anything on the screen to change its colour.";
  overlay.classList.toggle("designing", designing());
  drawOverlay();
}

function goTo(step) {
  state.step = step;
  state.picked = null;
  const screen = DESIGNS[step]?.screen;
  if (screen && state.screen !== screen) {
    state.screen = screen;
    restart();
  }
  render();
  inspector.scrollTop = 0;
}

function designing() {
  const config = DESIGNS[state.step];
  return Boolean(config && state.designs[state.step] && state.screen === config.screen);
}

function restart() {
  state.accent = shownAccent(state.theme, state.accent);
  state.preview.start(state.turn, state.screen, state.accent);
  apply();
  for (let spent = 0; state.preview.busy && spent < SKIP_LIMIT_MS; spent += SKIP_STEP_MS) {
    state.preview.advance(SKIP_STEP_MS);
  }
  state.preview.advance(SKIP_SETTLE_MS);
  state.lastFrame = null;
}

function fitZoom(width, height) {
  const stage = document.querySelector(".device-wrap");
  const room = Math.min((stage.clientWidth - 92) / width, (stage.clientHeight - 92) / height);
  return Math.max(1, Math.min(8, Math.floor(room * ZOOM_STEPS) / ZOOM_STEPS));
}

function setZoom(choice) {
  const { width, height } = state.preview.frame();
  const fitted = fitZoom(width, height);
  const current = state.zoomChoice ?? fitted;
  if (choice === "fit") {
    state.zoomChoice = null;
  } else {
    state.zoomChoice = Math.max(1, Math.min(8, choice === "in" ? Math.floor(current) + 1 : Math.ceil(current) - 1));
  }
  $("zoom-level").textContent = state.zoomChoice === null ? "Fit" : `${state.zoomChoice}×`;
}

function fit(width, height) {
  state.zoom = state.zoomChoice ?? fitZoom(width, height);
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
  canvas.style.width = `${width * state.zoom}px`;
  canvas.style.height = `${height * state.zoom}px`;
}

function frame(now) {
  state.preview.advance(Math.min(Math.round(now - (state.lastFrame ?? now)), 1000));
  state.lastFrame = now;
  const { width, height, pixels } = state.preview.frame();
  fit(width, height);
  context.putImageData(new ImageData(pixels, width, height), 0, 0);
  drawOverlay();
  requestAnimationFrame(frame);
}

function drawOverlay() {
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (overlay.width !== width || overlay.height !== height) {
    overlay.width = width;
    overlay.height = height;
  }
  const pen = overlay.getContext("2d");
  pen.clearRect(0, 0, width, height);
  if (!designing()) {
    return;
  }
  drawer.overlay(pen, state.zoom);
}

function pointer(event) {
  const box = overlay.getBoundingClientRect();
  return { x: (event.clientX - box.left) / state.zoom, y: (event.clientY - box.top) / state.zoom };
}

function shownScript() {
  const step = Object.keys(DESIGNS).find((name) => DESIGNS[name].screen === state.screen);
  return step && handWritten(step) ? step : null;
}

function scriptRoles(point) {
  const step = shownScript();
  const scene = step ? state.theme[DESIGNS[step].key] : null;
  if (!scene) {
    return [];
  }
  if (point.y < AREAS[shape()].top) {
    return scene.chrome ? CHROME_ROLES.map(([role]) => [`chrome:${step}:${role}`, scene.chrome[role]]) : [];
  }
  return scriptColours(scene.script).map(({ name, value }) => [`script:${step}:${name}`, value]);
}

function roleAt(point) {
  const own = scriptRoles(point);
  const theme = Object.keys(state.theme.colors).map((role) => [role, shownColour(role)]);
  return (own.length > 0 && closestRole(point, own)) || closestRole(point, theme);
}

function channels(value) {
  const number = parseInt(value.slice(1), 16);
  return [number >> 16, (number >> 8) & 255, number & 255];
}

function gapBetween(first, second) {
  return first.reduce((sum, part, index) => sum + Math.abs(part - second[index]), 0);
}

function blendOf(shade, candidates) {
  let best = null;
  let distance = Infinity;
  for (const [role, value] of candidates) {
    for (const [, other] of candidates) {
      const [from, to] = [channels(value), channels(other)];
      const span = gapBetween(from, to);
      const along = gapBetween(from, shade) + gapBetween(shade, to) - span;
      const near = gapBetween(from, shade);
      if (span > GRADIENT_LEAST && along <= GRADIENT_SLACK && near <= span / 2 && near < distance) {
        distance = near;
        best = role;
      }
    }
  }
  return best;
}

function closestRole(point, candidates) {
  const { width, pixels } = state.preview.frame();
  const index = (Math.floor(point.y) * width + Math.floor(point.x)) * 4;
  const shade = [pixels[index], pixels[index + 1], pixels[index + 2]];
  let best = null;
  let distance = Infinity;
  for (const [role, value] of candidates) {
    const gap = gapBetween(channels(value), shade);
    if (gap < distance) {
      distance = gap;
      best = role;
    }
  }
  return distance <= COLOUR_SLACK ? best : blendOf(shade, candidates);
}

function jumpToRole(role) {
  const [kind, step, name] = role.split(":");
  const own = kind === "script" || kind === "chrome";
  state.step = own ? step : "colours";
  render();
  const row = own
    ? inspector.querySelector(`[data-${kind}="${name}"]`)
    : inspector.querySelector(`[data-role="${role}"]`);
  if (row) {
    row.scrollIntoView({ block: "center", behavior: "smooth" });
    row.classList.add("flash");
  }
}

function wireOverlay() {
  overlay.addEventListener("pointerdown", (event) => {
    const point = pointer(event);
    if (!designing()) {
      event.preventDefault();
      const role = roleAt(point);
      if (role) {
        jumpToRole(role);
      } else {
        say("Couldn't tell which colour that is. It may be part of a picture, such as the wallpaper or a cover.");
      }
      canvas.focus({ preventScroll: true });
      return;
    }
    if (drawer.down(point)) {
      overlay.setPointerCapture(event.pointerId);
    }
  });
  overlay.addEventListener("dblclick", (event) => {
    if (designing()) {
      drawer.deeper(pointer(event));
    }
  });
  overlay.addEventListener("pointermove", (event) => {
    if (designing()) {
      const cursor = drawer.move(pointer(event));
      if (cursor) {
        overlay.style.cursor = cursor;
      }
    }
  });
  const endDrag = () => drawer.up();
  overlay.addEventListener("pointerup", endDrag);
  overlay.addEventListener("pointercancel", endDrag);
  overlay.addEventListener("lostpointercapture", endDrag);
}

function nudge(event) {
  const steps = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
  if (!designing() || !(event.key in steps) || event.target.closest("input, select, textarea, canvas")) {
    return false;
  }
  const [dx, dy] = steps[event.key];
  const scale = event.shiftKey ? 10 : 1;
  if (!drawer.nudge(dx * scale, dy * scale)) {
    return false;
  }
  event.preventDefault();
  return true;
}

function download() {
  const bytes = apply();
  if (bytes) {
    save(`${slug(state.theme.name)}.echotheme`, bytes, "application/octet-stream");
  }
}

function downloadSource() {
  syncScene();
  save(`${slug(state.theme.name)}.toml`, toToml(state.theme, state.schema), "text/plain");
}

function save(name, bytes, type) {
  const link = el("a", { href: URL.createObjectURL(new Blob([bytes], { type })), download: name });
  link.click();
  URL.revokeObjectURL(link.href);
}

function wear(bytes) {
  settleHistory();
  try {
    const theme = unpack(bytes, state.schema);
    state.theme = theme;
    state.designs = {
      playing: theme.now_playing_scene?.design ? drawingFrom(theme.now_playing_scene.design, "playing") : null,
      home: theme.home_scene?.design ? drawingFrom(theme.home_scene.design, "home") : null,
    };
    state.stash = {};
    state.picked = null;
    restart();
    remember();
    render();
    return true;
  } catch (error) {
    if (!(error instanceof ThemeError)) {
      throw error;
    }
    say(error.message, true);
    return false;
  }
}

async function fetchBytes(path) {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`${path}: ${response.status}`);
  }
  return new Uint8Array(await response.arrayBuffer());
}

async function openSample(sample, kept = false) {
  let bytes;
  try {
    bytes = await fetchBytes(`themes/${sample}.echotheme`);
  } catch {
    say(`${sampleName(sample)} couldn't be opened. Check your connection and try again.`, true);
    return;
  }
  if (wear(bytes)) {
    say(kept ? `Opened ${sampleName(sample)}. Undo brings back the theme you were making.` : `Started from ${sampleName(sample)}`);
  }
}

function startOver() {
  state.theme = defaultTheme(state.schema);
  state.designs = { playing: null, home: null };
  state.stash = {};
  state.picked = null;
  restart();
  changed({ rebuild: true });
}

function wireControls() {
  $("name").addEventListener("input", (event) => {
    state.theme.name = event.target.value;
    changed();
  });
  $("save").addEventListener("click", download);
  $("undo").addEventListener("click", () => travel(true));
  $("redo").addEventListener("click", () => travel(false));
  $("picker").addEventListener("change", async () => {
    const picker = $("picker");
    if (picker.files.length > 0) {
      wear(new Uint8Array(await picker.files[0].arrayBuffer()));
      picker.value = "";
    }
  });
  for (const button of $("turn").querySelectorAll("button")) {
    button.addEventListener("click", () => {
      state.turn = Number(button.dataset.turn);
      restart();
      render();
    });
  }
  for (const button of $("zoom").querySelectorAll("button")) {
    button.addEventListener("click", () => setZoom(button.dataset.zoom));
  }
  for (const button of $("keys").querySelectorAll("button")) {
    const key = Number(button.dataset.key);
    button.addEventListener("pointerdown", () => state.preview.key(key, true));
    button.addEventListener("pointerup", () => state.preview.key(key, false));
    button.addEventListener("pointerleave", () => state.preview.key(key, false));
  }
  const keyboard = { ArrowDown: KEY.down, ArrowUp: KEY.up, Enter: KEY.play, " ": KEY.play, Backspace: KEY.back, Escape: KEY.back, m: KEY.menu };
  canvas.addEventListener("keydown", (event) => {
    if (event.key in keyboard) {
      event.preventDefault();
      if (!event.repeat) {
        state.preview.key(keyboard[event.key], true);
      }
    }
  });
  canvas.addEventListener("keyup", (event) => {
    if (event.key in keyboard) {
      state.preview.key(keyboard[event.key], false);
    }
  });
  document.addEventListener("keydown", (event) => {
    if (nudge(event)) {
      return;
    }
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "z" && !event.target.closest("input, textarea")) {
      event.preventDefault();
      travel(!event.shiftKey);
    }
  });
  window.addEventListener("resize", drawOverlay);
}

async function main() {
  state.schema = await (await fetch("theme_schema.json")).json();
  state.preview = await Preview.load(fetchBytes, state.schema);
  try {
    state.gallery = await (await fetch("themes/index.json")).json();
  } catch {
    state.gallery = [];
  }
  state.theme = defaultTheme(state.schema);
  const asked = new URLSearchParams(location.search).get("theme");
  const draft = loadDraft();
  if (draft) {
    try {
      restore(draft);
      syncScene();
      pack(state.theme, state.schema);
    } catch {
      state.theme = defaultTheme(state.schema);
      state.designs = { playing: null, home: null };
      clearDraft();
    }
  }
  wireControls();
  wireOverlay();
  restart();
  state.history = [snapshot()];
  render();
  refreshUndo();
  if (asked !== null) {
    history.replaceState(null, "", location.pathname);
    if (state.schema.samples.includes(asked)) {
      await openSample(asked, Boolean(draft));
    } else {
      say(`There's no theme called “${asked}” here, so your own theme is open.`, true);
    }
  }
  requestAnimationFrame(frame);
}

main().catch((error) => say(`The editor couldn't start: ${error.message}`, true));
