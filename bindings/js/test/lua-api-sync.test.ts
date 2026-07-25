import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { readFileSync } from "node:fs";
import { join } from "node:path";
// Import the constant directly, NOT via src/index.ts — this test must not drag in the FFI loader.
import { LUA_DB_API_REFERENCE } from "../src/lua-api.ts";

const CPP_PATH = join(__dirname, "..", "..", "..", "src", "lua_runner.cpp");
const CPP = readFileSync(CPP_PATH, "utf8");

// Markup- and wrap-insensitive view, so re-wrapping a paragraph can't fail the stdlib assert.
const DOC_FLAT = LUA_DB_API_REFERENCE.replaceAll("`", "").replace(/\s+/g, " ");

// Pass 1: `bind.set_function("x", ...)` -> db:x ; `ns.set_function("x", ...)` -> quiver.x
// (`ns` is `sol::table ns = lua["quiver"]` in bind_binary/bind_expression.) Multiline so the
// clang-format-wrapped call (`bind.set_function(\n    "open_file",`) is still caught.
const setFns = [...CPP.matchAll(/\b(bind|ns)\.set_function\(\s*"([a-z_][a-z0-9_]*)"/g)];
const dbMethods = new Set(setFns.filter((m) => m[1] === "bind").map((m) => m[2]));
const quiverFns = new Set(setFns.filter((m) => m[1] === "ns").map((m) => m[2]));

// Pass 2: `new_usertype<T>("T", "name", lambda, ...)` variadic pairs. Position parsing is
// impossible here — sol::no_constructor and the sol::meta_function::* entries break parity, and
// lambda bodies contain string literals (resolve_sandboxed_path(self, "export_csv", path)) that
// would be captured as method names. Key off the line shape clang-format guarantees instead: the
// bound name alone on its own line as a bare lowercase string. Leading-lowercase drops the
// usertype names themselves ("Database", "BinaryFile", ...).
const usertypeMethods = new Map<string, Set<string>>();
let current = "";
for (const line of CPP.split("\n")) {
  const open = /new_usertype<(\w+)>/.exec(line);
  if (open) {
    current = open[1];
    usertypeMethods.set(current, new Set());
    continue;
  }
  // Load-bearing: a wrapped `set_function(` name must not be attributed to the open usertype.
  if (line.includes(".set_function(")) {
    current = "";
    continue;
  }
  const pair = /^\s*"([a-z_][a-z0-9_]*)",\s*$/.exec(line);
  if (pair && current) usertypeMethods.get(current)?.add(pair[1]);
}
for (const name of usertypeMethods.get("Database") ?? []) dbMethods.add(name);

// Word-boundary suffix: `db:describe` must not be satisfied by `db:describe_collection`, and
// `quiver.gt` / `quiver.metadata` must not be satisfied by `gte` / `metadata_from_toml`.
const documented = (token: string) =>
  new RegExp(`${token}(?![a-z0-9_])`).test(LUA_DB_API_REFERENCE);

describe("lua-api reference stays in sync with src/lua_runner.cpp", () => {
  test("parse found the binding surface", () => {
    // Meta-guard: if a reformat of lua_runner.cpp breaks the regexes above, fail loudly instead of
    // passing vacuously forever on an empty match set.
    expect(dbMethods.size).toBeGreaterThan(40);
    expect(quiverFns.size).toBeGreaterThan(10);
    expect(usertypeMethods.get("Expression")?.size).toBeGreaterThan(0);
  });

  test("every db: method appears as the literal token db:<name>", () => {
    expect([...dbMethods].filter((n) => !documented(`db:${n}`)).sort()).toEqual([]);
  });

  test("every quiver.* free function appears as the literal token quiver.<name>", () => {
    expect([...quiverFns].filter((n) => !documented(`quiver\\.${n}`)).sort()).toEqual([]);
  });

  test("every BinaryFile/BinaryMetadata/Expression method appears as :<name>(", () => {
    // ponytail: receiver-agnostic. `:name(` proves the method is shown somewhere, not that it is
    // shown on the right type — the doc uses ad-hoc receiver names (f/r/md/e), so no fixed token
    // is available. Coverage, not signature checking.
    const missing: string[] = [];
    for (const type of ["BinaryFile", "BinaryMetadata", "Expression"]) {
      for (const name of usertypeMethods.get(type) ?? []) {
        if (!LUA_DB_API_REFERENCE.includes(`:${name}(`)) missing.push(`${type}:${name}`);
      }
    }
    expect(missing.sort()).toEqual([]);
  });

  test("no documented db:/quiver. name has been removed from the binding", () => {
    const stale = [
      ...[...LUA_DB_API_REFERENCE.matchAll(/\bdb:([a-z_][a-z0-9_]*)/g)]
        .filter((m) => !dbMethods.has(m[1]))
        .map((m) => `db:${m[1]}`),
      ...[...LUA_DB_API_REFERENCE.matchAll(/\bquiver\.([a-z_][a-z0-9_]*)/g)]
        .filter((m) => !quiverFns.has(m[1]))
        .map((m) => `quiver.${m[1]}`),
    ];
    expect([...new Set(stale)].sort()).toEqual([]);
  });

  test("documents the exact open_libraries stdlib list", () => {
    const call = /open_libraries\(([\s\S]*?)\);/.exec(CPP)?.[1] ?? "";
    const libs = [...call.matchAll(/sol::lib::(\w+)/g)].map((m) => m[1]);
    expect(libs.length).toBeGreaterThan(0);
    // The doc must carry the list verbatim in one canonical sentence. A fuzzy "does it mention
    // math" check passes on text that mentions math while denying it exists — which is exactly how
    // this drifted (the doc claimed base/string/table only, and "there is NO math", for releases).
    // Compare the extracted list, not the whole doc: containment failures print all 23 KB.
    const documentedLibs = /Loaded standard libraries: ([^.]*)\./.exec(DOC_FLAT)?.[1];
    expect(documentedLibs).toBe(libs.join(", "));
  });
});
