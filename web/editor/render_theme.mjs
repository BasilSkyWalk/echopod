import { mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { crc32, deflateSync } from "node:zlib";
import { Preview } from "./preview_host.js";

const GAP = 8;
const LONG = 320;
const SHORT = 170;
const PAPER = [28, 30, 36, 255];
const SCRIPT_LIMIT_MS = 20000;
const SETTLE_MS = 700;
const HELD_MS = 9000;
const LANDSCAPE = ["home", "list", "playing", "toast", "letters", "settings"];
const PORTRAIT = ["list", "playing", "toast"];
const COLUMNS = 3;
const SUFFIX = ".echotheme";
const THEME_FACE = 5;

function shoot(preview, orientation, screen) {
  preview.start(orientation, screen, 0, THEME_FACE);
  if (screen === "letters") {
    preview.advance(HELD_MS);
  } else {
    for (let spent = 0; preview.busy && spent < SCRIPT_LIMIT_MS; spent += 100) {
      preview.advance(100);
    }
    preview.advance(SETTLE_MS);
  }
  const { width, height, pixels } = preview.frame();
  return { width, height, pixels: Uint8Array.from(pixels) };
}

function paste(sheet, shot, left, top) {
  for (let row = 0; row < shot.height; row++) {
    const from = row * shot.width * 4;
    const to = ((top + row) * sheet.width + left) * 4;
    sheet.pixels.set(shot.pixels.subarray(from, from + shot.width * 4), to);
  }
}

function blankSheet(width, height) {
  const pixels = new Uint8Array(width * height * 4);
  for (let index = 0; index < pixels.length; index += 4) {
    pixels.set(PAPER, index);
  }
  return { width, height, pixels };
}

function chunk(kind, body) {
  const named = Buffer.concat([Buffer.from(kind, "latin1"), body]);
  const sized = Buffer.alloc(4);
  const checked = Buffer.alloc(4);
  sized.writeUInt32BE(body.length);
  checked.writeUInt32BE(crc32(named));
  return Buffer.concat([sized, named, checked]);
}

function png(sheet) {
  const stride = sheet.width * 4;
  const rows = Buffer.alloc((stride + 1) * sheet.height);
  for (let row = 0; row < sheet.height; row++) {
    Buffer.from(sheet.pixels.buffer, row * stride, stride).copy(rows, row * (stride + 1) + 1);
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(sheet.width, 0);
  header.writeUInt32BE(sheet.height, 4);
  header.set([8, 6, 0, 0, 0], 8);
  const magic = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
  return Buffer.concat([magic, chunk("IHDR", header), chunk("IDAT", deflateSync(rows)), chunk("IEND", Buffer.alloc(0))]);
}

function render(preview, themeFile, output) {
  if (!preview.wear(new Uint8Array(readFileSync(themeFile)))) {
    console.error(`error: ${themeFile} is not a theme this firmware accepts`);
    return false;
  }
  const rows = Math.ceil(LANDSCAPE.length / COLUMNS);
  const sheet = blankSheet(COLUMNS * (LONG + GAP) + GAP, rows * (SHORT + GAP) + LONG + 2 * GAP);
  LANDSCAPE.forEach((screen, index) => {
    const left = GAP + (index % COLUMNS) * (LONG + GAP);
    paste(sheet, shoot(preview, 0, screen), left, GAP + Math.floor(index / COLUMNS) * (SHORT + GAP));
  });
  PORTRAIT.forEach((screen, index) => {
    paste(sheet, shoot(preview, 1, screen), GAP + index * (SHORT + GAP), GAP + rows * (SHORT + GAP));
  });
  writeFileSync(output, png(sheet));
  console.log(`wrote ${output}: ${sheet.width}x${sheet.height}`);
  return true;
}

function everyTheme(site) {
  const folder = join(site, "previews");
  mkdirSync(folder, { recursive: true });
  return readdirSync(join(site, "themes"))
    .filter((name) => name.endsWith(SUFFIX))
    .sort()
    .map((name) => [join(site, "themes", name), join(folder, `${basename(name, SUFFIX)}.png`)]);
}

async function main() {
  const [site, themeFile, output] = process.argv.slice(2);
  if (!site || Boolean(themeFile) !== Boolean(output)) {
    console.error("usage: node render_theme.mjs <site folder> [<theme.echotheme> <out.png>]");
    return 2;
  }
  const schema = JSON.parse(readFileSync(join(site, "theme_schema.json"), "utf-8"));
  const preview = await Preview.load(async (name) => new Uint8Array(readFileSync(join(site, name))), schema);
  const jobs = themeFile ? [[themeFile, output]] : everyTheme(site);
  const drawn = jobs.map(([from, to]) => render(preview, from, to));
  return drawn.every(Boolean) ? 0 : 1;
}

process.exitCode = await main();
