import { buildAccents, shownAccent } from "./accent_editor.js";
import { arrangeUpright } from "./arrange.js";
import { buildFonts } from "./font_editor.js";
import { buildIcons } from "./icon_editor.js";
import { AREAS, colorOf, drawingScript, findElement, walk } from "./drawing.js";
import { drawingEditor } from "./drawing_editor.js";
import { STARTS, drawingFrom, faceOf, plainPicture, standardDrawing, startDrawing } from "./drawings.js";
import { KEY, PLAYBACK, PLAYER, Preview, SONGS } from "./preview_host.js";
import {
  ACCENT_SHADES,
  ThemeError,
  contrast,
  memoryParts,
  defaultTheme,
  lint,
  opacityPercent,
  pack,
  seeThroughOf,
  toToml,
  unpack,
} from "./theme_format.js";
import { buildWallpapers } from "./wallpaper_editor.js";
import { ACCENT_FAMILY, COLOUR_SETS, mix, paletteOf, readableShade, schemeFrom, schemeOf } from "./colour_tools.js";
import { addTileIcons } from "./tile_icons.js";
import { addVersion, copyTheme, emptyLibrary, entryOf, keep, newId, readLibrary, removeTheme, versionDue, writeLibrary } from "./library.js";

const LOOK = [
  { id: "colours", label: "Colours" },
  { id: "wallpaper", label: "Picture behind the lists", screen: "list" },
  { id: "fonts", label: "Lettering" },
  { id: "shapes", label: "Rows, title bar and spacing", screen: "list" },
  { id: "icons", label: "Icons", screen: "list" },
];
const OWN_SCREEN = { id: "screen", label: "This screen's colours" };
const LOOK_LEADS = {
  wallpaper: "It's cut to 16 colours. Fade it until the text reads well.",
  fonts: "Lettering for titles, lists and small text: use a font file, a picture of your letters, or draw your own.",
  icons: "The small pictures in the title bar and the lists, and the icons on Home tiles. Drag to draw and right-drag to erase; the theme's colours tint them.",
};
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
    standard: "This is the player's own Home: the menu, with album art beside it.",
  },
  playing: {
    screen: "playing",
    key: "now_playing_scene",
    title: "Now Playing",
    standard: "This is the player's own Now Playing: the cover, the song and its progress.",
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
const PANEL = "echopod-panel-width";
const FOLDED = "echopod-folded";
const LOOK_OPEN = "echopod-look";
const SHARE_GUIDE = "https://github.com/BasilSkyWalk/echopod/blob/main/docs/themes.md";
const WELCOMED = "echopod-welcomed";
const PANEL_DEFAULT = 420;
const PANEL_LEAST = 320;
const PANEL_MOST = 760;
const STAGE_LEAST = 360;
const LIBRARY_SETTLE_MS = 1500;
const MINUTE = 60 * 1000;
const TOUR = [
  ["screens", "Pick the screen to work on: Home, the lists or Now Playing. The buttons beside them zoom and turn the player."],
  ["device", "This is the player, running for real. Click anything on it to change it. Ctrl and the mouse wheel, or two fingers, zoom in; drag the space around it to move about."],
  ["playback", "Try your theme playing or paused, part way through a song, with lyrics or a long title."],
  ["shelf", "What's on the screen you picked. Make Home or Now Playing your own and this side fills with things to add."],
  ["inspector", "The theme's look: colours, lettering, the picture behind the lists. Drag the left edge to make this side wider. Pick a part of a screen you drew and a small bar appears beside it; More on that bar opens every setting here."],
  ["save", "When you're happy, put it on your player. Your themes are kept in this browser, under Themes."],
];
const BAR_COLOURS = [["@text", "Text"], ["@text_secondary", "Soft text"], ["@selection_background", "Accent"], ["@divider", "Lines"], ["@background", "Background"]];
const BAR_KEYS = { box: "color", circle: "color", line: "color", text: "color", symbol: "color", number: "lit" };
const BAR_SIZES = [["small", "S", "Small text"], ["medium", "M", "List text"], ["large", "L", "Titles"]];
const BAR_ALIGN = [["left", "Line up left", "M4 5h12M4 9h7M4 13h10"], ["center", "Line up in the middle", "M4 5h12M6.5 9h7M5 13h10"], ["right", "Line up right", "M4 5h12M9 9h7M6 13h10"]];
const MAIN_ROLES = { background: "Background", text: "Text", selection_background: "Accent" };
const FOLLOWING_STARTS = ["standard", "empty", "tiles"];
const FIRST_STARTS = ["dark", "light", "paper"];
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
  panel: "look",
  more: false,
  spot: null,
  tallSeen: null,
  look: new Set(["colours"]),
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
  playback: { ...PLAYBACK },
  recent: [],
  picture: null,
  dark: true,
  library: emptyLibrary(),
  themeId: null,
  libraryTimer: null,
  folded: new Set(),
  confirming: null,
  statusAt: 0,
  scrubbing: false,
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

function designStep() {
  return Object.keys(DESIGNS).find((step) => DESIGNS[step].screen === state.screen) ?? null;
}

function currentDesign() {
  const step = designStep();
  return step ? state.designs[step] : null;
}

function designScreen() {
  return DESIGNS[designStep()]?.screen ?? "playing";
}

function record() {
  const now = snapshot();
  if (state.history[state.history.length - 1] !== now) {
    state.history.push(now);
    state.history.splice(0, Math.max(0, state.history.length - HISTORY_LIMIT));
    state.future = [];
    saveDraft(now);
    keepLater();
  }
  refreshUndo();
}

const store = {
  getItem: (key) => {
    try {
      return localStorage.getItem(key);
    } catch {
      return null;
    }
  },
  setItem: (key, value) => localStorage.setItem(key, value),
};

function keepNow(label = null) {
  clearTimeout(state.libraryTimer);
  state.libraryTimer = null;
  const now = Date.now();
  keep(state.library, state.themeId, snapshot(), state.theme.name, now);
  const added = (label !== null || versionDue(state.library, state.themeId, now))
    && addVersion(state.library, state.themeId, now, label ?? "Kept as you worked");
  if (!writeLibrary(store, state.library) && !state.draftFailed) {
    state.draftFailed = true;
    say("This browser has no room left to keep your themes. Download the ones you want to keep.", true);
  }
  return added;
}

function keepLater() {
  clearTimeout(state.libraryTimer);
  state.libraryTimer = setTimeout(() => keepNow(), LIBRARY_SETTLE_MS);
}

function freshHistory() {
  state.history = [snapshot()];
  state.future = [];
  saveDraft(state.history[0]);
  refreshUndo();
}

function openKept(id) {
  settleHistory();
  keepNow();
  const entry = entryOf(state.library, id);
  if (!entry) {
    return;
  }
  state.themeId = id;
  restore(entry.snapshot);
  state.stash = {};
  state.picked = null;
  state.panel = "look";
  freshHistory();
  keepNow();
  restart();
  render();
  say(`Opened ${entry.name}`);
}

function newKept() {
  settleHistory();
  keepNow();
  state.themeId = newId(Date.now());
  state.theme = defaultTheme(state.schema);
  state.designs = { playing: null, home: null };
  state.stash = {};
  state.picked = null;
  state.panel = "look";
  freshHistory();
  keepNow();
  restart();
  render();
  say("A new theme. Your other themes are under Themes.");
}

function ago(time) {
  const gone = Date.now() - time;
  if (gone < MINUTE) {
    return "just now";
  }
  if (gone < 60 * MINUTE) {
    const minutes = Math.round(gone / MINUTE);
    return `${minutes} minute${minutes === 1 ? "" : "s"} ago`;
  }
  if (gone < 24 * 60 * MINUTE) {
    const hours = Math.round(gone / (60 * MINUTE));
    return `${hours} hour${hours === 1 ? "" : "s"} ago`;
  }
  return new Date(time).toLocaleDateString(undefined, { day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" });
}

function swatchesOf(snapshotText) {
  try {
    const colours = JSON.parse(snapshotText).theme?.colors ?? {};
    return el("span", { class: "kept-swatches", "aria-hidden": "true" },
      ["background", "selection_background", "text", "header_background"].map((role) => el("span", { style: `background: ${colours[role] ?? "transparent"}` })));
  } catch {
    return null;
  }
}

function confirmButton(key, label, sure, act) {
  const asking = state.confirming === key;
  return el("button", { type: "button", class: asking ? "danger" : "", onclick: () => {
    if (asking) {
      state.confirming = null;
      act();
    } else {
      state.confirming = key;
      render();
    }
  } }, asking ? sure : label);
}

function keptThemes() {
  const rows = state.library.themes.map((entry) => {
    const current = entry.id === state.themeId;
    return el("li", { class: current ? "kept current" : "kept" },
      swatchesOf(entry.snapshot),
      el("span", { class: "kept-name" }, el("strong", {}, entry.name || "Untitled"), el("small", {}, current ? "Open now" : `Changed ${ago(entry.updated)}`)),
      el("span", { class: "kept-tools" },
        current ? null : el("button", { type: "button", onclick: () => openKept(entry.id) }, "Open"),
        el("button", { type: "button", onclick: () => {
          keepNow();
          const copy = copyTheme(state.library, entry.id, Date.now(), `${entry.name} copy`.slice(0, NAME_BYTES));
          writeLibrary(store, state.library);
          if (copy) {
            openKept(copy.id);
          }
        } }, "Copy"),
        confirmButton(`delete:${entry.id}`, "Delete", "Delete it?", () => {
          const wasOpen = entry.id === state.themeId;
          removeTheme(state.library, entry.id);
          writeLibrary(store, state.library);
          if (!wasOpen) {
            render();
          } else if (state.library.current) {
            state.themeId = null;
            openKept(state.library.current);
          } else {
            newKept();
          }
        })));
  });
  const entry = entryOf(state.library, state.themeId);
  const versions = [...(entry?.versions ?? [])].reverse().map((version) =>
    el("li", { class: "kept version" },
      swatchesOf(version.snapshot),
      el("span", { class: "kept-name" }, el("strong", {}, ago(version.at)), el("small", {}, version.label)),
      el("span", { class: "kept-tools" }, el("button", { type: "button", onclick: () => {
        settleHistory();
        keepNow("Before going back");
        restore(version.snapshot);
        state.stash = {};
        state.picked = null;
        restart();
        changed({ rebuild: true });
        say(`Back to how it was ${ago(version.at)}. Undo or the list below brings the newer one back.`);
      } }, "Go back to this"))));
  return [
    el("h3", {}, "My themes"),
    el("p", { class: "hint" }, "Kept in this browser as you work. Download a theme to keep it for good."),
    el("ul", { class: "kept-list" }, rows),
    el("div", { class: "actions" }, el("button", { type: "button", onclick: newKept }, "New theme")),
    el("h3", {}, "Earlier versions"),
    versions.length
      ? el("ul", { class: "kept-list" }, versions)
      : el("p", { class: "hint" }, "A version is kept every ten minutes while you work, and before you open another theme over this one."),
    el("div", { class: "actions" }, el("button", { type: "button", onclick: () => {
      settleHistory();
      const added = keepNow("Kept by you");
      render();
      say(added ? "Kept this version." : "This version is already kept.");
    } }, "Keep this version now")),
  ];
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
  keepLater();
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

function tallPlaces(design) {
  const places = {};
  walk(design.elements, (item) => {
    const at = item.at?.tall;
    places[item.id] = at ? [at.x, at.y, at.w, at.h, at.align ?? ""].join(",") : "";
  });
  return places;
}

function followed(design) {
  design.upright = "auto";
  arrangeUpright(design);
  return design;
}

function settleUpright() {
  const design = currentDesign();
  if (!design) {
    state.tallSeen = null;
    return;
  }
  if (design.upright === "auto") {
    const now = tallPlaces(design);
    const seen = state.tallSeen ?? {};
    if (shape() === "tall" && Object.keys(now).some((id) => id in seen && seen[id] !== now[id])) {
      design.upright = "own";
      say("The upright screen is yours now. It no longer follows the sideways one.");
    } else {
      arrangeUpright(design);
    }
  }
  state.tallSeen = tallPlaces(design);
}

function changed({ rebuild = false } = {}) {
  settleUpright();
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
    box.replaceWith(noticeBox(box.dataset.notice === "colours"));
  }
  const memory = inspector.querySelector("[data-memory]");
  if (memory) {
    memory.replaceWith(memoryBar());
  }
  inspector.querySelector("[data-made]")?.replaceWith(madeDots());
  const three = mainColours();
  for (const input of inspector.querySelectorAll("[data-main]")) {
    if (input !== document.activeElement) {
      input.value = three[input.dataset.main];
    }
  }
  for (const badge of inspector.querySelectorAll("[data-reads]")) {
    const fresh = readingBadge(badge.dataset.reads);
    if (fresh && !badge.contains(document.activeElement)) {
      badge.replaceWith(fresh);
    }
  }
  refreshSwatches();
}

function plainNotice(text) {
  const match = /^(.*): contrast ([\d.]+), below ([\d.]+)$/.exec(text);
  return match ? el("li", { title: `Contrast ${match[2]}; it should be at least ${match[3]}` }, `${match[1]} is hard to read against what's behind it`) : el("li", {}, text);
}

function noticeBox(inColours = false) {
  const found = lint(state.theme, state.schema);
  const where = inColours ? "colours" : "";
  if (found.length === 0) {
    return el("div", { "data-notice": where });
  }
  return el("div", { class: "notice", "data-notice": where },
    found.length === 1 ? "One thing may be hard to read:" : `${found.length} things may be hard to read:`,
    el("ul", {}, found.map(plainNotice)),
    inColours
      ? el("button", { type: "button", onclick: fixReading }, found.length === 1 ? "Fix it" : "Fix them")
      : el("button", { type: "button", onclick: () => showLook("colours") }, "Fix it in Colours"));
}

function readingChecks(role) {
  const colours = { ...state.theme.colors, [role]: shownColour(role) };
  return state.schema.checks.flatMap((check) => {
    if (check.kind === "pair" && check.color === role) {
      return [{ check, backs: [colours[check.on]] }];
    }
    if (check.kind === "ink") {
      const box = state.theme.boxes[check.box || state.theme.selection];
      if (box?.ink === role) {
        return [{ check, backs: [colours[box.fill_top], colours[box.fill_bottom]] }];
      }
    }
    return [];
  });
}

function readingOf(role) {
  const checks = readingChecks(role);
  if (!checks.length) {
    return null;
  }
  const colour = shownColour(role);
  const worst = checks
    .map(({ check, backs }) => ({ check, backs, ratio: Math.min(...backs.map((back) => contrast(colour, back))) }))
    .sort((a, b) => a.ratio / a.check.floor - b.ratio / b.check.floor)[0];
  return { colour, worst, good: worst.ratio >= worst.check.floor };
}

function fixReading() {
  let fixed = 0;
  for (let pass = 0; pass < 3; pass++) {
    for (const role of Object.keys(state.theme.colors)) {
      const reading = readingOf(role);
      if (reading && !reading.good) {
        const shade = readableShade(reading.colour, reading.worst.backs, reading.worst.check.floor + 0.1);
        (shownAccentOf(role) ?? state.theme.colors)[role] = shade;
        fixed++;
      }
    }
  }
  changed({ rebuild: true });
  say(fixed ? "Made the text easier to read. Undo puts it back." : "Nothing needed changing.");
}

function mainColours() {
  return { background: shownColour("background"), text: shownColour("text"), accent: shownColour("selection_background") };
}

function putScheme(scheme, roles) {
  for (const role of roles) {
    (shownAccentOf(role) ?? state.theme.colors)[role] = scheme[role];
  }
}

function setMain(key, value) {
  const three = { ...mainColours(), [key]: value };
  const scheme = schemeOf(three.background, three.text, three.accent);
  putScheme(scheme, Object.keys(scheme).filter((role) => ACCENT_FAMILY.includes(role) === (key === "accent")));
  changed();
}

function madeDots() {
  const made = [["text_secondary", "Soft text"], ["divider", "Lines"], ["header_background", "Title bar"], ["selection_text", "Text on the accent"]];
  return el("p", { class: "made", "data-made": "" }, "Made from them:",
    made.map(([role, label]) => el("span", { class: "made-dot", title: label, style: `background: ${shownColour(role)}` })));
}

function mainTrio() {
  const three = mainColours();
  return el("div", { class: "trio" }, [["background", "Background"], ["text", "Text"], ["accent", "Accent"]].map(([key, label]) => {
    const picker = el("input", { type: "color", value: three[key], list: "swatches", "data-main": key });
    picker.addEventListener("input", () => setMain(key, picker.value));
    picker.addEventListener("change", () => {
      noteRecent(picker.value);
      render();
    });
    return el("label", { class: "trio-swatch" }, picker, el("span", {}, label));
  }));
}

function colourSets() {
  return el("div", { class: "colour-sets" }, COLOUR_SETS.map(([name, background, text, accent]) =>
    el("button", { type: "button", onclick: () => {
      const scheme = schemeOf(background, text, accent);
      putScheme(scheme, Object.keys(scheme));
      changed({ rebuild: true });
      say(`Coloured with ${name}. Undo puts your colours back.`);
    } }, el("span", { class: "set-dots", "aria-hidden": "true" }, [background, text, accent].map((colour) => el("span", { style: `background: ${colour}` }))), name)));
}

function readingBadge(role) {
  const reading = readingOf(role);
  if (!reading) {
    return null;
  }
  const { colour, worst, good } = reading;
  const badge = el("span", {
    class: good ? "reads good" : "reads poor",
    "data-reads": role,
    title: `Contrast ${worst.ratio.toFixed(1)}; ${worst.check.what.toLowerCase()} needs at least ${worst.check.floor.toFixed(1)}`,
  }, good ? "Easy to read" : "Hard to read");
  if (!good) {
    badge.append(el("button", { type: "button", onclick: (event) => {
      event.preventDefault();
      setRole(role, readableShade(colour, worst.backs, worst.check.floor + 0.1));
      say("Made it easier to read. Undo puts it back.");
      render();
    } }, "Fix"));
  }
  return badge;
}

function setRole(role, value) {
  const accent = shownAccentOf(role);
  if (accent) {
    accent[role] = value;
  } else {
    state.theme.colors[role] = value;
  }
  changed();
}

function noteRecent(value) {
  state.recent = [value, ...state.recent.filter((one) => one !== value)].slice(0, 8);
}

function refreshSwatches() {
  let list = document.getElementById("swatches");
  if (!list) {
    list = el("datalist", { id: "swatches" });
    document.body.append(list);
  }
  const colours = [...(state.picture?.palette.map((entry) => entry.color) ?? []), ...state.recent, ...Object.values(state.theme.colors)];
  list.replaceChildren(...[...new Set(colours.filter((one) => /^#[0-9a-f]{6}$/i.test(one)))].slice(0, 24).map((one) => el("option", { value: one })));
}

function eyedropper(set) {
  if (!("EyeDropper" in window)) {
    return null;
  }
  return el("button", { type: "button", class: "dropper", title: "Pick a colour from anywhere on the screen", "aria-label": "Pick a colour from the screen", onclick: async (event) => {
    event.preventDefault();
    try {
      const { sRGBHex } = await new window.EyeDropper().open();
      set(sRGBHex.toLowerCase());
    } catch {
      return;
    }
  } }, icon("M13.5 3.5l3 3-2 2-3-3zM11.5 5.5l-7 7V16h3.5l7-7"));
}

function field(label, control, note) {
  return el("label", { class: "field" }, el("span", {}, label, note ? el("small", {}, note) : null), control);
}

function group(...fields) {
  return el("div", { class: "group" }, ...fields);
}

function colourInput(value, set) {
  const picker = el("input", { type: "color", value, list: "swatches" });
  const hex = el("input", { class: "hex", value, maxlength: 7, spellcheck: "false", "aria-label": "Colour code" });
  picker.addEventListener("input", () => {
    hex.value = picker.value;
    set(picker.value);
  });
  picker.addEventListener("change", () => noteRecent(picker.value));
  const dropped = (chosen) => {
    picker.value = hex.value = chosen;
    noteRecent(chosen);
    set(chosen);
  };
  hex.addEventListener("change", () => {
    const tidy = hex.value.trim().replace(/^#?/, "#").toLowerCase();
    if (/^#[0-9a-f]{6}$/.test(tidy)) {
      picker.value = hex.value = tidy;
      set(tidy);
    } else {
      hex.value = picker.value;
    }
  });
  return el("span", { class: "color-control" }, picker, hex, eyedropper(dropped));
}

function shownAccentOf(role) {
  const accents = state.theme.accents ?? [];
  return ACCENT_SHADES.includes(role) && accents.length > 0 ? accents[Math.min(state.accent, accents.length - 1)] : null;
}

function shownColour(role) {
  return shownAccentOf(role)?.[role] ?? state.theme.colors[role];
}

function roleColour(role) {
  const control = colourInput(shownColour(role), (value) => setRole(role, value));
  const row = field(ROLE_LABELS[role], el("span", { class: "role-control" }, control, readingBadge(role)));
  row.dataset.role = role;
  return row;
}

function choiceColour(value, set) {
  const roles = COLOUR_GROUPS.flatMap(([, list]) => list.map(([role]) => role));
  const select = el("select", { "aria-label": "Colour" },
    roles.map((role) => new Option(COLOUR_NAMES[role], `@${role}`)),
    new Option("My own colour", "custom"));
  select.value = typeof value === "string" && value.startsWith("@") ? value : "custom";
  const picker = el("input", { type: "color", value: colorOf(value, state.theme), list: "swatches" });
  picker.addEventListener("input", () => {
    select.value = "custom";
    set(picker.value);
  });
  picker.addEventListener("change", () => noteRecent(picker.value));
  select.addEventListener("change", () => {
    if (select.value === "custom") {
      set(picker.value);
    } else {
      picker.value = colorOf(select.value, state.theme);
      set(select.value);
    }
  });
  return el("span", { class: "color-control" }, picker, select, eyedropper((chosen) => {
    picker.value = chosen;
    select.value = "custom";
    noteRecent(chosen);
    set(chosen);
  }));
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
  tileIcons: () => tileIcons(),
  sideNote: () => sideNote(),
  coarse: () => window.matchMedia?.("(pointer: coarse)").matches ?? false,
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

function panelThemes() {
  const starts = el("div", { class: "starts" }, state.schema.samples.map((sample) =>
    el("button", { type: "button", onclick: () => openSample(sample) },
      el("span", { class: "thumb", style: `background-image: url("previews/${sample}.png")` }),
      el("span", {}, sampleName(sample)))));
  return [
    el("h2", {}, "Themes"),
    el("p", { class: "lead" }, "Begin from one of these, or go back to one you made earlier. The theme you're making now is kept first."),
    el("h3", {}, "Begin from"),
    starts,
    el("div", { class: "actions" },
      el("button", { type: "button", onclick: () => $("picker").click() }, "Open a theme file…"),
      el("button", { type: "button", onclick: startOver }, "Start over"),
      el("button", { type: "button", onclick: () => startTour() }, "Show me around")),
    ...keptThemes(),
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
  const every = el("details", { class: "more every-colour" },
    el("summary", {}, "Every colour, one by one"),
    el("p", { class: "hint" }, "Changing Background or Text above sets the colours made from them again; changing Accent sets the highlighted row's."),
    ...groups.flat(),
    el("h3", {}, "Cover Flow"),
    group(field("Keep it dark", cover, "Good for light themes: the album wall stays on black")),
    el("h3", {}, "Accent choices"),
    el("p", { class: "hint" }, "Colours people can pick for the selected row under Settings, Display, Accent color. Leave the list empty to keep yours."),
    accents);
  every.open = state.open.has("colours:every");
  every.addEventListener("toggle", () => {
    if (every.open) {
      state.open.add("colours:every");
    } else {
      state.open.delete("colours:every");
    }
  });
  return [
    el("h2", {}, "Colours"),
    el("p", { class: "lead" }, "Click something on the screen to jump to its colour."),
    el("p", { class: "hint look-lead" }, "Three colours set the whole theme. The rest are made from them."),
    mainTrio(),
    madeDots(),
    noticeBox(true),
    el("h3", { class: "keep" }, "Colour sets"),
    colourSets(),
    el("h3", { class: "keep" }, "Colours from a picture"),
    ...pictureColours(),
    every,
  ];
}

async function pictureFile(file) {
  try {
    const bitmap = await createImageBitmap(file);
    const side = Math.min(1, 160 / Math.max(bitmap.width, bitmap.height));
    const sketch = el("canvas", { width: Math.max(1, Math.round(bitmap.width * side)), height: Math.max(1, Math.round(bitmap.height * side)) });
    const pen = sketch.getContext("2d", { willReadFrequently: true });
    pen.drawImage(bitmap, 0, 0, sketch.width, sketch.height);
    const palette = paletteOf(pen.getImageData(0, 0, sketch.width, sketch.height).data, 6);
    if (!palette.length) {
      say(`${file.name} has no colours to take.`, true);
      return;
    }
    state.picture = { name: file.name, palette };
    refreshSwatches();
    render();
  } catch {
    say(`${file.name} couldn't be opened as a picture.`, true);
  }
}

function pictureColours() {
  const picker = el("input", { type: "file", accept: "image/*", hidden: true });
  picker.addEventListener("change", () => picker.files[0] && pictureFile(picker.files[0]));
  const choose = el("button", { type: "button", onclick: () => picker.click() }, state.picture ? "Another picture…" : "Colours from a picture…");
  if (!state.picture) {
    return [el("div", { class: "actions picture-colours" }, choose, picker)];
  }
  const swatches = el("div", { class: "palette", role: "list", "aria-label": `Colours in ${state.picture.name}` },
    state.picture.palette.map((entry) => el("span", { class: "palette-swatch", role: "listitem", title: entry.color, style: `background: ${entry.color}; flex-grow: ${Math.max(1, Math.round(entry.share * 20))}` })));
  return [
    group(
      el("div", { class: "palette-block" }, swatches, el("p", { class: "hint" }, `From ${state.picture.name}. These colours are also offered in every colour picker.`)),
      field("Make it", chips([[true, "Dark"], [false, "Light"]], state.dark, (dark) => {
        state.dark = dark;
        render();
      })),
    ),
    el("div", { class: "actions" },
      el("button", { type: "button", class: "primary", onclick: () => {
        const scheme = schemeFrom(state.picture.palette, state.dark);
        Object.assign(state.theme.colors, scheme);
        const accent = shownAccentOf("selection_background");
        for (const role of accent ? ACCENT_SHADES : []) {
          accent[role] = scheme[role] ?? accent[role];
        }
        changed({ rebuild: true });
        say("Coloured from the picture. Undo puts your colours back.");
      } }, "Use these colours"),
      choose, picker),
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
    el("p", { class: "lead" }, "The boxes the player draws its lists, title bar and messages with. Their colours are under Colours."),
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
      ? el("p", { class: "hint" }, "Your own Home drawing decides what goes on the Home screen. Change it on the Home screen itself.")
      : group(field("Album art beside the menu", toggle(state.theme.home_art !== false, (shown) => {
        state.theme.home_art = shown;
        changed();
      }), "Turn it off to give the menu the whole screen")),
    el("h3", {}, "Spacing"),
    group(...spacing),
    more,
  ];
}

function panelLegacy(section, build, openFirst = false) {
  const host = el("div", { class: "legacy" });
  build(host, state.theme, state.schema, () => changed());
  host.querySelectorAll("details").forEach((details, index) => {
    const key = `${openFirst ? "shut:" : ""}${section}:${index}`;
    details.open = state.open.has(key) !== openFirst;
    details.addEventListener("toggle", () => {
      if (details.open !== openFirst) {
        state.open.add(key);
      } else {
        state.open.delete(key);
      }
    });
  });
  return [el("h2", {}, section), el("p", { class: "lead" }, LOOK_LEADS[section]), host];
}

const FONT_NAMES = { title: "Titles font", body: "List text font", small: "Small text font" };
const LICENCES = [
  ["", "Choose one"],
  ["mine", "I drew them, or they came with EchoPod"],
  ["open", "From a font with an open licence, like the SIL Open Font License"],
  ["unsure", "I'm not sure"],
];

function kilobytes(bytes) {
  return `${(bytes / 1024).toFixed(1)} KB`;
}

function share(bytes, limit) {
  const part = (bytes * 100) / limit;
  return `${part > 0 && part < 1 ? "under 1" : Math.round(part)}%`;
}

function memoryBar() {
  const parts = memoryParts(state.theme, state.schema);
  const pieces = [
    ...Object.entries(parts.fonts).map(([name, bytes]) => [FONT_NAMES[name], bytes, () => {
      state.theme.fonts[name] = null;
      delete state.theme.font_sources?.[name];
    }]),
    ["Wallpaper", parts.wallpaper, () => {
      for (const key of Object.keys(state.theme.wallpapers ?? {})) {
        state.theme.wallpapers[key] = null;
      }
    }],
  ].filter(([, bytes]) => bytes > 0);
  const over = parts.used > parts.limit;
  const scale = Math.max(parts.limit, parts.used);
  const used = share(parts.used, parts.limit);
  const bar = el("div", { class: "memory-track", role: "img", "aria-label": `${used} of the player's room for pictures and fonts used`, title: `${kilobytes(parts.used)} of ${kilobytes(parts.limit)}` },
    pieces.map(([label, bytes], index) => el("span", {
      class: `memory-part memory-part-${index}`,
      style: `width: ${(bytes * 100) / scale}%`,
      title: `${label}: ${kilobytes(bytes)}`,
    })));
  const legend = el("ul", { class: "memory-legend" }, pieces.map(([label, bytes], index) =>
    el("li", {}, el("span", { class: `memory-dot memory-part-${index}` }), `${label} ${share(bytes, parts.limit)}`)));
  const box = el("div", { class: over ? "memory over" : "memory", "data-memory": "" },
    el("p", { class: "memory-total" }, over
      ? `Too much: the pictures and fonts need ${used} of the player's room for them`
      : pieces.length
        ? `Pictures and fonts use ${used} of the player's room for them`
        : "The player has room for a wallpaper and your own fonts"),
    bar, pieces.length ? legend : null);
  if (over) {
    box.append(
      el("p", { class: "hint" }, "Make a font smaller, use a smaller or repeating wallpaper, or remove one of these."),
      el("div", { class: "actions" }, pieces.map(([label, bytes, drop]) => el("button", { type: "button", onclick: () => {
        drop();
        changed({ rebuild: true });
        say(`Removed the ${label.toLowerCase()}. Undo brings it back.`);
      } }, `Remove the ${label.toLowerCase()} (${share(bytes, parts.limit)})`))),
    );
  }
  return box;
}

function licenceField() {
  const fonts = Object.values(state.theme.fonts ?? {}).some(Boolean);
  if (!fonts) {
    return null;
  }
  const select = el("select", {}, LICENCES.map(([value, label]) => new Option(label, value)));
  select.value = state.theme.font_licence ?? "";
  const advice = el("p", { class: "hint" });
  const explain = () => {
    advice.textContent = select.value === "unsure"
      ? "Keep this theme to yourself until you know the font may be passed on. Most free fonts say so in a licence file that comes with them."
      : select.value === "open"
        ? "Include the font's licence when you share the theme."
        : "";
  };
  select.addEventListener("change", () => {
    state.theme.font_licence = select.value;
    remember();
    explain();
  });
  explain();
  return [el("h3", {}, "The fonts in this theme"), group(field("Where they're from", select)), advice];
}

function withMemory(parts) {
  const [title, lead, ...rest] = parts;
  return [title, lead, memoryBar(), ...rest];
}

function panelShare() {
  const bytes = apply();
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
  return [
    el("h2", {}, "Put it on your player"),
    noticeBox(),
    el("ol", { class: "steps-list" },
      el("li", {}, "Save the theme file."),
      el("li", {}, "Plug in the player and copy the file into the Themes folder on its card. Make the folder if it isn't there."),
      el("li", {}, "On the player, open Settings, Display, Theme and pick it.")),
    el("div", { class: "actions" },
      el("button", { type: "button", class: "primary", disabled: !bytes, onclick: download }, "Save the theme file")),
    ...(licenceField() ?? []),
    el("h3", {}, "Share it with everyone"),
    el("p", { class: "hint" }, "The gallery's themes are kept with the EchoPod project. Save an editable copy and send it there to have yours added."),
    group(field("Made by", author, "Shown in the gallery"), field("Description", about, "One line for the gallery")),
    el("div", { class: "actions" },
      el("button", { type: "button", onclick: downloadSource }, "Save an editable copy"),
      el("a", { class: "link", href: SHARE_GUIDE, target: "_blank", rel: "noopener" }, "How to send it in")),
  ];
}

function shape() {
  return state.turn === 0 ? "wide" : "tall";
}

function handWritten(step) {
  return !state.designs[step] && typeof state.theme[DESIGNS[step].key]?.script === "string";
}

function setMode(mode) {
  const step = designStep();
  const { key, screen } = DESIGNS[step];
  if (handWritten(step)) {
    state.stash[step] = state.theme[key];
  }
  state.designs[step] = mode === "design" ? state.designs[step] ?? designFrom(step) ?? followed(standardDrawing(screen)) : null;
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

function tileIcons() {
  const added = addTileIcons(state.theme, state.schema);
  if (added) {
    say("Added ready-made icons for the tiles. Redraw them under Icons.");
  }
  return added;
}

function startFrom(step, name, label) {
  const consts = stashColours(step);
  const made = startDrawing(name, DESIGNS[step].screen, faceOf(consts) === name ? consts : {});
  state.designs[step] = FOLLOWING_STARTS.includes(name) ? followed(made) : made;
  state.picked = null;
  const icons = name === "tiles" && tileIcons();
  if (!fits()) {
    say("That drawing doesn't fit with the rest of this theme. Make room by removing a picture or a font.", true);
  } else {
    say(`Started again from ${label}.${icons ? " Its icons are under Icons, ready to redraw." : ""} Undo brings back your drawing.`);
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
    el("p", { class: "hint" }, "This screen came with the theme. Change its colours here, or make the screen your own to move things around."),
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

function modeButtons(step) {
  const buttons = [];
  if (state.designs[step] || handWritten(step)) {
    buttons.push(el("button", { type: "button", onclick: () => setMode("standard") }, "The player's own"));
  }
  if (state.stash[step] && !handWritten(step)) {
    buttons.push(el("button", { type: "button", onclick: () => setMode("script") }, "This theme's own"));
  }
  return buttons;
}

function shelfParts() {
  const step = designStep();
  if (!step) {
    return [
      el("h2", {}, SCREENS.find(([id]) => id === state.screen)[1]),
      el("p", { class: "lead" }, "The player draws this screen itself. Click anything on it to change its colour."),
      el("p", { class: "hint" }, "Its rows, title bar and spacing are under Look."),
    ];
  }
  const config = DESIGNS[step];
  const design = state.designs[step];
  if (!design) {
    return [
      el("h2", {}, config.title),
      el("p", { class: "lead" }, handWritten(step)
        ? "This screen came with the theme. Click it to change its colours."
        : config.standard),
      el("div", { class: "actions" },
        el("button", { type: "button", class: "primary", onclick: () => setMode("design") }, "Make it my own"),
        ...modeButtons(step)),
      el("p", { class: "hint" }, "Making it your own gives you every part of this screen to move, resize, recolour and add to."),
    ];
  }
  let menu = false;
  walk(design.elements, (item) => {
    menu ||= !item.hidden && (item.kind === "menu" || item.repeat?.count === "items");
  });
  const shortcuts = el("details", { class: "more" }, el("summary", {}, "Keyboard and mouse shortcuts"),
    el("p", { class: "hint shortcuts" }, "Shift-click, or drag across an empty or locked part, to pick several parts. Ctrl+C and Ctrl+V copy parts, also between Home and Now Playing; Ctrl+G groups, Ctrl+D duplicates, Delete removes. Hold Alt while dragging to place freely, without lining up."));
  return [
    el("h2", {}, "Add to this screen"),
    el("p", { class: "lead" }, "Click one and it lands on the screen, ready to move."),
    config.screen === "home" && !menu
      ? el("p", { class: "notice" }, "This Home screen shows no menu. The buttons still move through Music, Settings and the rest, so add a Menu.")
      : null,
    ...drawer.adding(config.screen),
    el("h3", { class: "keep" }, "On this screen"),
    ...drawer.parts(),
    el("h3", { class: "keep" }, "Start again from"),
    el("p", { class: "hint" }, "Replaces this drawing. Undo brings it back."),
    el("div", { class: "actions" }, STARTS[config.screen].map(([name, label]) =>
      el("button", { type: "button", onclick: () => startFrom(step, name, label) }, label)), ...modeButtons(step)),
    shortcuts,
  ];
}

function sideNote() {
  const design = currentDesign();
  if (shape() === "tall") {
    return "Back to front, on the upright screen.";
  }
  return design?.upright === "auto"
    ? "Back to front. The upright screen arranges itself from this one."
    : "Back to front, on the sideways screen. The upright screen has its own arrangement: turn the player above to change it.";
}

function renderUpright() {
  const host = $("upright");
  const design = currentDesign();
  host.hidden = !(design && shape() === "tall");
  if (host.hidden) {
    return;
  }
  const auto = design.upright === "auto";
  host.className = auto ? "upright-note following" : "upright-note";
  host.replaceChildren(
    el("span", {}, auto
      ? "Arranged for you from the sideways screen. Move anything here and this screen becomes yours."
      : "This upright screen has its own arrangement."),
    ...(auto ? [] : [el("button", { type: "button", onclick: () => {
      followed(design);
      state.tallSeen = tallPlaces(design);
      changed({ rebuild: true });
      say("Arranged from the sideways screen. Undo brings your own back.");
    } }, "Arrange it from the sideways screen")]));
}

function renderShelf() {
  $("shelf").replaceChildren(...shelfParts().flat().filter(Boolean));
}

function lookParts(id) {
  const built = {
    screen: () => [null, null, ...scriptPanel(shownScript())],
    colours: () => panelColours(),
    shapes: () => panelShapes(),
    wallpaper: () => withMemory(panelLegacy("wallpaper", buildWallpapers, true)),
    fonts: () => withMemory(panelLegacy("fonts", buildFonts, true)),
    icons: () => panelLegacy("icons", buildIcons),
  }[id]();
  const [, lead, ...rest] = built;
  return [lead && id !== "colours" ? el("p", { class: "hint look-lead" }, lead.textContent) : null, ...rest];
}

function keepLook() {
  try {
    localStorage.setItem(LOOK_OPEN, JSON.stringify([...state.look]));
  } catch {
    return;
  }
}

function toggleLook(id) {
  const opening = !state.look.has(id);
  state.look = new Set(opening ? [id] : []);
  keepLook();
  const wanted = LOOK.find((entry) => entry.id === id)?.screen;
  if (opening && wanted && state.screen === "playing") {
    state.screen = wanted;
    restart();
  }
  render();
  if (opening) {
    inspector.querySelector(`[data-look="${id}"]`)?.scrollIntoView({ block: "nearest" });
  }
}

function showLook(id) {
  state.panel = "look";
  state.picked = null;
  state.also = null;
  state.look = new Set([id]);
  keepLook();
  render();
  inspector.querySelector(`[data-look="${id}"]`)?.scrollIntoView({ block: "start" });
}

function lookSection({ id, label }) {
  const open = state.look.has(id);
  const head = el("button", { type: "button", class: "look-head", "aria-expanded": String(open), onclick: () => toggleLook(id) }, label);
  const section = el("section", { class: "look-section", "data-look": id }, el("h3", { class: "look-title keep" }, head));
  if (open) {
    const body = el("div", { class: "look-body" }, ...lookParts(id).flat().filter(Boolean));
    foldSections(body, id);
    section.append(body);
  }
  return section;
}

function panelLook() {
  const sections = shownScript() ? [OWN_SCREEN, ...LOOK] : LOOK;
  return [
    el("h2", {}, "Look"),
    el("p", { class: "lead" }, "Colours, lettering and pictures for the whole theme. Click anything on a screen the player draws to jump to its colour."),
    ...sections.map(lookSection),
  ];
}

function barChip(label, pressed, act, title) {
  return el("button", { type: "button", class: "bar-chip", "aria-pressed": pressed === null ? null : String(pressed), title, onclick: act }, label);
}

function barIcon(title, path, act, pressed = null) {
  return el("button", { type: "button", class: "bar-icon", title, "aria-label": title, "aria-pressed": pressed === null ? null : String(pressed), onclick: act }, icon(path));
}

function barGap() {
  return el("span", { class: "bar-gap", "aria-hidden": "true" });
}

function barPicker(value, label, set) {
  const picker = el("input", { type: "color", class: "bar-own", value, list: "swatches", title: label, "aria-label": label });
  picker.addEventListener("input", () => set(picker.value));
  picker.addEventListener("change", () => {
    noteRecent(picker.value);
    render();
  });
  return picker;
}

function barRange(label, value, most, set) {
  const slider = el("input", { type: "range", min: 0, max: most, value: Math.min(most, value), "aria-label": label });
  slider.addEventListener("input", () => set(Number(slider.value)));
  slider.addEventListener("change", () => render());
  return el("label", { class: "bar-range" }, el("span", {}, label), slider);
}

function barColours(current, set) {
  const custom = typeof current === "string" && !current.startsWith("@");
  return [
    ...BAR_COLOURS.map(([role, label]) => el("button", {
      type: "button", class: "bar-dot", title: label, "aria-label": label, "aria-pressed": String(current === role),
      style: `background: ${shownColour(role.slice(1))}`, onclick: () => set(role, true),
    })),
    el("span", { class: custom ? "bar-own-wrap chosen" : "bar-own-wrap" }, barPicker(colorOf(current, state.theme), "Its own colour", (value) => set(value, false))),
  ];
}

function partBar() {
  const picks = designing() ? drawer.picks() : [];
  if (picks.length === 0) {
    return null;
  }
  const tools = [
    barChip("More", state.more, () => {
      state.more = !state.more;
      render();
    }, "Every setting for this part"),
    barIcon("Duplicate", "M7 7h9v9H7zM4 13V4h9", () => drawer.duplicateChosen()),
    barIcon("Remove", "M5 6h10M8 6V4.5h4V6m-6 0 .7 9.5h6.6L14 6", () => drawer.removeChosen()),
  ];
  if (picks.length > 1) {
    return [el("span", { class: "bar-name" }, `${picks.length} parts`), barChip("Group", null, () => drawer.groupChosen(), "Make them one group"), barGap(), ...tools];
  }
  const item = picks[0];
  const bits = [el("span", { class: "bar-name" }, drawer.nameOf(item))];
  const key = BAR_KEYS[item.kind];
  if (key && !(item.kind === "box" && item.fill && item.fill !== "solid")) {
    bits.push(...barColours(item[key], (value, redraw) => {
      item[key] = value;
      changed({ rebuild: redraw });
    }));
  }
  if (item.kind === "text") {
    const at = item.at[shape()];
    bits.push(barGap(), ...BAR_SIZES.map(([size, mark, title]) => barChip(mark, (item.size ?? "medium") === size, () => {
      item.size = size;
      changed({ rebuild: true });
    }, title)));
    if (at) {
      bits.push(barGap(), ...BAR_ALIGN.map(([align, title, path]) => barIcon(title, path, () => {
        at.align = align;
        changed({ rebuild: true });
      }, (at.align ?? item.align ?? "left") === align)));
    }
  }
  if (item.kind === "cover") {
    bits.push(...[["square", "Square"], ["circle", "Round"]].map(([form, label]) => barChip(label, (item.shape ?? "square") === form, () => {
      item.shape = form;
      changed({ rebuild: true });
    })));
    const at = item.at[shape()];
    if (at && item.shape !== "circle") {
      bits.push(barRange("Corners", item.corner ?? 0, Math.max(1, Math.floor(Math.min(at.w, at.h) / 2)), (value) => {
        item.corner = value;
        changed();
      }));
    }
  }
  if (item.kind === "box" && (item.fill ?? "solid") === "solid") {
    bits.push(barGap(), barRange("Corners", item.radius ?? 0, 40, (value) => {
      item.radius = value;
      changed();
    }));
  }
  return [...bits, barGap(), ...tools];
}

function pictureAt(point) {
  const step = designStep();
  if (!step || state.designs[step] || handWritten(step)) {
    return false;
  }
  if (step === "home" ? state.theme.home_art === false : SONGS[state.playback.song]?.bare) {
    return false;
  }
  return plainPicture(DESIGNS[step].screen, shape(), point.x, point.y - AREAS[shape()].top);
}

function pictureBar() {
  const step = designStep();
  return [
    el("span", { class: "bar-name" }, "Album art"),
    barChip("Make this screen my own", null, () => setMode("design"), "Then you can move it, resize it or take it off"),
    step === "home" ? barChip("Hide it", null, () => {
      state.theme.home_art = false;
      changed({ rebuild: true });
      restart();
      say("The menu has the whole Home screen now. Undo brings the art back.");
    }, "Give the menu the whole screen") : null,
  ];
}

function spotBar() {
  const spot = state.spot;
  if (!spot || designing()) {
    return null;
  }
  if (spot.picture) {
    return pictureBar();
  }
  const [kind, step, name] = spot.role.split(":");
  const scene = step ? state.theme[DESIGNS[step]?.key] : null;
  let label = COLOUR_NAMES[spot.role] ?? spot.role;
  let value = shownColour(spot.role);
  let set = (shade) => setRole(spot.role, shade);
  if (kind === "script" && scene) {
    label = friendlyName(name);
    value = scriptColours(scene.script).find((entry) => entry.name === name)?.value ?? "#000000";
    set = (shade) => setScriptColour(step, name, shade);
  } else if (kind === "chrome" && scene?.chrome) {
    label = CHROME_ROLES.find(([role]) => role === name)?.[1] ?? name;
    value = scene.chrome[name] ?? state.theme.colors[name];
    set = (shade) => {
      scene.chrome[name] = shade;
      changed();
    };
  } else if (spot.role === "header_background") {
    label = "Title bar";
    set = (shade) => {
      state.theme.colors.header_background = shade;
      state.theme.colors.header_top = mix(shade, shownColour("header_text"), 0.1);
      changed();
    };
  } else if (MAIN_ROLES[spot.role]) {
    const main = spot.role === "selection_background" ? "accent" : spot.role;
    label = MAIN_ROLES[spot.role];
    set = (shade) => setMain(main, shade);
  }
  return [
    el("span", { class: "bar-name" }, label),
    barPicker(value, `${label} colour`, set),
    barChip("More", null, () => {
      const role = spot.role;
      state.spot = null;
      jumpToRole(role);
    }, "Find it among all the colours"),
  ];
}

function renderBar() {
  const bar = $("bar");
  const bits = partBar() ?? spotBar();
  bar.hidden = !bits;
  bar.replaceChildren(...(bits ?? []).flat().filter(Boolean));
  placeBar();
}

function placeBar() {
  const bar = $("bar");
  if (bar.hidden) {
    return;
  }
  const box = designing() ? drawer.frame() : state.spot ? { x: state.spot.x, y: state.spot.y, w: 0, h: 0 } : null;
  if (!box || drawer.dragging()) {
    bar.style.visibility = "hidden";
    return;
  }
  const stage = $("stage").getBoundingClientRect();
  const screen = canvas.getBoundingClientRect();
  const zoom = screen.width / canvas.width || state.zoom;
  const left = screen.left - stage.left + (box.x + box.w / 2) * zoom - bar.offsetWidth / 2;
  const above = screen.top - stage.top + box.y * zoom - bar.offsetHeight - 10;
  const below = screen.top - stage.top + (box.y + box.h) * zoom + 12;
  const top = above >= 6 ? above : Math.min(stage.height - bar.offsetHeight - 6, below);
  bar.style.left = `${Math.max(6, Math.min(stage.width - bar.offsetWidth - 6, left))}px`;
  bar.style.top = `${Math.max(6, top)}px`;
  bar.style.visibility = "visible";
}

function backToLook() {
  return el("button", { type: "button", class: "back", onclick: () => {
    state.panel = "look";
    render();
  } }, icon("M12 5 7 10l5 5"), "Back to the look");
}

function panelRight() {
  const part = designing() && state.more ? drawer.settings(designScreen()) : [];
  if (part.length > 0) {
    return part;
  }
  if (state.panel === "themes") {
    return [backToLook(), ...panelThemes()];
  }
  if (state.panel === "share") {
    return [backToLook(), ...panelShare()];
  }
  return panelLook();
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

function foldSections(host, prefix) {
  let body = null;
  for (const node of [...host.children]) {
    if (node.tagName === "H3" && !node.classList.contains("keep")) {
      const key = `${prefix}:${node.textContent}`;
      const shut = state.folded.has(key);
      body = el("div", { class: "fold-body", hidden: shut });
      const head = el("button", { type: "button", class: "fold-head", "aria-expanded": String(!shut) }, node.textContent);
      const section = body;
      head.addEventListener("click", () => {
        const open = section.hidden;
        section.hidden = !open;
        head.setAttribute("aria-expanded", String(open));
        if (open) {
          state.folded.delete(key);
        } else {
          state.folded.add(key);
        }
        try {
          localStorage.setItem(FOLDED, JSON.stringify([...state.folded]));
        } catch {
          return;
        }
      });
      node.replaceChildren(head);
      node.classList.add("fold-title");
      node.after(body);
      continue;
    }
    if (node.tagName === "H2" || node.tagName === "H3") {
      body = null;
      continue;
    }
    if (body && node !== body) {
      body.append(node);
    }
  }
}

function unfoldTo(node) {
  const body = node?.closest(".fold-body");
  if (body?.hidden) {
    body.hidden = false;
    body.previousElementSibling?.querySelector(".fold-head")?.setAttribute("aria-expanded", "true");
  }
}

function render() {
  refreshSwatches();
  renderScreens();
  renderShelf();
  $("name").value = state.theme.name;
  const picking = designing() && state.more && Boolean(state.picked);
  const panel = picking ? "look" : state.panel;
  inspector.replaceChildren(...panelRight().flat().filter(Boolean));
  if (!picking && panel !== "look") {
    foldSections(inspector, panel === "themes" ? "start" : panel);
  }
  $("themes").setAttribute("aria-pressed", String(panel === "themes"));
  $("save").setAttribute("aria-pressed", String(panel === "share"));
  $("hint").textContent = designing()
    ? "Click a part to change it. Drag to move, drag the corner to resize; the arrow keys nudge it."
    : "Click anything on the screen to change its colour.";
  overlay.classList.toggle("designing", designing());
  renderUpright();
  state.tallSeen = currentDesign() ? tallPlaces(currentDesign()) : null;
  renderBar();
  drawOverlay();
}

function showPanel(panel) {
  state.panel = state.panel === panel ? "look" : panel;
  state.picked = null;
  state.also = null;
  render();
  inspector.scrollTop = 0;
}

function designing() {
  return Boolean(currentDesign());
}

function restart() {
  state.spot = null;
  state.accent = shownAccent(state.theme, state.accent);
  state.preview.start(state.turn, state.screen, state.accent, 0, state.playback);
  apply();
  for (let spent = 0; state.preview.busy && spent < SKIP_LIMIT_MS; spent += SKIP_STEP_MS) {
    state.preview.advance(SKIP_STEP_MS);
  }
  state.preview.advance(SKIP_SETTLE_MS);
  state.lastFrame = null;
  renderPlayback();
}

const SHUFFLES = [[0, "Off"], [1, "Songs"], [2, "Albums"]];
const REPEATS = [[0, "Off"], [1, "One song"], [2, "All"]];
const VOLUME_MAX = 50;

function clock(milliseconds) {
  const seconds = Math.max(0, Math.floor(milliseconds / 1000));
  return `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, "0")}`;
}

function pickList(label, options, value, set) {
  const select = el("select", { "aria-label": label }, options.map(([key, text]) => new Option(text, key)));
  select.value = String(value);
  select.addEventListener("change", () => set(select.value));
  return el("label", { class: "playback-pick" }, el("span", {}, label), select);
}

function renderPlayback() {
  const host = $("playback");
  const playing = state.screen === "playing";
  if (!playing && state.screen !== "home") {
    host.hidden = true;
    return;
  }
  host.hidden = false;
  const playback = state.playback;
  const songs = Object.entries(SONGS).map(([key, song]) => [key, song.label]);
  if (!playing) {
    songs.unshift(["none", "Nothing playing"]);
  }
  const songNow = playback.song ?? (playing ? "usual" : "none");
  const live = playing || playback.song;
  const toggle = el("button", { type: "button", class: "playback-toggle", "data-toggle": "", onclick: () => {
    playback.paused = state.preview.status().state !== PLAYER.paused;
    if (playing) {
      state.preview.press(KEY.play);
      refreshPlayback(true);
    } else {
      restart();
    }
  } });
  const scrub = el("input", { type: "range", min: 0, max: 1000, value: 0, "aria-label": "How far into the song", "data-scrub": "" });
  scrub.addEventListener("pointerdown", () => {
    state.scrubbing = true;
  });
  scrub.addEventListener("input", () => {
    playback.at = Number(scrub.value) / 1000;
    if (playing) {
      state.preview.seek(playback.at);
    }
    refreshPlayback(true);
  });
  scrub.addEventListener("change", () => {
    state.scrubbing = false;
    if (!playing) {
      restart();
    }
  });
  const volume = el("input", { type: "range", min: 0, max: VOLUME_MAX, value: playback.volume ?? state.preview.status().volume, "aria-label": "Volume" });
  volume.addEventListener("change", () => {
    playback.volume = Number(volume.value);
    restart();
  });
  host.replaceChildren(...[
    live ? el("div", { class: "playback-row" },
      toggle,
      scrub,
      el("span", { class: "playback-time", "data-time": "" }),
    ) : null,
    el("div", { class: "playback-row playback-options" },
      pickList("Song", songs, songNow, (value) => {
        playback.song = value === "none" ? null : value;
        playback.at = null;
        restart();
      }),
      live ? el("label", { class: "playback-pick" }, el("span", {}, "Volume"), volume) : null,
      live ? pickList("Shuffle", SHUFFLES, playback.shuffle, (value) => {
        playback.shuffle = Number(value);
        restart();
      }) : null,
      live ? pickList("Repeat", REPEATS, playback.repeat, (value) => {
        playback.repeat = Number(value);
        restart();
      }) : null,
      playing ? el("button", { type: "button", title: "Shows what the screen looks like while the volume changes", onclick: () => {
        state.preview.nudgeVolume();
      } }, "Show the volume change") : null,
    ),
  ].filter(Boolean));
  refreshPlayback(true);
}

function refreshPlayback(now = false, time = performance.now()) {
  const host = $("playback");
  if (host.hidden || (!now && time - state.statusAt < 250)) {
    return;
  }
  state.statusAt = time;
  const status = state.preview.status();
  const toggle = host.querySelector("[data-toggle]");
  if (toggle) {
    const paused = status.state !== PLAYER.playing;
    toggle.replaceChildren(paused ? "▶ Play" : "❚❚ Pause");
    toggle.setAttribute("aria-label", paused ? "Play" : "Pause");
  }
  const scrub = host.querySelector("[data-scrub]");
  if (scrub && !state.scrubbing && status.duration > 0) {
    scrub.value = String(Math.round((status.position * 1000) / status.duration));
  }
  const shown = host.querySelector("[data-time]");
  if (shown) {
    shown.textContent = status.duration > 0 ? `${clock(status.position)} / ${clock(status.duration)}` : "";
  }
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
  refreshPlayback(false, now);
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
  placeBar();
  if (!designing()) {
    return;
  }
  drawer.overlay(pen, width / canvas.width || state.zoom);
}

function pointer(event) {
  const box = canvas.getBoundingClientRect();
  const scale = box.width / canvas.width || state.zoom;
  return { x: (event.clientX - box.left) / scale, y: (event.clientY - box.top) / scale };
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
  const header = point.y < AREAS[shape()].top ? theme.filter(([role]) => role.startsWith("header_")) : [];
  const found = (own.length > 0 && closestRole(point, own)) || (header.length > 0 && closestRole(point, header)) || closestRole(point, theme);
  if (found === "selection_top" || found === "selection_shine") {
    return "selection_background";
  }
  return found === "header_top" ? "header_background" : found;
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
  const [kind, , name] = role.split(":");
  const own = kind === "script" || kind === "chrome";
  state.panel = "look";
  state.look = new Set([own ? "screen" : "colours"]);
  if (!own) {
    state.open.add("colours:every");
  }
  keepLook();
  render();
  const row = own
    ? inspector.querySelector(`[data-${kind}="${name}"]`)
    : inspector.querySelector(`[data-role="${role}"]`);
  if (row) {
    unfoldTo(row);
    row.scrollIntoView({ block: "center", behavior: "smooth" });
    row.classList.add("flash");
  }
}

function setPanel(width, keepIt = true) {
  const room = document.querySelector(".app").clientWidth - STAGE_LEAST - $("shelf").offsetWidth;
  const shown = Math.round(Math.max(PANEL_LEAST, Math.min(PANEL_MOST, room, width)));
  document.documentElement.style.setProperty("--panel", `${shown}px`);
  $("resizer").setAttribute("aria-valuenow", String(shown));
  if (keepIt) {
    try {
      localStorage.setItem(PANEL, String(shown));
    } catch {
      return shown;
    }
  }
  return shown;
}

function panelWidth() {
  return inspector.getBoundingClientRect().width;
}

function wirePanel() {
  const handle = $("resizer");
  let saved = PANEL_DEFAULT;
  try {
    saved = Number(localStorage.getItem(PANEL)) || PANEL_DEFAULT;
    state.folded = new Set(JSON.parse(localStorage.getItem(FOLDED) ?? "[]"));
    state.look = new Set(JSON.parse(localStorage.getItem(LOOK_OPEN) ?? '["colours"]'));
  } catch {
    saved = PANEL_DEFAULT;
  }
  setPanel(saved, false);
  let dragging = null;
  handle.addEventListener("pointerdown", (event) => {
    dragging = { from: event.clientX, width: panelWidth() };
    handle.setPointerCapture(event.pointerId);
    handle.classList.add("dragging");
  });
  handle.addEventListener("pointermove", (event) => {
    if (dragging) {
      setPanel(dragging.width + dragging.from - event.clientX, false);
    }
  });
  const stop = () => {
    if (dragging) {
      dragging = null;
      handle.classList.remove("dragging");
      setPanel(panelWidth());
    }
  };
  handle.addEventListener("pointerup", stop);
  handle.addEventListener("pointercancel", stop);
  handle.addEventListener("dblclick", () => setPanel(PANEL_DEFAULT));
  handle.addEventListener("keydown", (event) => {
    const step = event.shiftKey ? 64 : 16;
    if (event.key === "ArrowLeft" || event.key === "ArrowRight") {
      event.preventDefault();
      setPanel(panelWidth() + (event.key === "ArrowLeft" ? step : -step));
    }
  });
  window.addEventListener("resize", () => setPanel(panelWidth(), false));
}

function zoomTo(value, anchor = null) {
  const wrap = document.querySelector(".device-wrap");
  const before = canvas.getBoundingClientRect();
  const old = before.width / canvas.width || state.zoom;
  state.zoomChoice = Math.max(1, Math.min(8, Math.round(value * ZOOM_STEPS) / ZOOM_STEPS));
  $("zoom-level").textContent = `${state.zoomChoice}×`;
  fit(canvas.width, canvas.height);
  if (anchor) {
    const spotX = (anchor.x - before.left) / old;
    const spotY = (anchor.y - before.top) / old;
    const after = canvas.getBoundingClientRect();
    wrap.scrollLeft += after.left + spotX * state.zoom - anchor.x;
    wrap.scrollTop += after.top + spotY * state.zoom - anchor.y;
  }
}

function wireZoom() {
  const wrap = document.querySelector(".device-wrap");
  wrap.addEventListener("wheel", (event) => {
    if (!event.ctrlKey && !event.metaKey) {
      return;
    }
    event.preventDefault();
    zoomTo(state.zoom * Math.exp(-event.deltaY * 0.004), { x: event.clientX, y: event.clientY });
  }, { passive: false });
  let pan = null;
  wrap.addEventListener("pointerdown", (event) => {
    const onScreen = event.target === overlay || event.target === canvas;
    if (event.pointerType === "touch" || (onScreen && event.button !== 1)) {
      return;
    }
    pan = { x: event.clientX, y: event.clientY, left: wrap.scrollLeft, top: wrap.scrollTop };
    wrap.setPointerCapture(event.pointerId);
    wrap.classList.add("panning");
    event.preventDefault();
  });
  wrap.addEventListener("pointermove", (event) => {
    if (pan) {
      wrap.scrollLeft = pan.left - (event.clientX - pan.x);
      wrap.scrollTop = pan.top - (event.clientY - pan.y);
    }
  });
  const stop = () => {
    pan = null;
    wrap.classList.remove("panning");
  };
  wrap.addEventListener("pointerup", stop);
  wrap.addEventListener("pointercancel", stop);
  let pinch = null;
  const spread = (touches) => Math.hypot(touches[0].clientX - touches[1].clientX, touches[0].clientY - touches[1].clientY);
  wrap.addEventListener("touchstart", (event) => {
    if (event.touches.length === 2) {
      pinch = { gap: spread(event.touches), zoom: state.zoom };
    }
  }, { passive: true });
  wrap.addEventListener("touchmove", (event) => {
    if (pinch && event.touches.length === 2) {
      event.preventDefault();
      const middle = { x: (event.touches[0].clientX + event.touches[1].clientX) / 2, y: (event.touches[0].clientY + event.touches[1].clientY) / 2 };
      zoomTo(pinch.zoom * spread(event.touches) / pinch.gap, middle);
    }
  }, { passive: false });
  wrap.addEventListener("touchend", (event) => {
    if (event.touches.length < 2) {
      pinch = null;
    }
  });
}

function startTour(at = 0) {
  document.querySelector(".tour")?.remove();
  document.querySelector(".tour-target")?.classList.remove("tour-target");
  if (at >= TOUR.length) {
    try {
      localStorage.setItem(WELCOMED, "1");
    } catch {
      return;
    }
    return;
  }
  if (at === 2 && $("playback").hidden) {
    state.screen = "home";
    restart();
    render();
  }
  const [id, words] = TOUR[at];
  const target = $(id);
  target.classList.add("tour-target");
  const finish = () => startTour(TOUR.length);
  const bubble = el("div", { class: "tour", role: "dialog", "aria-label": "Getting started" },
    el("p", {}, words),
    el("div", { class: "tour-foot" },
      el("span", { class: "hint" }, `${at + 1} of ${TOUR.length}`),
      el("span", { class: "actions" },
        at < TOUR.length - 1 ? el("button", { type: "button", onclick: finish }, "Skip") : null,
        el("button", { type: "button", class: "primary", onclick: () => startTour(at + 1) }, at < TOUR.length - 1 ? "Next" : "Done"))));
  document.body.append(bubble);
  const spot = target.getBoundingClientRect();
  const box = bubble.getBoundingClientRect();
  const gap = 12;
  const across = Math.max(gap, Math.min(window.innerWidth - box.width - gap, spot.left + spot.width / 2 - box.width / 2));
  const down = Math.max(gap, Math.min(window.innerHeight - box.height - gap, spot.top + Math.min(spot.height / 2, 160) - box.height / 2));
  let place = [across, down];
  if (spot.bottom + gap + box.height < window.innerHeight) {
    place = [across, spot.bottom + gap];
  } else if (spot.right + gap + box.width < window.innerWidth) {
    place = [spot.right + gap, down];
  } else if (spot.left - gap - box.width > 0) {
    place = [spot.left - gap - box.width, down];
  } else if (spot.top - gap - box.height > 0) {
    place = [across, spot.top - gap - box.height];
  }
  bubble.style.left = `${place[0]}px`;
  bubble.style.top = `${place[1]}px`;
  bubble.querySelector(".primary").focus();
}

function welcome(returning) {
  let welcomed = true;
  try {
    welcomed = returning || Boolean(localStorage.getItem(WELCOMED));
    if (returning) {
      localStorage.setItem(WELCOMED, "1");
    }
  } catch {
    welcomed = true;
  }
  if (welcomed) {
    return;
  }
  const done = () => {
    card.remove();
    try {
      localStorage.setItem(WELCOMED, "1");
    } catch {
      return;
    }
  };
  const begin = async (sample) => {
    card.remove();
    if (sample !== FIRST_STARTS[0]) {
      await openSample(sample);
    }
    startTour(0);
  };
  const card = el("div", { class: "welcome", role: "dialog", "aria-label": "Welcome" },
    el("h2", {}, "Make your own EchoPod theme"),
    el("p", {}, "What would you like to begin from?"),
    el("div", { class: "starts" }, FIRST_STARTS.filter((sample) => state.schema.samples.includes(sample)).map((sample) =>
      el("button", { type: "button", onclick: () => begin(sample) },
        el("span", { class: "thumb", style: `background-image: url("previews/${sample}.png")` }),
        el("span", {}, sampleName(sample))))),
    el("p", { class: "hint" }, "Pick one and a short tour shows you around. Nothing to install, and your work is kept in this browser."),
    el("div", { class: "actions" },
      el("button", { type: "button", onclick: () => {
        done();
        showPanel("themes");
      } }, "More to begin from"),
      el("button", { type: "button", onclick: done }, "I'll find my way")));
  document.querySelector(".stage").append(card);
}

function wireOverlay() {
  overlay.addEventListener("pointerdown", (event) => {
    const point = pointer(event);
    if (!designing()) {
      event.preventDefault();
      const picture = pictureAt(point);
      const role = picture ? null : roleAt(point);
      state.spot = picture || role ? { role, picture, x: point.x, y: point.y } : null;
      renderBar();
      if (!state.spot) {
        say("Couldn't tell which colour that is. It may be part of a picture, such as the wallpaper or a cover.");
      }
      canvas.focus({ preventScroll: true });
      return;
    }
    state.panel = "look";
    if (drawer.down(point, { add: event.shiftKey || event.ctrlKey || event.metaKey })) {
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
      const cursor = drawer.move(pointer(event), { free: event.altKey });
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
  if (!designing() || !(event.key in steps) || event.target.closest?.("input, select, textarea, canvas")) {
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
  const licence = LICENCES.find(([value]) => value && value === state.theme.font_licence);
  const note = licence ? `# Fonts: ${licence[1]}\n` : "";
  save(`${slug(state.theme.name)}.toml`, note + toToml(state.theme, state.schema), "text/plain");
}

function save(name, bytes, type) {
  const link = el("a", { href: URL.createObjectURL(new Blob([bytes], { type })), download: name });
  link.click();
  URL.revokeObjectURL(link.href);
}

function wear(bytes) {
  settleHistory();
  keepNow("Before opening another theme over it");
  try {
    const theme = unpack(bytes, state.schema);
    state.theme = theme;
    state.designs = {
      playing: theme.now_playing_scene?.design ? drawingFrom(theme.now_playing_scene.design, "playing") : null,
      home: theme.home_scene?.design ? drawingFrom(theme.home_scene.design, "home") : null,
    };
    state.stash = {};
    state.picked = null;
    state.panel = "look";
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
  settleHistory();
  keepNow("Before starting over");
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
  $("save").addEventListener("click", () => showPanel("share"));
  $("themes").addEventListener("click", () => showPanel("themes"));
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
  document.addEventListener("pointerdown", (event) => {
    if (state.spot && event.target !== overlay && !event.target.closest?.("#bar")) {
      state.spot = null;
      renderBar();
    }
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && state.spot) {
      state.spot = null;
      renderBar();
    }
    if (nudge(event)) {
      return;
    }
    if (designing() && !event.target.closest?.("input, select, textarea, canvas") && drawer.key(event)) {
      event.preventDefault();
      return;
    }
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "z" && !event.target.closest?.("input, textarea")) {
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
  wirePanel();
  wireZoom();
  restart();
  state.history = [snapshot()];
  state.library = readLibrary(store);
  state.themeId = entryOf(state.library, state.library.current) ? state.library.current : newId(Date.now());
  keepNow();
  window.addEventListener("pagehide", () => {
    settleHistory();
    keepNow();
  });
  render();
  refreshUndo();
  welcome(Boolean(draft));
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
