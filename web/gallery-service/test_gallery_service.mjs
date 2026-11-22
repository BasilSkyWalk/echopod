import assert from "node:assert/strict";
import { generateKeyPairSync, verify } from "node:crypto";
import { appToken } from "./github.js";
import { fakeGitHub, localDatabase } from "./local.mjs";
import { described, handle } from "./worker.js";

const ORIGIN = "https://themes.example";
const REPOSITORY = "someone/player";
const INDEX = "https://themes.example/themes/index.json";
const SITEVERIFY = "https://challenges.cloudflare.com/turnstile/v0/siteverify";
const VOTERS = ["11111111-1111-4111-8111-111111111111", "22222222-2222-4222-8222-222222222222"];
const AMBER = 'name = "Amber"\nauthor = "Van `Zorn`"\ndescription = "Phosphor, in amber"\nselection = "row"\n\n[colors]\ntext = "#ffb000"\n';
const keys = generateKeyPairSync("rsa", { modulusLength: 2048 });
const PKCS1 = keys.privateKey.export({ type: "pkcs1", format: "pem" });
const PKCS8 = keys.privateKey.export({ type: "pkcs8", format: "pem" });

function world(taken = ["assets/themes/dark.toml", "themes/mosaic.toml"]) {
  const github = fakeGitHub(REPOSITORY, taken);
  const env = {
    DB: localDatabase(),
    ALLOWED_ORIGINS: `${ORIGIN}, http://127.0.0.1:8770`,
    GALLERY_INDEX: INDEX,
    GITHUB_REPOSITORY: REPOSITORY,
    GITHUB_BRANCH: "main",
    GITHUB_APP_ID: "4242",
    GITHUB_INSTALLATION_ID: "77",
    GITHUB_APP_PRIVATE_KEY: PKCS1,
    TURNSTILE_SECRET: "secret",
    VOTER_SALT: "salt",
  };
  const net = async (input, init) => {
    if (String(input) === INDEX) {
      return new Response(JSON.stringify([{ file: "dark" }, { file: "mosaic" }]));
    }
    if (String(input) === SITEVERIFY) {
      const sent = JSON.parse(init.body);
      return new Response(JSON.stringify({ success: sent.secret === "secret" && sent.response === "person" }));
    }
    return github.net(input, init);
  };
  const ask = async (method, path, body, headers = {}) => {
    const response = await handle(new Request(`https://gallery.example${path}`, {
      method,
      headers: { Origin: ORIGIN, "CF-Connecting-IP": "203.0.113.9", ...headers },
      body: body === undefined ? undefined : typeof body === "string" ? body : JSON.stringify(body),
    }), env, net);
    const text = await response.text();
    return { status: response.status, headers: response.headers, data: text ? JSON.parse(text) : null };
  };
  return { github, env, ask };
}

for (const pem of [PKCS1, PKCS8, PKCS1.replaceAll("\n", "\\n")]) {
  const token = await appToken("4242", pem, 1_700_000_000_000);
  const [head, claims, signature] = token.split(".");
  assert.deepEqual(JSON.parse(Buffer.from(head, "base64url")), { alg: "RS256", typ: "JWT" });
  assert.deepEqual(JSON.parse(Buffer.from(claims, "base64url")),
    { iat: 1_699_999_940, exp: 1_700_000_540, iss: "4242" });
  assert.ok(verify("sha256", Buffer.from(`${head}.${claims}`), keys.publicKey,
    Buffer.from(signature, "base64url")));
}

assert.deepEqual(described("amber", AMBER), { name: "Amber", author: "Van 'Zorn'", description: "Phosphor, in amber" });
assert.deepEqual(described("amber", AMBER.replaceAll("\n", "\r\n")).name, "Amber");
assert.throws(() => described("Amber", AMBER), { status: 400 });
assert.throws(() => described("-amber", AMBER), { status: 400 });
assert.throws(() => described("a".repeat(24), AMBER), { status: 400 });
assert.throws(() => described("amber", AMBER.replace(/^author.*\n/m, "")), { status: 400 });
assert.throws(() => described("amber", `[colors]\n${AMBER}`), { status: 400 });
assert.throws(() => described("amber", AMBER.replace("Phosphor, in amber", "x".repeat(121))), { status: 400 });
assert.throws(() => described("amber", AMBER + "#".repeat(600_000)), { status: 413 });

{
  const { ask } = world();
  assert.deepEqual((await ask("GET", "/likes")).data, { likes: {} });
  const first = await ask("POST", "/likes/mosaic", { voter: VOTERS[0] });
  assert.equal(first.status, 200);
  assert.equal(first.headers.get("Access-Control-Allow-Origin"), ORIGIN);
  assert.deepEqual(first.data, { file: "mosaic", likes: 1 });
  assert.deepEqual((await ask("POST", "/likes/mosaic", { voter: VOTERS[0] })).data, { file: "mosaic", likes: 1 });
  assert.deepEqual((await ask("POST", "/likes/mosaic", { voter: VOTERS[1] })).data, { file: "mosaic", likes: 2 });
  assert.deepEqual((await ask("POST", "/likes/dark", { voter: VOTERS[1] })).data, { file: "dark", likes: 1 });
  assert.deepEqual((await ask("GET", "/likes")).data, { likes: { mosaic: 2, dark: 1 } });
  assert.deepEqual((await ask("DELETE", "/likes/mosaic", { voter: VOTERS[0] })).data, { file: "mosaic", likes: 1 });
  assert.deepEqual((await ask("DELETE", "/likes/mosaic", { voter: VOTERS[0] })).data, { file: "mosaic", likes: 1 });
  assert.deepEqual((await ask("DELETE", "/likes/dark", { voter: VOTERS[1] })).data, { file: "dark", likes: 0 });
  assert.deepEqual((await ask("GET", "/likes")).data, { likes: { mosaic: 1 } });
  assert.equal((await ask("POST", "/likes/nothing", { voter: VOTERS[0] })).status, 404);
  assert.equal((await ask("POST", "/likes/mosaic", { voter: "me" })).status, 400);
  assert.equal((await ask("POST", "/likes/mosaic", "{")).status, 400);
  assert.equal((await ask("POST", "/likes/mosaic", { voter: VOTERS[0] }, { Origin: "https://elsewhere.example" })).status, 403);
  assert.equal((await ask("GET", "/likes", undefined, { Origin: "https://elsewhere.example" })).headers
    .get("Access-Control-Allow-Origin"), null);
  assert.equal((await ask("GET", "/")).status, 404);
  const allowed = await ask("OPTIONS", "/likes/mosaic");
  assert.equal(allowed.status, 204);
  assert.equal(allowed.headers.get("Access-Control-Allow-Methods"), "GET, POST, DELETE");
  assert.equal((await ask("OPTIONS", "/likes/mosaic", undefined, { Origin: "https://elsewhere.example" })).status, 403);
}

{
  const { ask } = world();
  const voter = (index) => `33333333-3333-4333-8333-${String(index).padStart(12, "0")}`;
  for (let index = 0; index < 5; index += 1) {
    assert.equal((await ask("POST", "/likes/mosaic", { voter: voter(index) })).status, 200);
  }
  assert.equal((await ask("POST", "/likes/mosaic", { voter: voter(5) })).status, 429);
  assert.equal((await ask("POST", "/likes/dark", { voter: voter(5) })).status, 200);
  assert.equal((await ask("POST", "/likes/mosaic", { voter: voter(5) }, { "CF-Connecting-IP": "203.0.113.10" })).status, 200);
  assert.deepEqual((await ask("GET", "/likes")).data, { likes: { mosaic: 6, dark: 1 } });
}

{
  const { ask, github } = world();
  const sent = await ask("POST", "/themes", { file: "amber", toml: AMBER, token: "person" });
  assert.equal(sent.status, 201);
  assert.deepEqual(sent.data, { number: 1, url: `https://github.com/${REPOSITORY}/pull/1` });
  assert.equal(github.saved.path, "themes/amber.toml");
  assert.equal(github.saved.text, AMBER);
  assert.equal(github.saved.message, "feat: add the amber theme");
  assert.match(github.saved.branch, /^gallery\/amber\/[0-9a-f]{8}$/);
  assert.ok(github.refs.has(`refs/heads/${github.saved.branch}`));
  const [pull] = github.pulls;
  assert.equal(pull.title, "feat: add the amber theme to the gallery");
  assert.equal(pull.base, "main");
  assert.equal(pull.head.ref, github.saved.branch);
  assert.ok(pull.body.includes("- Made by: `Van 'Zorn'`"));
  assert.ok(pull.body.includes("- File: `themes/amber.toml`"));
  const [head, claims, signature] = github.signed.split(".");
  assert.equal(JSON.parse(Buffer.from(claims, "base64url")).iss, "4242");
  assert.ok(verify("sha256", Buffer.from(`${head}.${claims}`), keys.publicKey,
    Buffer.from(signature, "base64url")));
  assert.ok(github.calls.includes("POST /app/installations/77/access_tokens"));

  const again = await ask("POST", "/themes", { file: "amber", toml: AMBER, token: "person" });
  assert.equal(again.status, 409);
  assert.match(again.data.error, /already waiting/);
  assert.equal((await ask("POST", "/themes", { file: "mosaic", toml: AMBER, token: "person" })).status, 409);
  assert.equal((await ask("POST", "/themes", { file: "dark", toml: AMBER, token: "person" })).status, 409);
  assert.equal(github.pulls.length, 1);
}

{
  const { ask, github, env } = world();
  assert.equal((await ask("POST", "/themes", { file: "amber", toml: AMBER, token: "robot" })).status, 403);
  assert.equal((await ask("POST", "/themes", { file: "amber", toml: AMBER })).status, 403);
  assert.equal((await ask("POST", "/themes", { file: "amber", toml: "name = 1", token: "person" })).status, 400);
  assert.equal((await ask("POST", "/themes", { file: "amber", toml: AMBER, token: "person" },
    { Origin: "https://elsewhere.example" })).status, 403);
  assert.equal((await ask("POST", "/themes", { file: "amber", toml: `${AMBER}# ${"x".repeat(700_000)}`, token: "person" })).status, 413);
  assert.equal(github.calls.length, 0);

  for (const broken of ["GET /repos/someone/player/pulls", "PUT /repos/someone/player/contents/",
    "POST /repos/someone/player/pulls"]) {
    github.failing = broken;
    assert.equal((await ask("POST", "/themes", { file: "amber", toml: AMBER, token: "person" })).status, 502);
    assert.equal(github.refs.size, 0, broken);
    assert.equal(github.pulls.length, 0, broken);
  }
  assert.equal(github.calls.filter((call) => call.startsWith("DELETE /repos/someone/player/git/refs/heads/gallery/amber/")).length, 2);
  github.failing = null;

  for (const file of ["one", "two", "three"]) {
    assert.equal((await ask("POST", "/themes", { file, toml: AMBER, token: "person" })).status, 201);
  }
  assert.equal((await ask("POST", "/themes", { file: "four", toml: AMBER, token: "person" })).status, 429);
  assert.equal((await ask("POST", "/themes", { file: "four", toml: AMBER, token: "person" },
    { "CF-Connecting-IP": "203.0.113.10" })).status, 201);

  for (let number = 0; number < 16; number += 1) {
    github.pulls.push({ number: 100 + number, head: { ref: `gallery/filler-${number}/00000000` }, open: true });
  }
  github.pulls.push({ number: 200, head: { ref: "fix/something" }, open: true });
  const full = await ask("POST", "/themes", { file: "five", toml: AMBER, token: "person" },
    { "CF-Connecting-IP": "203.0.113.11" });
  assert.equal(full.status, 429);
  assert.match(full.data.error, /waiting to be looked over/);

  env.SENT_A_DAY = "5";
  github.pulls.length = 0;
  assert.equal((await ask("POST", "/themes", { file: "seven", toml: AMBER, token: "person" })).status, 201);
  assert.equal((await ask("POST", "/themes", { file: "eight", toml: AMBER, token: "person" })).status, 201);
  assert.equal((await ask("POST", "/themes", { file: "nine", toml: AMBER, token: "person" })).status, 429);

  delete env.TURNSTILE_SECRET;
  assert.equal((await ask("POST", "/themes", { file: "six", toml: AMBER, token: "person" },
    { "CF-Connecting-IP": "203.0.113.12" })).status, 503);
}

console.log("gallery service: ok");
