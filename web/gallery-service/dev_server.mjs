import { generateKeyPairSync } from "node:crypto";
import { createServer } from "node:http";
import { fakeGitHub, localDatabase } from "./local.mjs";
import { handle } from "./worker.js";

const GITHUB = "https://api.github.com";
const TEST_SECRET = "1x0000000000000000000000000000000AA";
const REPOSITORY = "local/echopod";
const site = (process.argv[2] ?? "http://127.0.0.1:8770").replace(/\/$/, "");
const port = Number(process.argv[3] ?? 8787);
const here = `http://127.0.0.1:${port}`;
const sent = [];

async function taken() {
  try {
    const listed = await (await fetch(`${site}/themes/index.json`)).json();
    return listed.map((theme) => `themes/${theme.file}.toml`);
  } catch {
    return [];
  }
}

const github = fakeGitHub(REPOSITORY, await taken(), (number) => `${here}/sent/${number}`);
const env = {
  DB: localDatabase(),
  ALLOWED_ORIGINS: [site, site.replace("127.0.0.1", "localhost")].join(","),
  GALLERY_INDEX: `${site}/themes/index.json`,
  GITHUB_REPOSITORY: REPOSITORY,
  GITHUB_BRANCH: "main",
  GITHUB_APP_ID: "1",
  GITHUB_INSTALLATION_ID: "1",
  GITHUB_APP_PRIVATE_KEY: generateKeyPairSync("rsa", { modulusLength: 2048 }).privateKey
    .export({ type: "pkcs1", format: "pem" }),
  TURNSTILE_SECRET: TEST_SECRET,
  VOTER_SALT: "local",
  SENT_A_DAY: "1000",
};
const net = (input, init) => (String(input).startsWith(GITHUB) ? github.net(input, init) : fetch(input, init));

createServer(async (incoming, outgoing) => {
  const waiting = /^\/sent\/(\d+)$/.exec(incoming.url);
  if (incoming.method === "GET" && waiting) {
    const text = sent[Number(waiting[1]) - 1];
    outgoing.writeHead(text === undefined ? 404 : 200, { "Content-Type": "text/plain; charset=utf-8" });
    outgoing.end(text ?? "Nothing was sent with this number.");
    return;
  }
  const chunks = [];
  for await (const chunk of incoming) {
    chunks.push(chunk);
  }
  const headers = new Headers({ "CF-Connecting-IP": incoming.socket.remoteAddress ?? "127.0.0.1" });
  for (const [name, value] of Object.entries(incoming.headers)) {
    if (typeof value === "string" && name !== "host" && name !== "connection") {
      headers.set(name, value);
    }
  }
  const response = await handle(new Request(here + incoming.url, {
    method: incoming.method,
    headers,
    body: chunks.length > 0 ? Buffer.concat(chunks) : undefined,
  }), env, net);
  if (incoming.method === "POST" && incoming.url === "/themes" && response.status === 201) {
    sent.push(github.saved.text);
  }
  outgoing.writeHead(response.status, Object.fromEntries(response.headers));
  outgoing.end(Buffer.from(await response.arrayBuffer()));
}).listen(port, "127.0.0.1", () => {
  console.log(`the gallery service is at ${here}, for the site at ${site}; nothing leaves this computer but the person check`);
});
