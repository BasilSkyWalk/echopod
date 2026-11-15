import { buildAccents, shownAccent } from "./accent_editor.js";
import { buildFonts } from "./font_editor.js";
import { buildIcons } from "./icon_editor.js";
import {
  AREAS,
  PART_COLOURS,
  designFromScript,
  isPart,
  partSize,
  sizedFrame,
  BACKGROUNDS,
  MENU_STYLES,
  colorOf,
  defaultDesign,
  designScript,
  layerTypes,
  newLayer,
  readDesign,
  textSources,
} from "./now_playing_designer.js";
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
    lead: "Lay out the first screen however you like: the menu, the album art, what's playing. Drag things on the screen to move them, and drag a corner to resize. The buttons still move through the menu from top to bottom.",
  },
  playing: {
    screen: "playing",
    key: "now_playing_scene",
    title: "Now Playing",
    lead: "Arrange the Now Playing screen however you like: drag things on the screen to move them, and drag a corner to resize.",
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
const SELECTION_STYLES = [
  ["gradient", "Gradient"], ["solid", "Solid"], ["outline", "Outline"], ["underline", "Underline"], ["bar", "Side bar"],
];
const SPACINGS = {
  row_height: "Row height", padding: "Side margin", scrollbar_width: "Scrollbar width",
  scrollbar_inset: "Scrollbar gap", detail_gap: "Gap before values",
};
const HISTORY_LIMIT = 80;
const SETTLE_MS = 350;
const DRAFT = "echopod-theme-draft";
const HANDLE = 7;
const NAME_BYTES = 23;
const SLUG_LETTERS = 23;
const MERGED_PARTS = ["colors", "boxes", "spacing", "icons", "wallpapers", "fonts"];
const SKIP_STEP_MS = 100;
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
  drag: null,
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

function revived(design) {
  return design ? readDesign(JSON.stringify(design)) : null;
}

function restore(text) {
  const saved = JSON.parse(text);
  state.theme = mergedTheme(saved.theme ?? {});
  const designs = saved.designs ?? { playing: saved.design ?? null, home: null };
  state.designs = { playing: revived(designs.playing), home: revived(designs.home) };
  state.picked = currentDesign()?.layers?.some((item) => item.id === state.picked) ? state.picked : null;
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
      state.theme[key] = { script: designScript(design, state.theme), chrome: null, design: JSON.stringify(design) };
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

function selectStyle() {
  if (state.theme.selection === "selection_flat") {
    return "solid";
  }
  if (state.theme.selection === "selection_outline") {
    return "outline";
  }
  const box = state.theme.boxes.selection;
  if (box.border_width > 0 && box.border_edges === 2) {
    return "underline";
  }
  if (box.border_width > 0 && box.border_edges === 4) {
    return "bar";
  }
  return "gradient";
}

function setSelectStyle(style) {
  const boxes = state.theme.boxes;
  const radius = boxes.selection.radius ?? 0;
  const see_through = boxes.selection.see_through ?? 0;
  const base = { border: "background", shine: "background", border_width: 0, border_edges: 0, shine_height: 0, inset_across: 0, inset_down: 0, radius, see_through };
  state.theme.selection = style === "solid" ? "selection_flat" : style === "outline" ? "selection_outline" : "selection";
  if (style === "gradient") {
    boxes.selection = { ...base, fill_top: "selection_top", fill_bottom: "selection_background", shine: "selection_shine", shine_height: 1, ink: "selection_text" };
  } else if (style === "underline") {
    boxes.selection = { ...base, fill_top: "background", fill_bottom: "background", border: "selection_top", border_width: 2, border_edges: 2, ink: "text" };
  } else if (style === "bar") {
    boxes.selection = { ...base, fill_top: "background", fill_bottom: "background", border: "selection_top", border_width: 3, border_edges: 4, ink: "text" };
  }
  changed({ rebuild: true });
}

function setRoundness(radius) {
  for (const name of ["selection", "selection_flat", "selection_outline", "toast", "section"]) {
    state.theme.boxes[name].radius = radius;
  }
  changed();
}

const SEE_THROUGH = {
  row: ["selection", "selection_flat", "selection_outline"],
  title: ["header"],
  bubbles: ["toast", "section"],
};

function seeThrough(part) {
  const box = state.theme.boxes[SEE_THROUGH[part][0]];
  const control = range(100 - opacityPercent(box.see_through ?? 0), 0, 100, (shown) => {
    for (const name of SEE_THROUGH[part]) {
      state.theme.boxes[name].see_through = seeThroughOf(100 - shown);
    }
    changed();
  }, "%");
  return field("See-through", control, "Lets the wallpaper show through");
}

function titleStyle() {
  const header = state.theme.boxes.header;
  return header.fill_top === header.fill_bottom ? "flat" : "gradient";
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
  const header = state.theme.boxes.header;
  const lineUnder = toggle(header.border_width > 0, (on) => {
    header.border_width = on ? 1 : 0;
    header.border_edges = on ? 2 : 0;
    changed();
  });
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
  return [
    el("h2", {}, "Lists & shapes"),
    el("p", { class: "lead" }, "How the selected row, the title bar and the lists are drawn."),
    el("h3", {}, "Selected row"),
    group(
      el("div", { class: "field stack" }, chips(SELECTION_STYLES, selectStyle(), setSelectStyle)),
      field("Rounded corners", range(state.theme.boxes.selection.radius ?? 0, 0, 14, setRoundness)),
      seeThrough("row"),
    ),
    el("h3", {}, "Title bar"),
    group(
      el("div", { class: "field stack" }, chips([["gradient", "Gradient"], ["flat", "Flat"]], titleStyle(), (style) => {
        header.fill_top = style === "flat" ? "header_background" : "header_top";
        changed({ rebuild: true });
      })),
      field("Line under it", lineUnder),
      seeThrough("title"),
    ),
    el("h3", {}, "Messages and the letter bubble"),
    group(seeThrough("bubbles")),
    el("h3", {}, "Home"),
    state.designs.home
      ? el("p", { class: "hint" }, "Your own Home design decides what goes on the Home screen. Change it in the Home step.")
      : group(field("Album art beside the menu", toggle(state.theme.home_art !== false, (shown) => {
        state.theme.home_art = shown;
        changed();
      }), "Turn it off to give the menu the whole screen")),
    el("h3", {}, "Spacing"),
    group(...spacing),
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

function layerName(item) {
  if (item.type === "text") {
    return item.source === "custom" ? `“${item.custom ?? ""}”` : textSources(designScreen())[item.source] ?? "Text";
  }
  if (item.type === "menu") {
    return `Menu: ${(MENU_STYLES[item.style] ?? "List").toLowerCase()}`;
  }
  return layerTypes(designScreen())[item.type] ?? item.type;
}

function shape() {
  return state.turn === 0 ? "wide" : "tall";
}

function pickedLayer() {
  return currentDesign()?.layers.find((item) => item.id === state.picked) ?? null;
}

function eyePath(hidden) {
  return hidden
    ? "M3 10s2.5-5 7-5 7 5 7 5-2.5 5-7 5-7-5-7-5zM4 4l12 12"
    : "M3 10s2.5-5 7-5 7 5 7 5-2.5 5-7 5-7-5-7-5zM10 12a2 2 0 1 0 0-4 2 2 0 0 0 0 4";
}

function layerList() {
  const layers = currentDesign().layers;
  const tool = (title, path, disabled, act) => el("button", {
    type: "button",
    title,
    "aria-label": title,
    disabled,
    onclick: (event) => {
      event.stopPropagation();
      act();
    },
  }, icon(path));
  return el("ul", { class: "layers group", role: "listbox", "aria-label": "Layers, back to front" },
    layers.map((item, index) => el("li", {
      role: "option",
      "aria-selected": String(item.id === state.picked),
      class: item.hidden ? "hidden-layer" : "",
      onclick: () => {
        state.picked = item.id;
        render();
      },
    },
    el("span", {}, layerName(item)),
    el("span", { class: "tools" },
      tool("Move back", "m6 8 4 4 4-4", index === 0, () => moveLayer(index, -1)),
      tool("Move forward", "m6 12 4-4 4 4", index === layers.length - 1, () => moveLayer(index, 1)),
      tool(item.hidden ? "Show" : "Hide", eyePath(item.hidden), false, () => {
        item.hidden = !item.hidden;
        changed({ rebuild: true });
      }),
      tool("Remove", "M5 6h10M8 6V4.5h4V6m-6 0 .7 9.5h6.6L14 6", false, () => {
        layers.splice(index, 1);
        state.picked = null;
        changed({ rebuild: true });
      })))));
}

function moveLayer(index, step) {
  const layers = currentDesign().layers;
  const [item] = layers.splice(index, 1);
  layers.splice(index + step, 0, item);
  changed({ rebuild: true });
}

function missingField(item) {
  const other = shape() === "wide" ? "tall" : "wide";
  const here = shape() === "wide" ? "sideways" : "upright";
  return el("div", { class: "field stack" },
    el("span", {}, `Not on the ${here} screen`),
    el("button", { type: "button", onclick: () => {
      const area = AREAS[shape()];
      const source = item.frames[other] ?? { x: 0, y: 0, w: 60, h: 30 };
      const w = Math.min(source.w, area.width);
      const h = Math.min(source.h, area.height);
      item.frames[shape()] = sizedFrame(item, { x: Math.round((area.width - w) / 2), y: Math.round((area.height - h) / 2), w, h }, shape());
      changed({ rebuild: true });
    } }, `Show it on the ${here} screen too`));
}

function placeField(item) {
  const box = item.frames[shape()];
  if (!box) {
    return missingField(item);
  }
  const fixed = partSize(item, shape());
  const number = (key, label) => {
    const input = el("input", { type: "number", value: Math.round(box[key]), style: "width:58px" });
    input.addEventListener("change", () => {
      box[key] = Math.max(key === "w" || key === "h" ? 1 : -400, Math.round(Number(input.value) || 0));
      changed();
    });
    return el("span", { class: "color-control" }, label, input);
  };
  if (fixed) {
    return el("div", { class: "field stack" }, el("span", {}, "Place", el("small", {}, "This part keeps its own size")),
      el("span", { class: "color-control" }, number("x", "Left"), number("y", "Top")));
  }
  return el("div", { class: "field stack" }, el("span", {}, "Place and size"),
    el("span", { class: "color-control" }, number("x", "Left"), number("y", "Top")),
    el("span", { class: "color-control" }, number("w", "Width"), number("h", "Height")));
}

function partFields(item, set, setAndShow) {
  const fields = [];
  if (item.type === "vu") {
    const needles = [["left", "Left"], ["right", "Right"], ["both", "Both"]];
    const key = shape() === "tall" ? "tallChannel" : "channel";
    fields.push(field("Needle", chips(needles, item[key] ?? item.channel ?? "left", setAndShow(key)), "Both puts left and right on one meter"));
  }
  if (item.type === "cassette") {
    const key = shape() === "tall" ? "tallSize" : "size";
    const current = shape() === "tall" ? item.tallSize ?? item.size ?? "small" : item.size ?? "large";
    fields.push(field("Size", chips([["large", "Large"], ["small", "Small"]], current, (size) => {
      item[key] = size;
      item.frames[shape()] = sizedFrame(item, item.frames[shape()], shape());
      changed({ rebuild: true });
    }), shape() === "tall" ? "Large is wider than the upright screen" : null));
    if (current === "large") {
      fields.push(field("Song on the label", toggle(item.writing !== false, setAndShow("writing"))));
    }
  }
  item.colours ??= {};
  for (const [key, label, value] of PART_COLOURS[item.type]) {
    fields.push(field(label, choiceColour(item.colours[key] ?? value, (picked) => {
      item.colours[key] = picked;
      changed();
    })));
  }
  return fields;
}

function layerFields(item) {
  const set = (key) => (value) => {
    item[key] = value;
    changed();
  };
  const setAndShow = (key) => (value) => {
    item[key] = value;
    changed({ rebuild: true });
  };
  const fields = [];
  const home = designScreen() === "home";
  if (isPart(item.type)) {
    fields.push(...partFields(item, set, setAndShow));
  } else if (item.type === "menu") {
    fields.push(...menuFields(item, set, setAndShow));
  } else if (item.type === "cover") {
    fields.push(field("Shape", chips([["square", "Square"], ["circle", "Round"]], item.shape, setAndShow("shape"))));
    if (item.shape !== "circle") {
      fields.push(field("Reflection", toggle(Boolean(item.reflection), set("reflection")), "Make the frame taller than it is wide to show more of it"));
    }
    fields.push(field("Frame", range(item.border ?? 0, 0, 8, set("border"))));
    fields.push(field("Frame colour", choiceColour(item.borderColor, set("borderColor"))));
    fields.push(field("Without a cover", choiceColour(item.placeholder, set("placeholder")),
      home ? "Shown while there's no album art to show" : "Shown when a song has no cover"));
  } else if (item.type === "text") {
    const source = el("select", {}, Object.entries(textSources(designScreen())).map(([value, label]) => new Option(label, value)));
    source.value = item.source;
    source.addEventListener("change", () => setAndShow("source")(source.value));
    fields.push(field("Shows", source));
    if (item.source === "custom") {
      const words = el("input", { value: item.custom ?? "", maxlength: 40 });
      words.addEventListener("input", () => {
        set("custom")(words.value);
        const named = inspector.querySelector('.layers [aria-selected="true"] span');
        if (named) {
          named.textContent = layerName(item);
        }
      });
      fields.push(field("Words", words));
    }
    fields.push(field("Size", chips([["large", "Large"], ["medium", "Medium"], ["small", "Small"]], item.size, setAndShow("size"))));
    const alignKey = shape() === "tall" ? "tallAlign" : "align";
    fields.push(field("Line up", chips([["left", "Left"], ["center", "Centre"], ["right", "Right"]], item[alignKey] ?? item.align, setAndShow(alignKey))));
    fields.push(field("Colour", choiceColour(item.color, set("color"))));
    fields.push(field("Scroll long text", toggle(item.scroll, set("scroll"))));
    if (!home) {
      fields.push(field("Seek and volume here", toggle(item.feedback, set("feedback")), "Shows “Volume 20” and the seek hint in this place"));
    }
  } else if (item.type === "times") {
    fields.push(field("Size", chips([["medium", "Medium"], ["small", "Small"]], item.size, setAndShow("size"))));
    fields.push(field("Time played", choiceColour(item.color, set("color"))));
    fields.push(field("Time left", choiceColour(item.color2, set("color2"))));
  } else if (item.type === "progress") {
    fields.push(field("Style", chips([["bar", "Bar"], ["dots", "Dots"], ["ring", "Ring of lights"]], item.style, setAndShow("style"))));
    if (item.style === "ring") {
      fields.push(field("Light length", range(item.thickness ?? 6, 1, 20, set("thickness"))));
    }
    fields.push(field("Played", choiceColour(item.color, set("color"))));
    fields.push(field("Still to play", choiceColour(item.track, set("track"))));
  } else if (item.type === "shape") {
    fields.push(field("Shape", chips([["rect", "Box"], ["rounded", "Rounded"], ["circle", "Circle"], ["ring", "Ring"], ["line", "Line"]], item.kind, setAndShow("kind"))));
    if (item.kind === "rounded") {
      fields.push(field("Corners", range(item.radius ?? 6, 0, 40, set("radius"))));
    }
    if (item.kind === "ring" || item.kind === "line") {
      fields.push(field("Thickness", range(item.thickness ?? 2, 1, 20, set("thickness"))));
    }
    fields.push(field("Colour", choiceColour(item.color, set("color"))));
  } else if (item.type === "playmark") {
    fields.push(field("Colour", choiceColour(item.color, set("color")), "Pause while paused, and the skip and play marks"));
  } else if (item.type === "spinner") {
    fields.push(field("Speed", range(item.speed ?? 3, 1, 16, set("speed"), "")));
    fields.push(field("Mark length", range(item.length ?? 4, 1, 30, set("length"))));
    fields.push(field("Colour", choiceColour(item.color, set("color"))));
  }
  fields.push(placeField(item));
  return fields;
}

function menuFields(item, set, setAndShow) {
  const fields = [
    field("Style", chips(Object.entries(MENU_STYLES), item.style ?? "list", setAndShow("style"))),
  ];
  if ((item.style ?? "list") === "list") {
    fields.push(el("p", { class: "hint" }, "Drawn like the theme's other lists: its row height, selected row and arrows. Change those under Lists & shapes."));
    return fields;
  }
  const sizes = [["large", "Large"], ["medium", "Medium"], ["small", "Small"]];
  if (item.style === "tiles") {
    fields.push(field("Across", range(item.columns ?? 2, 1, 6, set("columns"), "")));
    fields.push(field("Tile height", range(item.tileHeight ?? 40, 16, 120, set("tileHeight"))));
    fields.push(field("Gap", range(item.gap ?? 6, 0, 20, set("gap"))));
  } else {
    fields.push(field("Box behind it", toggle(item.boxed !== false, setAndShow("boxed"))));
  }
  fields.push(field("Corners", range(item.radius ?? 8, 0, 30, set("radius"))));
  fields.push(field("Text size", chips(sizes, item.size ?? "medium", setAndShow("size"))));
  fields.push(field("Line up", chips([["left", "Left"], ["center", "Centre"], ["right", "Right"]], item.align ?? "center", setAndShow("align"))));
  if (item.style === "tiles" || item.boxed !== false) {
    fields.push(field(item.style === "tiles" ? "Tiles" : "Box", choiceColour(item.tile, set("tile"))));
    fields.push(field("Highlighted", choiceColour(item.tileSelected, set("tileSelected"))));
  }
  fields.push(field("Text", choiceColour(item.color, set("color")), item.style === "focus" ? "The items before and after" : null));
  fields.push(field("Highlighted text", choiceColour(item.colorSelected, set("colorSelected"))));
  return fields;
}

function backgroundFields() {
  const background = currentDesign().background;
  const colour = (key) => choiceColour(background[key], (value) => {
    background[key] = value;
    changed();
  });
  const fields = [field("Fill", chips(Object.entries(BACKGROUNDS), background.style, (style) => {
    background.style = style;
    changed({ rebuild: true });
  }))];
  if (background.style === "gradient") {
    fields.push(field("Top", colour("top")), field("Bottom", colour("bottom")));
  } else if (background.style === "wallpaper") {
    fields.push(el("p", { class: "hint" }, "The theme's wallpaper, or its background colour when it has none."));
  } else {
    fields.push(field("Colour", colour("color")));
  }
  return fields;
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
  state.designs[step] = mode === "design" ? state.designs[step] ?? designFrom(step) ?? defaultDesign(screen) : null;
  if (mode === "standard") {
    state.theme[key] = null;
  } else if (mode === "script") {
    state.theme[key] = state.stash[step] ?? null;
  }
  state.picked = null;
  changed({ rebuild: true });
  restart();
}

function designFrom(step) {
  const script = state.stash[step]?.script;
  if (typeof script !== "string") {
    return null;
  }
  const consts = Object.fromEntries(scriptColours(script).map(({ name, value }) => [name, value]));
  return designFromScript(script, consts);
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
    el("p", { class: "hint" }, "This screen comes with the theme. Change its colours here, or choose “My own design” to arrange a screen of your own."),
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

function copyOtherSide() {
  const from = shape() === "wide" ? "tall" : "wide";
  const [a, b] = [AREAS[from], AREAS[shape()]];
  const scale = Math.min(b.width / a.width, b.height / a.height);
  const across = (b.width - a.width * scale) / 2;
  const down = (b.height - a.height * scale) / 2;
  for (const item of currentDesign().layers) {
    const box = item.frames[from];
    if (!box) {
      continue;
    }
    item.frames[shape()] = sizedFrame(item, {
      x: Math.round(box.x * scale + across),
      y: Math.round(box.y * scale + down),
      w: Math.max(1, Math.round(box.w * scale)),
      h: Math.max(1, Math.round(box.h * scale)),
    }, shape());
  }
  changed({ rebuild: true });
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
  modes.push(["design", "My own design"]);
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
  if (config.screen === "home" && !design.layers.some((item) => item.type === "menu" && !item.hidden)) {
    parts.push(el("p", { class: "notice" }, "This Home screen shows no menu. The buttons still move through Music, Settings and the rest, so add a Menu layer."));
  }
  const picked = pickedLayer();
  const other = shape() === "wide" ? "upright" : "sideways";
  parts.push(
    el("p", { class: "hint" }, `You're arranging the ${shape() === "wide" ? "sideways" : "upright"} screen. Turn the player above to arrange the ${other} one.`),
    el("div", { class: "actions" }, el("button", { type: "button", onclick: copyOtherSide }, `Copy the ${other} arrangement`)),
    el("h3", {}, "Layers"),
    layerList(),
    el("div", { class: "add-menu" }, Object.entries(layerTypes(config.screen)).map(([type, label]) =>
      el("button", { type: "button", onclick: () => {
        const item = newLayer(type, config.screen);
        design.layers.push(item);
        if (!fits()) {
          design.layers.pop();
          syncScene();
          say("There's no room for another layer on this screen. Remove one first.", true);
          return;
        }
        state.picked = item.id;
        changed({ rebuild: true });
      } }, `+ ${label}`))),
  );
  if (picked) {
    parts.push(el("h3", {}, layerName(picked)), group(...layerFields(picked)));
  } else {
    parts.push(el("h3", {}, "Background"), group(...backgroundFields()));
  }
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
    ? "Drag to move, drag the corner to resize. Arrow keys nudge the picked layer."
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
  return Math.max(1, Math.min(8, Math.floor(room)));
}

function setZoom(choice) {
  const { width, height } = state.preview.frame();
  const fitted = fitZoom(width, height);
  const current = state.zoomChoice ?? fitted;
  if (choice === "fit") {
    state.zoomChoice = null;
  } else {
    state.zoomChoice = Math.max(1, Math.min(8, current + (choice === "in" ? 1 : -1)));
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

function areaBox(item) {
  const area = AREAS[shape()];
  const box = item.frames[shape()];
  return { x: box.x, y: box.y + area.top, w: box.w, h: box.h };
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
  const zoom = state.zoom;
  for (const item of currentDesign().layers) {
    if (item.hidden || !item.frames[shape()]) {
      continue;
    }
    const box = areaBox(item);
    const picked = item.id === state.picked;
    pen.setLineDash(picked ? [] : [4, 3]);
    pen.lineWidth = picked ? 1.5 : 1;
    pen.strokeStyle = picked ? "#ffffff" : "rgba(255, 255, 255, 0.35)";
    pen.strokeRect(box.x * zoom + 0.5, box.y * zoom + 0.5, box.w * zoom - 1, box.h * zoom - 1);
    if (picked && !partSize(item, shape())) {
      pen.setLineDash([]);
      const hx = (box.x + box.w) * zoom - HANDLE / 2;
      const hy = (box.y + box.h) * zoom - HANDLE / 2;
      pen.fillStyle = "#ffffff";
      pen.fillRect(hx, hy, HANDLE, HANDLE);
      pen.strokeStyle = "#1c1b19";
      pen.strokeRect(hx + 0.5, hy + 0.5, HANDLE - 1, HANDLE - 1);
    }
  }
}

function pointer(event) {
  const box = overlay.getBoundingClientRect();
  return { x: (event.clientX - box.left) / state.zoom, y: (event.clientY - box.top) / state.zoom };
}

function layerAt(point) {
  const layers = currentDesign().layers;
  for (let index = layers.length - 1; index >= 0; index--) {
    const item = layers[index];
    if (item.hidden || !item.frames[shape()]) {
      continue;
    }
    const box = areaBox(item);
    if (point.x >= box.x && point.x < box.x + box.w && point.y >= box.y && point.y < box.y + box.h) {
      return item;
    }
  }
  return null;
}

function nearHandle(item, point) {
  if (partSize(item, shape()) || !item.frames[shape()]) {
    return false;
  }
  const box = areaBox(item);
  const reach = HANDLE / state.zoom + 1;
  return Math.abs(point.x - (box.x + box.w)) <= reach && Math.abs(point.y - (box.y + box.h)) <= reach;
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
    const current = pickedLayer();
    const resize = Boolean(current && nearHandle(current, point));
    const item = resize ? current : layerAt(point);
    if (!item) {
      state.picked = null;
      render();
      return;
    }
    overlay.setPointerCapture(event.pointerId);
    state.picked = item.id;
    state.drag = { item, resize, from: point, start: { ...item.frames[shape()] } };
    render();
  });
  overlay.addEventListener("pointermove", (event) => {
    const point = pointer(event);
    if (!state.drag) {
      if (designing()) {
        const current = pickedLayer();
        overlay.style.cursor = current && nearHandle(current, point) ? "nwse-resize" : layerAt(point) ? "move" : "default";
      }
      return;
    }
    const { item, resize, from, start } = state.drag;
    const dx = Math.round(point.x - from.x);
    const dy = Math.round(point.y - from.y);
    item.frames[shape()] = resize
      ? { ...start, w: Math.max(2, start.w + dx), h: Math.max(1, start.h + dy) }
      : { ...start, x: start.x + dx, y: start.y + dy };
    apply();
  });
  const endDrag = () => {
    if (state.drag) {
      state.drag = null;
      changed({ rebuild: true });
    }
  };
  overlay.addEventListener("pointerup", endDrag);
  overlay.addEventListener("pointercancel", endDrag);
  overlay.addEventListener("lostpointercapture", endDrag);
}

function nudge(event) {
  const item = pickedLayer();
  const steps = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
  if (!designing() || !item || !(event.key in steps) || event.target.closest("input, select, textarea, canvas")) {
    return false;
  }
  const [dx, dy] = steps[event.key];
  const scale = event.shiftKey ? 10 : 1;
  const box = item.frames[shape()];
  if (!box) {
    return false;
  }
  box.x += dx * scale;
  box.y += dy * scale;
  event.preventDefault();
  changed({ rebuild: true });
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
      playing: theme.now_playing_scene?.design ? readDesign(theme.now_playing_scene.design) : null,
      home: theme.home_scene?.design ? readDesign(theme.home_scene.design) : null,
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
