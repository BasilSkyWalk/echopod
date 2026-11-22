import assert from "node:assert/strict";
import { generateKeyPairSync } from "node:crypto";
import { byLikes, keepLiked, likeCounts, likedFiles, sendTheme, setLike, siteSettings, voterId } from "./gallery_service.js";
import { fakeGitHub, localDatabase } from "../gallery-service/local.mjs";
import { handle } from "../gallery-service/worker.js";

const SERVICE = "https://gallery.example";
const ORIGIN = "https://themes.example";
const INDEX = `${ORIGIN}/themes/index.json`;
const AMBER = 'name = "Amber"\nauthor = "Van Zorn"\ndescription = "Phosphor, in amber"\n\n[colors]\ntext = "#ffb000"\n';

function storage() {
  const kept = new Map();
  return { getItem: (key) => kept.get(key) ?? null, setItem: (key, value) => kept.set(key, String(value)) };
}

const locked = {
  getItem() {
    throw new Error("blocked");
  },
  setItem() {
    throw new Error("blocked");
  },
};

const page = (value, ok = true) => async () => ({ ok, json: async () => value });
assert.deepEqual(await siteSettings(page({ service: "https://gallery.example//", turnstile: "0x4" })),
  { service: SERVICE, turnstile: "0x4" });
assert.deepEqual(await siteSettings(page({ service: 4 })), { service: "", turnstile: "" });
assert.deepEqual(await siteSettings(page({}, false)), { service: "", turnstile: "" });
assert.deepEqual(await siteSettings(async () => {
  throw new Error("offline");
}), { service: "", turnstile: "" });

const mine = storage();
const voter = voterId(mine);
assert.match(voter, /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/);
assert.equal(voterId(mine), voter);
assert.match(voterId(locked), /^[0-9a-f-]{36}$/);
assert.match(voterId(null), /^[0-9a-f-]{36}$/);
assert.deepEqual([...likedFiles(mine)], []);
keepLiked(mine, new Set(["mosaic", "dark"]));
assert.deepEqual([...likedFiles(mine)], ["mosaic", "dark"]);
mine.setItem("echopod-liked", "{");
assert.deepEqual([...likedFiles(mine)], []);
assert.deepEqual([...likedFiles(locked)], []);
keepLiked(locked, new Set(["mosaic"]));
keepLiked(null, new Set(["mosaic"]));

const themes = [{ file: "aurora" }, { file: "deck" }, { file: "mosaic" }, { file: "dark" }];
assert.deepEqual(byLikes(themes, { mosaic: 3, dark: 3, deck: 1 }).map((theme) => theme.file),
  ["mosaic", "dark", "deck", "aurora"]);
assert.deepEqual(byLikes(themes, {}), themes);

const github = fakeGitHub("someone/player", ["themes/mosaic.toml"]);
const env = {
  DB: localDatabase(),
  ALLOWED_ORIGINS: ORIGIN,
  GALLERY_INDEX: INDEX,
  GITHUB_REPOSITORY: "someone/player",
  GITHUB_BRANCH: "main",
  GITHUB_APP_ID: "1",
  GITHUB_INSTALLATION_ID: "1",
  GITHUB_APP_PRIVATE_KEY: generateKeyPairSync("rsa", { modulusLength: 2048 }).privateKey
    .export({ type: "pkcs1", format: "pem" }),
  TURNSTILE_SECRET: "secret",
  VOTER_SALT: "salt",
};
const net = async (input, init) => {
  if (String(input) === INDEX) {
    return new Response(JSON.stringify(themes));
  }
  if (String(input).includes("/siteverify")) {
    return new Response(JSON.stringify({ success: JSON.parse(init.body).response === "person" }));
  }
  return github.net(input, init);
};
const browser = (input, init = {}) => handle(new Request(input, {
  ...init,
  headers: { ...init.headers, Origin: ORIGIN, "CF-Connecting-IP": "203.0.113.9" },
}), env, net);

assert.deepEqual(await likeCounts(SERVICE, browser), {});
assert.deepEqual(await setLike(SERVICE, "mosaic", voter, true, browser), { file: "mosaic", likes: 1 });
assert.deepEqual(await likeCounts(SERVICE, browser), { mosaic: 1 });
assert.deepEqual(await setLike(SERVICE, "mosaic", voter, false, browser), { file: "mosaic", likes: 0 });
await assert.rejects(setLike(SERVICE, "nothing", voter, true, browser), { message: "The gallery has no theme with this name." });

const sent = await sendTheme(SERVICE, "amber", AMBER, "person", browser);
assert.deepEqual(sent, { number: 1, url: "https://github.com/someone/player/pull/1" });
assert.equal(github.saved.text, AMBER);
await assert.rejects(sendTheme(SERVICE, "mosaic", AMBER, "person", browser), /already has a theme with this name/);
await assert.rejects(sendTheme(SERVICE, "ember", AMBER, "robot", browser), /confirm you're a person/);
await assert.rejects(likeCounts(SERVICE, async () => {
  throw new TypeError("offline");
}), /Couldn't reach the gallery/);
await assert.rejects(likeCounts(SERVICE, async () => new Response("<html>", { status: 502 })), /Couldn't reach the gallery/);

console.log("gallery pages: ok");
