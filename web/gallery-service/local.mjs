import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";

const TABLES = new URL("./migrations/0001_likes_and_submissions.sql", import.meta.url);
const API = "https://api.github.com";

export function localDatabase() {
  const database = new DatabaseSync(":memory:");
  database.exec(readFileSync(TABLES, "utf-8"));
  const numbered = (values) => (values.length === 0 ? []
    : [Object.fromEntries(values.map((value, index) => [`?${index + 1}`, value]))]);
  const statement = (sql, values = []) => ({
    bind: (...next) => statement(sql, next),
    first: async () => database.prepare(sql).get(...numbered(values)) ?? null,
    all: async () => ({ results: database.prepare(sql).all(...numbered(values)) }),
    run: async () => {
      database.prepare(sql).run(...numbered(values));
      return { success: true };
    },
  });
  return {
    prepare: (sql) => statement(sql),
    batch: async (statements) => {
      database.exec("BEGIN");
      try {
        const done = [];
        for (const each of statements) {
          done.push(await each.all());
        }
        database.exec("COMMIT");
        return done;
      } catch (error) {
        database.exec("ROLLBACK");
        throw error;
      }
    },
  };
}

function answer(status, value) {
  return new Response(value === null ? null : JSON.stringify(value), { status });
}

export function fakeGitHub(repository, taken = [], pullAddress = (number) => `https://github.com/${repository}/pull/${number}`) {
  const state = { files: new Map(taken.map((path) => [path, ""])), refs: new Map(), pulls: [], calls: [], failing: null };
  const repo = `${API}/repos/${repository}`;
  state.net = async (input, init = {}) => {
    const url = String(input);
    const method = init.method ?? "GET";
    const body = init.body ? JSON.parse(init.body) : null;
    const asked = `${method} ${url.slice(API.length)}`;
    state.calls.push(asked);
    if (state.failing && asked.startsWith(state.failing)) {
      return answer(500, { message: "broken" });
    }
    if (method === "POST" && /\/app\/installations\/\d+\/access_tokens$/.test(url)) {
      state.signed = init.headers.Authorization.slice("Bearer ".length);
      return answer(201, { token: "installation-token" });
    }
    if (method === "GET" && url.startsWith(`${repo}/contents/`)) {
      const path = decodeURIComponent(url.slice(`${repo}/contents/`.length).split("?")[0]);
      return state.files.has(path) ? answer(200, { path }) : answer(404, { message: "Not Found" });
    }
    if (method === "GET" && url.startsWith(`${repo}/pulls?`)) {
      return answer(200, state.pulls.filter((pull) => pull.open));
    }
    if (method === "GET" && url.startsWith(`${repo}/git/ref/heads/`)) {
      return answer(200, { object: { sha: "0123456789abcdef0123456789abcdef01234567" } });
    }
    if (method === "POST" && url === `${repo}/git/refs`) {
      state.refs.set(body.ref, body.sha);
      return answer(201, { ref: body.ref });
    }
    if (method === "DELETE" && url.startsWith(`${repo}/git/refs/`)) {
      state.refs.delete(`refs/${url.slice(`${repo}/git/refs/`.length)}`);
      return answer(204, null);
    }
    if (method === "PUT" && url.startsWith(`${repo}/contents/`)) {
      const path = url.slice(`${repo}/contents/`.length);
      state.saved = { path, branch: body.branch, message: body.message,
        text: Buffer.from(body.content, "base64").toString("utf-8") };
      return answer(201, { content: { path } });
    }
    if (method === "POST" && url === `${repo}/pulls`) {
      const number = state.pulls.length + 1;
      const pull = { number, html_url: pullAddress(number), head: { ref: body.head }, base: body.base,
        title: body.title, body: body.body, open: true };
      state.pulls.push(pull);
      return answer(201, pull);
    }
    return answer(404, { message: "Not Found" });
  };
  return state;
}
