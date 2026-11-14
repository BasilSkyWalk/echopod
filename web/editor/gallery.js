const status = document.getElementById("status");
const gallery = document.getElementById("gallery");

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

function card(theme) {
  const made = element("article", null, "card");
  const by = theme.author ? `by ${theme.author}` : "";
  const links = element("p", null, "links");
  links.append(
    link("Download", `themes/${theme.file}.echotheme`, `${theme.name}.echotheme`),
    link("Open in the editor", `index.html?theme=${encodeURIComponent(theme.file)}`),
    link("Source", `themes/${theme.file}.toml`, `${theme.file}.toml`),
  );
  made.append(picture(theme), element("h2", theme.name), element("p", by, "by"));
  made.append(element("p", theme.description), links);
  return made;
}

async function main() {
  const response = await fetch("themes/index.json");
  if (!response.ok) {
    throw new Error(`themes/index.json: ${response.status}`);
  }
  const themes = await response.json();
  const shared = themes.filter((theme) => theme.shared);
  const own = themes.filter((theme) => !theme.shared);
  gallery.replaceChildren(...[...shared, ...own].map(card));
  status.textContent = `${themes.length} themes`;
}

main().catch((error) => {
  status.textContent = `Could not load the themes: ${error.message}`;
  status.classList.add("bad");
});
