export const LIBRARY = "echopod-themes";
export const VERSION_LIMIT = 12;
export const VERSION_EVERY_MS = 10 * 60 * 1000;

export function emptyLibrary() {
  return { current: null, themes: [] };
}

export function readLibrary(store) {
  try {
    const found = JSON.parse(store.getItem(LIBRARY) ?? "null");
    if (found && Array.isArray(found.themes)) {
      return found;
    }
  } catch {
    return emptyLibrary();
  }
  return emptyLibrary();
}

function trimmed(library) {
  const older = library.themes
    .flatMap((entry) => entry.versions.map((version) => ({ entry, version })))
    .sort((a, b) => a.version.at - b.version.at);
  if (!older.length) {
    return false;
  }
  const { entry, version } = older[0];
  entry.versions.splice(entry.versions.indexOf(version), 1);
  return true;
}

export function writeLibrary(store, library) {
  for (;;) {
    try {
      store.setItem(LIBRARY, JSON.stringify(library));
      return true;
    } catch {
      if (!trimmed(library)) {
        return false;
      }
    }
  }
}

export function newId(now) {
  return `t${now.toString(36)}${Math.floor(Math.random() * 1296).toString(36).padStart(2, "0")}`;
}

export function entryOf(library, id) {
  return library.themes.find((entry) => entry.id === id) ?? null;
}

export function keep(library, id, snapshot, name, now) {
  let entry = entryOf(library, id);
  if (!entry) {
    entry = { id, name, updated: now, snapshot, versions: [] };
    library.themes.unshift(entry);
  }
  if (entry.snapshot !== snapshot || entry.name !== name) {
    entry.snapshot = snapshot;
    entry.name = name;
    entry.updated = now;
  }
  library.current = id;
  return entry;
}

export function addVersion(library, id, now, label = "") {
  const entry = entryOf(library, id);
  if (!entry || entry.versions.at(-1)?.snapshot === entry.snapshot) {
    return false;
  }
  entry.versions.push({ at: now, label, snapshot: entry.snapshot });
  entry.versions.splice(0, Math.max(0, entry.versions.length - VERSION_LIMIT));
  return true;
}

export function versionDue(library, id, now) {
  const entry = entryOf(library, id);
  return Boolean(entry) && (!entry.versions.length || now - entry.versions.at(-1).at >= VERSION_EVERY_MS);
}

export function removeTheme(library, id) {
  library.themes = library.themes.filter((entry) => entry.id !== id);
  if (library.current === id) {
    library.current = library.themes[0]?.id ?? null;
  }
}

export function copyTheme(library, id, now, name) {
  const entry = entryOf(library, id);
  if (!entry) {
    return null;
  }
  const saved = JSON.parse(entry.snapshot);
  saved.theme = { ...saved.theme, name };
  const copy = { id: newId(now), name, updated: now, snapshot: JSON.stringify(saved), versions: [] };
  library.themes.unshift(copy);
  return copy;
}
