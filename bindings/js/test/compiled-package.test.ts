import { expect, test } from "bun:test";
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { delimiter, join, resolve } from "node:path";

test("compiled Database and Sandbox load native siblings from an unrelated cwd", () => {
  const bindingDir = resolve(import.meta.dir, "..");
  const repoDir = resolve(bindingDir, "../..");
  const os = { win32: "windows", linux: "linux", darwin: "macos" }[process.platform];
  const arch = process.arch === "arm64" ? "aarch64" : "x86_64";
  const extension =
    process.platform === "win32" ? "dll" : process.platform === "darwin" ? "dylib" : "so";
  const library = `libquiver_c.${extension}`;
  const nativeDir = [
    join(bindingDir, "libs", `${os}-${arch}`),
    join(repoDir, "build", "lib"),
    join(repoDir, "build", "bin"),
    join(repoDir, "build", "bin", "Debug"),
    join(repoDir, "build", "bin", "Release"),
  ].find((dir) => existsSync(join(dir, library)));
  expect(nativeDir).toBeDefined();

  const root = mkdtempSync(join(tmpdir(), "quiver-compiled-"));
  try {
    const bin = join(root, "bin");
    const cwd = join(root, "empty-cwd");
    mkdirSync(bin);
    mkdirSync(cwd);
    for (const file of readdirSync(nativeDir!)) {
      if (/^libquiver.*\.(dll|dylib|so)(\.\d+)*$/.test(file)) {
        copyFileSync(join(nativeDir!, file), join(bin, file));
      }
    }
    const schema = join(root, "schema.sql");
    writeFileSync(
      schema,
      "CREATE TABLE Configuration(id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL) STRICT; CREATE TABLE Items(id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL, value INTEGER) STRICT;",
    );
    const entry = join(root, "probe.ts");
    writeFileSync(
      entry,
      `
      import assert from "node:assert/strict";
      import { Database, LOG_LEVEL_OFF, Sandbox } from ${JSON.stringify(join(bindingDir, "mod.ts"))};
      const options = { consoleLevel: LOG_LEVEL_OFF };
      const db = Database.fromSchema(process.argv[2], process.argv[3], options);
      try {
        const id = db.createElement("Items", { label: "probe", value: 1 });
        const sandbox = new Sandbox(db);
        try { sandbox.run('db:update_element("Items", 1, {value=42})'); }
        finally { sandbox.close(); }
        assert.equal(db.readScalarIntegerById("Items", "value", id), 42);
      } finally { db.close(); }
      const reader = Database.open(process.argv[2], { ...options, readOnly: true });
      try { assert.equal(reader.readScalarIntegerById("Items", "value", 1), 42); }
      finally { reader.close(); }
      console.log("native mutation and independent readback: 42");
    `,
    );
    const env = { ...process.env };
    for (const key of Object.keys(env)) {
      if (/^(LD_LIBRARY_PATH|LD_PRELOAD|DYLD_)/.test(key)) delete env[key];
    }
    // CI's native PATH entries must not mask a missing executable-directory lookup.
    env.PATH = (env.PATH ?? "")
      .split(delimiter)
      .filter((dir) => dir && !existsSync(join(dir, library)))
      .join(delimiter);
    const executable = join(bin, process.platform === "win32" ? "probe.exe" : "probe");
    const build = Bun.spawnSync(
      [process.execPath, "build", "--compile", entry, "--outfile", executable],
      { cwd: root, env },
    );
    expect(new TextDecoder().decode(build.stderr)).toBe("");
    expect(build.exitCode).toBe(0);
    const run = Bun.spawnSync([executable, join(root, "probe.db"), schema], { cwd, env });
    expect(new TextDecoder().decode(run.stderr)).toBe("");
    expect(run.exitCode).toBe(0);
    expect(new TextDecoder().decode(run.stdout)).toContain(
      "native mutation and independent readback: 42",
    );
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
}, 60_000);
