const TURNSTILE = "https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit";
const VOTER = "echopod-voter";
const LIKED = "echopod-liked";
const UNREACHABLE = "Couldn't reach the gallery. Check your connection and try again.";
let checker = null;

export async function siteSettings(get = fetch) {
  try {
    const response = await get("site.json");
    const settings = response.ok ? await response.json() : {};
    return {
      service: typeof settings.service === "string" ? settings.service.replace(/\/+$/, "") : "",
      turnstile: typeof settings.turnstile === "string" ? settings.turnstile : "",
    };
  } catch {
    return { service: "", turnstile: "" };
  }
}

async function ask(service, method, path, body, get = fetch) {
  let response;
  try {
    response = await get(service + path, {
      method,
      headers: body ? { "Content-Type": "application/json" } : undefined,
      body: body ? JSON.stringify(body) : undefined,
    });
  } catch {
    throw new Error(UNREACHABLE);
  }
  let value = null;
  try {
    value = await response.json();
  } catch {
    value = null;
  }
  if (!response.ok) {
    throw new Error(typeof value?.error === "string" ? value.error : UNREACHABLE);
  }
  return value;
}

export async function likeCounts(service, get = fetch) {
  const { likes } = await ask(service, "GET", "/likes", null, get);
  return likes && typeof likes === "object" ? likes : {};
}

export function setLike(service, file, voter, liked, get = fetch) {
  return ask(service, liked ? "POST" : "DELETE", `/likes/${encodeURIComponent(file)}`, { voter }, get);
}

export function sendTheme(service, file, toml, token, get = fetch) {
  return ask(service, "POST", "/themes", { file, toml, token }, get);
}

export function voterId(storage) {
  try {
    const kept = storage.getItem(VOTER);
    if (kept) {
      return kept;
    }
    const made = crypto.randomUUID();
    storage.setItem(VOTER, made);
    return made;
  } catch {
    return crypto.randomUUID();
  }
}

export function likedFiles(storage) {
  try {
    const kept = JSON.parse(storage.getItem(LIKED) ?? "[]");
    return new Set(Array.isArray(kept) ? kept.filter((file) => typeof file === "string") : []);
  } catch {
    return new Set();
  }
}

export function keepLiked(storage, files) {
  try {
    storage.setItem(LIKED, JSON.stringify([...files]));
  } catch {
    return;
  }
}

export function byLikes(themes, likes) {
  return themes
    .map((theme, place) => ({ theme, place }))
    .sort((one, other) => (likes[other.theme.file] ?? 0) - (likes[one.theme.file] ?? 0) || one.place - other.place)
    .map((entry) => entry.theme);
}

function loadChecker() {
  checker ??= new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = TURNSTILE;
    script.async = true;
    script.addEventListener("load", () => resolve(window.turnstile));
    script.addEventListener("error", () => {
      checker = null;
      script.remove();
      reject(new Error("The check that you're a person couldn't load. Check your connection and try again."));
    });
    document.head.append(script);
  });
  return checker;
}

export async function personCheck(host, sitekey) {
  const turnstile = await loadChecker();
  return new Promise((resolve, reject) => {
    const widget = turnstile.render(host, {
      sitekey,
      appearance: "interaction-only",
      callback: (token) => {
        turnstile.remove(widget);
        resolve(token);
      },
      "error-callback": () => {
        turnstile.remove(widget);
        reject(new Error("Couldn't confirm you're a person. Try again."));
        return true;
      },
    });
  });
}
