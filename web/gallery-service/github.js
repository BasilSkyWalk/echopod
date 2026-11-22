const API = "https://api.github.com";
const AGENT = "echopod-gallery";
const RSA_KEY = [0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00];
const TOKEN_LIFE_SECONDS = 540;
const CLOCK_SLACK_SECONDS = 60;
const BRANCHES = "gallery/";
const encoder = new TextEncoder();

export class ServiceError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

function derLength(length) {
  if (length < 0x80) {
    return [length];
  }
  const bytes = [];
  for (let rest = length; rest > 0; rest = Math.floor(rest / 256)) {
    bytes.unshift(rest % 256);
  }
  return [0x80 | bytes.length, ...bytes];
}

function der(tag, content) {
  return [tag, ...derLength(content.length), ...content];
}

function base64Bytes(bytes) {
  let text = "";
  for (const byte of bytes) {
    text += String.fromCharCode(byte);
  }
  return btoa(text);
}

function base64Url(bytes) {
  return base64Bytes(bytes).replaceAll("+", "-").replaceAll("/", "_").replace(/=+$/, "");
}

export function privateKeyBytes(pem) {
  const text = pem.replaceAll("\\n", "\n");
  const body = text.replace(/-----[A-Z ]+-----/g, "").replace(/\s+/g, "");
  const bytes = Uint8Array.from(atob(body), (letter) => letter.charCodeAt(0));
  if (!text.includes("BEGIN RSA PRIVATE KEY")) {
    return bytes;
  }
  return Uint8Array.from(der(0x30, [0x02, 0x01, 0x00, ...RSA_KEY, ...der(0x04, Array.from(bytes))]));
}

export async function appToken(appId, pem, now = Date.now()) {
  const key = await crypto.subtle.importKey("pkcs8", privateKeyBytes(pem),
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["sign"]);
  const seconds = Math.floor(now / 1000);
  const claims = { iat: seconds - CLOCK_SLACK_SECONDS, exp: seconds + TOKEN_LIFE_SECONDS, iss: String(appId) };
  const signed = [{ alg: "RS256", typ: "JWT" }, claims]
    .map((part) => base64Url(encoder.encode(JSON.stringify(part)))).join(".");
  const signature = await crypto.subtle.sign("RSASSA-PKCS1-v1_5", key, encoder.encode(signed));
  return `${signed}.${base64Url(new Uint8Array(signature))}`;
}

function unavailable(what, status) {
  console.error(JSON.stringify({ message: "github call failed", what, status }));
  return new ServiceError(502, "The gallery couldn't take the theme just now. Try again in a minute.");
}

async function call(net, token, method, path, body) {
  const response = await net(API + path, {
    method,
    headers: {
      Accept: "application/vnd.github+json",
      Authorization: `Bearer ${token}`,
      "User-Agent": AGENT,
      "X-GitHub-Api-Version": "2022-11-28",
      ...(body ? { "Content-Type": "application/json" } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await response.text();
  let data = null;
  try {
    data = text ? JSON.parse(text) : null;
  } catch {
    data = null;
  }
  return { status: response.status, data };
}

async function installationToken(config, net) {
  const signed = await appToken(config.appId, config.privateKey);
  const made = await call(net, signed, "POST", `/app/installations/${config.installationId}/access_tokens`);
  if (made.status !== 201 || typeof made.data?.token !== "string") {
    throw unavailable("installation token", made.status);
  }
  return made.data.token;
}

async function exists(net, token, config, path) {
  const found = await call(net, token, "GET",
    `/repos/${config.repository}/contents/${path}?ref=${encodeURIComponent(config.branch)}`);
  if (found.status !== 200 && found.status !== 404) {
    throw unavailable("contents " + path, found.status);
  }
  return found.status === 200;
}

function randomTag() {
  return Array.from(crypto.getRandomValues(new Uint8Array(4)), (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export async function openSubmission(config, net, submission) {
  const { file, toml, title, body, message } = submission;
  const repo = `/repos/${config.repository}`;
  const token = await installationToken(config, net);
  for (const folder of ["themes", "assets/themes"]) {
    if (await exists(net, token, config, `${folder}/${file}.toml`)) {
      throw new ServiceError(409, "The gallery already has a theme with this name. Give yours another name and send it again.");
    }
  }
  const open = await call(net, token, "GET", `${repo}/pulls?state=open&per_page=100`);
  if (open.status !== 200 || !Array.isArray(open.data)) {
    throw unavailable("open pulls", open.status);
  }
  const waiting = open.data.map((pull) => pull.head?.ref ?? "").filter((ref) => ref.startsWith(BRANCHES));
  if (waiting.some((ref) => ref.startsWith(`${BRANCHES}${file}/`))) {
    throw new ServiceError(409, "A theme with this name is already waiting to be looked over. Give yours another name, or wait for that one.");
  }
  if (waiting.length >= config.queueLimit) {
    throw new ServiceError(429, "A lot of themes are waiting to be looked over. Try again in a day or two.");
  }
  const base = await call(net, token, "GET", `${repo}/git/ref/heads/${encodeURIComponent(config.branch)}`);
  if (base.status !== 200 || typeof base.data?.object?.sha !== "string") {
    throw unavailable("base ref", base.status);
  }
  const branch = `${BRANCHES}${file}/${randomTag()}`;
  const made = await call(net, token, "POST", `${repo}/git/refs`,
    { ref: `refs/heads/${branch}`, sha: base.data.object.sha });
  if (made.status !== 201) {
    throw unavailable("create ref", made.status);
  }
  try {
    const saved = await call(net, token, "PUT", `${repo}/contents/themes/${file}.toml`,
      { message, content: base64Bytes(encoder.encode(toml)), branch });
    if (saved.status !== 201) {
      throw unavailable("save file", saved.status);
    }
    const pull = await call(net, token, "POST", `${repo}/pulls`,
      { title, body, head: branch, base: config.branch });
    if (pull.status !== 201 || typeof pull.data?.html_url !== "string") {
      throw unavailable("open pull", pull.status);
    }
    return { number: pull.data.number, url: pull.data.html_url };
  } catch (error) {
    await call(net, token, "DELETE", `${repo}/git/refs/heads/${branch}`);
    throw error;
  }
}
