import { byLikes, keepLiked, likeCounts, likedFiles, setLike, siteSettings, voterId } from "./gallery_service.js";

const HEART = "M10 16.5S3.5 12.6 3.5 7.9A3.4 3.4 0 0 1 10 6.4a3.4 3.4 0 0 1 6.5 1.5c0 4.7-6.5 8.6-6.5 8.6z";
const TROUBLE_MS = 6000;
const status = document.getElementById("status");
const gallery = document.getElementById("gallery");
const sorting = document.getElementById("sorting");
const view = { themes: [], likes: null, liked: new Set(), service: "", storage: null, said: "", timer: 0 };

function element(tag, text, className) {
  const made = document.createElement(tag);
  made.textContent = text ?? "";
  if (className) {
    made.className = className;
  }
  return made;
}

function link(text, href, download) {
  const made = element("a", text);
  made.href = href;
  if (download) {
    made.download = download;
  }
  return made;
}

function picture(theme) {
  const image = document.createElement("img");
  image.src = `previews/${theme.file}.png`;
  image.alt = `${theme.name} on the player's screens`;
  image.loading = "lazy";
  image.addEventListener("error", () => image.remove());
  return image;
}

function heart() {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 20 20");
  svg.setAttribute("aria-hidden", "true");
  const line = document.createElementNS("http://www.w3.org/2000/svg", "path");
  line.setAttribute("d", HEART);
  svg.append(line);
  return svg;
}

function trouble(text) {
  clearTimeout(view.timer);
  status.textContent = text;
  status.classList.add("bad");
  view.timer = setTimeout(() => {
    status.textContent = view.said;
    status.classList.remove("bad");
  }, TROUBLE_MS);
}

function likeButton(theme) {
  const liked = view.liked.has(theme.file);
  const count = view.likes[theme.file] ?? 0;
  const made = element("button", null, "like");
  made.type = "button";
  made.dataset.file = theme.file;
  made.setAttribute("aria-pressed", String(liked));
  made.setAttribute("aria-label", `Like ${theme.name}, ${count} so far`);
  made.title = liked ? "You like this. Click to take it back" : "Like this theme";
  made.append(heart(), element("span", count > 0 ? String(count) : ""));
  made.addEventListener("click", () => toggle(theme));
  return made;
}

function show(theme, liked, count) {
  view.likes[theme.file] = Math.max(0, count);
  if (liked) {
    view.liked.add(theme.file);
  } else {
    view.liked.delete(theme.file);
  }
  keepLiked(view.storage, view.liked);
  const shown = gallery.querySelector(`.like[data-file="${theme.file}"]`);
  const focused = shown === document.activeElement;
  const next = likeButton(theme);
  shown?.replaceWith(next);
  if (focused) {
    next.focus();
  }
}

async function toggle(theme) {
  const liked = !view.liked.has(theme.file);
  const before = view.likes[theme.file] ?? 0;
  show(theme, liked, before + (liked ? 1 : -1));
  try {
    const now = await setLike(view.service, theme.file, voterId(view.storage), liked);
    show(theme, liked, now.likes);
  } catch (error) {
    show(theme, !liked, before);
    trouble(error.message);
  }
}

function card(theme) {
  const made = element("article", null, "card");
  const by = theme.author ? `by ${theme.author}` : "";
  const head = element("div", null, "card-head");
  const links = element("p", null, "links");
  links.append(
    link("Download", `themes/${theme.file}.echotheme`, `${theme.name}.echotheme`),
    link("Open in the editor", `index.html?theme=${encodeURIComponent(theme.file)}`),
    link("Editable copy", `themes/${theme.file}.toml`, `${theme.file}.toml`),
  );
  head.append(element("h2", theme.name));
  if (view.likes) {
    head.append(likeButton(theme));
  }
  made.append(picture(theme), head, element("p", by, "by"));
  made.append(element("p", theme.description), links);
  return made;
}

function draw() {
  const ordered = view.likes && sorting.value === "liked" ? byLikes(view.themes, view.likes) : view.themes;
  gallery.replaceChildren(...ordered.map(card));
}

async function loadLikes() {
  const { service } = await siteSettings();
  if (!service) {
    return;
  }
  try {
    view.likes = await likeCounts(service);
  } catch {
    return;
  }
  try {
    view.storage = window.localStorage;
  } catch {
    view.storage = null;
  }
  view.service = service;
  view.liked = likedFiles(view.storage);
  sorting.closest("label").hidden = false;
  sorting.addEventListener("change", draw);
  draw();
}

async function main() {
  const response = await fetch("themes/index.json");
  if (!response.ok) {
    throw new Error(`themes/index.json: ${response.status}`);
  }
  const themes = await response.json();
  const shared = themes.filter((theme) => theme.shared);
  const own = themes.filter((theme) => !theme.shared);
  view.themes = [...shared, ...own];
  view.said = `${themes.length} themes`;
  draw();
  status.textContent = view.said;
  await loadLikes();
}

main().catch((error) => {
  status.textContent = `Could not load the themes: ${error.message}`;
  status.classList.add("bad");
});
