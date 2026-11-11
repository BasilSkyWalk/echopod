import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { ThemeError, checksum, defaultTheme, hexColor, pack, rgb565, toToml, unpack } from "./theme_format.js";

const site = process.argv[2];
const schema = JSON.parse(readFileSync(join(site, "theme_schema.json"), "utf-8"));
const packages = readdirSync(join(site, "themes")).filter((name) => name.endsWith(".echotheme"));

assert.ok(packages.length >= 2);
for (const name of packages) {
  const bytes = new Uint8Array(readFileSync(join(site, "themes", name)));
  const theme = unpack(bytes, schema);
  assert.deepEqual(Array.from(pack(theme, schema)), Array.from(bytes), `${name} repacks to itself`);
}

const dark = new Uint8Array(readFileSync(join(site, "themes", "dark.echotheme")));
const fresh = defaultTheme(schema);
fresh.name = "Dark";
assert.deepEqual(Array.from(pack(fresh, schema)), Array.from(dark), "the editor's starting theme is Dark");

const ember = unpack(new Uint8Array(readFileSync(join(site, "themes", "ember.echotheme"))), schema);
assert.equal(ember.name, "Ember");
assert.equal(ember.spacing.row_height, 28);
assert.equal(ember.spacing.scrollbar_width, null);
assert.equal(ember.boxes.toast.radius, 8);
assert.equal(ember.boxes.selection.border_edges, 4);
assert.equal(ember.colors.selection_top, "#ffa231");

assert.equal(checksum(new Uint8Array([])), 0x811c9dc5);
assert.equal(checksum(new TextEncoder().encode("a")), 0xe40c292c);
assert.equal(rgb565("#f2f2f5"), 0xf79e);
assert.equal(rgb565(hexColor(0x1082)), 0x1082);

const edited = { ...fresh, name: "x".repeat(24) };
assert.throws(() => pack(edited, schema), ThemeError);
assert.throws(() => pack({ ...fresh, spacing: { ...fresh.spacing, row_height: 41 } }, schema), ThemeError);
assert.throws(() => unpack(dark.subarray(0, dark.length - 1), schema), ThemeError);
const flipped = dark.slice();
flipped[60] ^= 1;
assert.throws(() => unpack(flipped, schema), ThemeError);

const source = toToml(ember, schema);
assert.match(source, /^name = "Ember"$/m);
assert.match(source, /^row_height = 28$/m);
assert.match(source, /^\[boxes\.toast\]$/m);
assert.match(source, /^radius = 8$/m);
console.log(`ok: ${packages.length} themes repack to themselves`);
