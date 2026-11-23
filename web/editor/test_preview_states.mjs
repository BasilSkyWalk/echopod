import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { PLAYER, Preview, SONGS } from "./preview_host.js";

const site = process.argv[2];
const schema = JSON.parse(readFileSync(join(site, "theme_schema.json"), "utf-8"));
const preview = await Preview.load(async (name) => new Uint8Array(readFileSync(join(site, name))), schema);
const usual = { song: null, paused: false, at: null, volume: null, shuffle: 0, repeat: 0 };

function shown(screen, playback) {
  preview.start(0, screen, 0, 0, { ...usual, ...playback });
  for (let spent = 0; preview.busy && spent < 20000; spent += 100) {
    preview.advance(100);
  }
  assert.equal(preview.busy, false, `${screen} ${JSON.stringify(playback)} finishes its script`);
  preview.advance(700);
  return preview.status();
}

const first = shown("playing", {});
assert.equal(first.state, PLAYER.playing);
assert.ok(first.position < 2000);

const posed = shown("playing", { paused: true, at: 0.5, volume: 7, shuffle: 1, repeat: 2 });
assert.equal(posed.state, PLAYER.paused);
assert.ok(Math.abs(posed.position - posed.duration / 2) < 1000, "paused half way through");
assert.deepEqual([posed.volume, posed.shuffle, posed.repeat], [7, 1, 2]);

const songs = Object.fromEntries(Object.keys(SONGS).map((key) => [key, shown("playing", { song: key }).duration]));
assert.equal(new Set(Object.values(songs)).size, Object.keys(SONGS).length, "each song is a different track");

const home = shown("home", { song: "usual", paused: true, at: 0.25 });
assert.equal(home.state, PLAYER.paused);
assert.equal(home.duration, first.duration);
assert.equal(shown("home", {}).state, PLAYER.idle);

const before = shown("playing", { volume: 0 });
preview.nudgeVolume();
assert.equal(preview.status().volume, before.volume, "showing the volume change leaves the volume where it was");

preview.start(0, "playing", 0, 0, usual);
for (let spent = 0; preview.busy && spent < 20000; spent += 100) {
  preview.advance(100);
}
preview.press(2);
preview.seek(0.75);
const sought = preview.status();
assert.equal(sought.state, PLAYER.paused, "moving through a paused song leaves it paused");
assert.ok(Math.abs(sought.position - sought.duration * 0.75) < 1000);

const files = JSON.parse(readFileSync(join(site, "demo_files.json"), "utf-8"));
for (const { file } of files) {
  assert.ok(readFileSync(join(site, file)).length > 0, file);
}
console.log("ok: preview states");
