import { ServiceError, openSubmission } from "./github.js";

const SITEVERIFY = "https://challenges.cloudflare.com/turnstile/v0/siteverify";
const FILE_NAME = /^[a-z0-9][a-z0-9_-]{0,22}$/;
const VOTER = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const WORDS = { name: 64, author: 40, description: 120 };
const BODY_LIMIT = 700_000;
const SOURCE_LIMIT = 600_000;
const SMALL_BODY_LIMIT = 400;
const TOKEN_LIMIT = 2048;
const LIKES_A_DAY = 60;
const LIKES_A_THEME = 5;
const SENT_A_DAY = 3;
const QUEUE_LIMIT = 20;
const INDEX_CACHE_SECONDS = 300;
const ADDRESS_LETTERS = 32;
const NOT_SET_UP = "The gallery isn't set up to take this yet.";

function allowedOrigin(request, env) {
  const origin = request.headers.get("Origin");
  const allowed = (env.ALLOWED_ORIGINS ?? "").split(",").map((entry) => entry.trim()).filter(Boolean);
  return origin && allowed.includes(origin) ? origin : null;
}

function reply(status, value, origin) {
  const headers = { "Cache-Control": "no-store", Vary: "Origin" };
  if (origin) {
    headers["Access-Control-Allow-Origin"] = origin;
  }
  if (value === null) {
    return new Response(null, { status, headers });
  }
  return new Response(JSON.stringify(value), {
    status,
    headers: { ...headers, "Content-Type": "application/json; charset=utf-8" },
  });
}

function preflight(origin) {
  if (!origin) {
    return reply(403, { error: "This page can't use the gallery." }, null);
  }
  return new Response(null, {
    status: 204,
    headers: {
      "Access-Control-Allow-Origin": origin,
      "Access-Control-Allow-Methods": "GET, POST, DELETE",
      "Access-Control-Allow-Headers": "Content-Type",
      "Access-Control-Max-Age": "86400",
      Vary: "Origin",
    },
  });
}

async function readJson(request, limit) {
  if (Number(request.headers.get("Content-Length") ?? 0) > limit || !request.body) {
    throw new ServiceError(request.body ? 413 : 400, request.body ? "That's too big to send." : "Nothing was sent.");
  }
  const reader = request.body.getReader();
  const chunks = [];
  let size = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    size += value.length;
    if (size > limit) {
      await reader.cancel();
      throw new ServiceError(413, "That's too big to send.");
    }
    chunks.push(value);
  }
  const bytes = new Uint8Array(size);
  let at = 0;
  for (const chunk of chunks) {
    bytes.set(chunk, at);
    at += chunk.length;
  }
  try {
    const value = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(bytes));
    if (value && typeof value === "object" && !Array.isArray(value)) {
      return value;
    }
  } catch {
    throw new ServiceError(400, "That couldn't be read.");
  }
  throw new ServiceError(400, "That couldn't be read.");
}

function today() {
  return new Date().toISOString().slice(0, 10);
}

async function addressOf(request, env) {
  if (!env.VOTER_SALT) {
    throw new ServiceError(503, NOT_SET_UP);
  }
  const address = request.headers.get("CF-Connecting-IP") ?? "unknown";
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(`${env.VOTER_SALT}:${address}`));
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("")
    .slice(0, ADDRESS_LETTERS);
}

async function galleryFiles(env, net) {
  const response = await net(env.GALLERY_INDEX, { cf: { cacheTtl: INDEX_CACHE_SECONDS, cacheEverything: true } });
  if (!response.ok) {
    throw new ServiceError(503, "The gallery's list couldn't be read. Try again in a minute.");
  }
  const listed = await response.json();
  return new Set(Array.isArray(listed) ? listed.map((theme) => theme?.file) : []);
}

function recount(env, file) {
  return env.DB.prepare(
    "INSERT INTO counts (theme, likes) SELECT ?1, COUNT(*) FROM likes WHERE theme = ?1 " +
    "ON CONFLICT (theme) DO UPDATE SET likes = excluded.likes").bind(file);
}

function countOf(env, file) {
  return env.DB.prepare("SELECT likes FROM counts WHERE theme = ?1").bind(file);
}

async function likes(env) {
  const { results } = await env.DB.prepare("SELECT theme, likes FROM counts WHERE likes > 0").all();
  return { likes: Object.fromEntries(results.map((row) => [row.theme, row.likes])) };
}

async function setLike(request, env, net, file, liked) {
  const { voter } = await readJson(request, SMALL_BODY_LIMIT);
  if (!FILE_NAME.test(file) || typeof voter !== "string" || !VOTER.test(voter)) {
    throw new ServiceError(400, "That couldn't be read.");
  }
  const address = await addressOf(request, env);
  const change = [];
  if (liked) {
    if (!(await galleryFiles(env, net)).has(file)) {
      throw new ServiceError(404, "The gallery has no theme with this name.");
    }
    const day = today();
    const seen = await env.DB.prepare(
      "SELECT COUNT(*) AS today, COALESCE(SUM(theme = ?2), 0) AS here FROM likes WHERE address = ?1 AND day = ?3")
      .bind(address, file, day).first();
    if (seen.today >= LIKES_A_DAY || seen.here >= LIKES_A_THEME) {
      throw new ServiceError(429, "That's a lot of likes from one place today. Try again tomorrow.");
    }
    change.push(env.DB.prepare("INSERT OR IGNORE INTO likes (theme, voter, address, day) VALUES (?1, ?2, ?3, ?4)")
      .bind(file, voter, address, day));
  } else {
    change.push(env.DB.prepare("DELETE FROM likes WHERE theme = ?1 AND voter = ?2").bind(file, voter));
  }
  const done = await env.DB.batch([...change, recount(env, file), countOf(env, file)]);
  return { file, likes: done[done.length - 1].results[0]?.likes ?? 0 };
}

function topValue(source, key) {
  const top = source.split(/^\[/m)[0];
  const match = new RegExp(`^${key}[ \\t]*=[ \\t]*("(?:[^"\\\\\\n]|\\\\.)*")[ \\t\\r]*$`, "m").exec(top);
  if (!match) {
    return "";
  }
  try {
    const value = JSON.parse(match[1]);
    return typeof value === "string" ? value.trim() : "";
  } catch {
    return "";
  }
}

function plain(text) {
  return text.replace(/[\u0000-\u001f\u007f]/g, " ").replaceAll("`", "'").replace(/\s+/g, " ").trim();
}

export function described(file, source) {
  if (typeof file !== "string" || !FILE_NAME.test(file)) {
    throw new ServiceError(400, "Give the theme a name made of letters or numbers first.");
  }
  if (typeof source !== "string" || source.includes("\u0000")) {
    throw new ServiceError(400, "That couldn't be read.");
  }
  if (new TextEncoder().encode(source).length > SOURCE_LIMIT) {
    throw new ServiceError(413, "That's too big to send.");
  }
  const about = Object.fromEntries(Object.keys(WORDS).map((key) => [key, plain(topValue(source, key))]));
  for (const [key, most] of Object.entries(WORDS)) {
    if (about[key] === "" || Array.from(about[key]).length > most) {
      throw new ServiceError(400, "A theme for the gallery needs a name, who made it and a line about it.");
    }
  }
  return about;
}

async function person(env, net, token, request) {
  if (typeof token !== "string" || token === "" || token.length > TOKEN_LIMIT) {
    throw new ServiceError(403, "Couldn't confirm you're a person. Try again.");
  }
  const response = await net(SITEVERIFY, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      secret: env.TURNSTILE_SECRET.trim(),
      response: token,
      remoteip: request.headers.get("CF-Connecting-IP") ?? undefined,
    }),
  });
  const checked = response.ok ? await response.json() : null;
  if (!checked?.success) {
    throw new ServiceError(403, "Couldn't confirm you're a person. Try again.");
  }
}

async function send(request, env, net) {
  const needed = ["TURNSTILE_SECRET", "GITHUB_APP_ID", "GITHUB_INSTALLATION_ID", "GITHUB_APP_PRIVATE_KEY",
    "GITHUB_REPOSITORY", "GITHUB_BRANCH"];
  if (needed.some((key) => !env[key])) {
    throw new ServiceError(503, NOT_SET_UP);
  }
  const { file, toml, token } = await readJson(request, BODY_LIMIT);
  const about = described(file, toml);
  const address = await addressOf(request, env);
  const day = today();
  const before = await env.DB.prepare("SELECT COUNT(*) AS sent FROM submissions WHERE address = ?1 AND day = ?2")
    .bind(address, day).first();
  if (before.sent >= (Number(env.SENT_A_DAY) || SENT_A_DAY)) {
    throw new ServiceError(429, "That's as many themes as one place can send in a day. Try again tomorrow.");
  }
  await person(env, net, token, request);
  const pull = await openSubmission({
    appId: env.GITHUB_APP_ID,
    installationId: env.GITHUB_INSTALLATION_ID,
    privateKey: env.GITHUB_APP_PRIVATE_KEY,
    repository: env.GITHUB_REPOSITORY,
    branch: env.GITHUB_BRANCH,
    queueLimit: QUEUE_LIMIT,
  }, net, {
    file,
    toml,
    message: `feat: add the ${file} theme`,
    title: `feat: add the ${file} theme to the gallery`,
    body: [
      "A theme sent from the editor.",
      "",
      `- File: \`themes/${file}.toml\``,
      `- Name: \`${about.name}\``,
      `- Made by: \`${about.author}\``,
      `- About: \`${about.description}\``,
      "",
      "The `themes` workflow checks that it packs and reads well, then shows its picture in a comment here.",
    ].join("\n"),
  });
  await env.DB.prepare("INSERT INTO submissions (address, day, file, pull) VALUES (?1, ?2, ?3, ?4)")
    .bind(address, day, file, pull.number).run();
  console.log(JSON.stringify({ message: "theme sent", file, pull: pull.number }));
  return pull;
}

export async function handle(request, env, net) {
  const origin = allowedOrigin(request, env);
  const url = new URL(request.url);
  const liking = /^\/likes\/([^/]+)$/.exec(url.pathname);
  try {
    if (request.method === "OPTIONS") {
      return preflight(origin);
    }
    if (request.method === "GET" && url.pathname === "/likes") {
      return reply(200, await likes(env), origin);
    }
    if (request.method === "GET" || !origin) {
      return reply(origin || request.method === "GET" ? 404 : 403, { error: "There's nothing here." }, origin);
    }
    if (liking && (request.method === "POST" || request.method === "DELETE")) {
      return reply(200, await setLike(request, env, net, liking[1], request.method === "POST"), origin);
    }
    if (request.method === "POST" && url.pathname === "/themes") {
      return reply(201, await send(request, env, net), origin);
    }
    return reply(404, { error: "There's nothing here." }, origin);
  } catch (error) {
    if (error instanceof ServiceError) {
      return reply(error.status, { error: error.message }, origin);
    }
    console.error(JSON.stringify({
      message: "request failed",
      path: url.pathname,
      error: error instanceof Error ? error.message : String(error),
    }));
    return reply(500, { error: "Something went wrong. Try again in a minute." }, origin);
  }
}

export default {
  async fetch(request, env) {
    return handle(request, env, (input, init) => fetch(input, init));
  },
};
