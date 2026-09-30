# 32 — JS: `mod.ts` re-exports `src/index.ts`; export the `DATA_TYPE_*` constants

**Batch** 4 · **Severity** medium · **Breaking** no (additive: new exports) · **Size** S · **Layers** JS binding only (+ CHANGELOG, bindings/js/AGENTS.md check)
**Depends on** none · **Overlaps with** 18 (adds `readVectorGroupById`/`readSetGroupById` and possibly a `GroupData` type to `src/index.ts`; with `export *` in `mod.ts` those reach the package root automatically), 33 (widens `QueryParam`/`GroupColumns`, no export change), 74 (JS README lists exported types — keep in sync if 74 lands later)

## Why

The npm package publishes exactly one entry point. `bindings/js/package.json`:

```json
  "exports": {
    ".": "./mod.ts"
  },
  "types": "./mod.ts",
```

`bindings/js/mod.ts` keeps its **own** hand-written re-export list:

```ts
export type {
  ArrayValue,
  CsvOptions,
  ElementData,
  GroupColumns,
  GroupMetadata,
  QueryParam,
  ScalarMetadata,
  ScalarValue,
  TimeSeriesData,
  Value,
} from "./src/index.ts";
export { Database, LUA_DB_API_REFERENCE, LuaRunner, QuiverError } from "./src/index.ts";
```

`bindings/js/src/index.ts` exports more than that — `type DatabaseOptions` and the five
`LOG_LEVEL_*` constants:

```ts
export type {
  ArrayValue,
  DatabaseOptions,
  ElementData,
  QueryParam,
  ScalarValue,
  Value,
} from "./types.ts";
export {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_INFO,
  LOG_LEVEL_OFF,
  LOG_LEVEL_WARN,
} from "./types.ts";
```

So a consumer of `quiverdb` **cannot import** `LOG_LEVEL_*` or `DatabaseOptions`, even though
`DatabaseOptions.consoleLevel` is documented in `src/types.ts` as "A LOG_LEVEL_* constant". The
subpath `quiverdb/src/types.ts` is blocked by the `exports` map.

Separately, `src/types.ts` defines the data-type tags

```ts
// Mirrors quiver_data_type_t in the C API
export const DATA_TYPE_INTEGER = 0;
export const DATA_TYPE_FLOAT = 1;
export const DATA_TYPE_STRING = 2;
export const DATA_TYPE_DATE_TIME = 3;
export const DATA_TYPE_NULL = 4;
```

but neither `src/index.ts` nor `mod.ts` exports them, while `ScalarMetadata.dataType` (and the
group metadata value columns) are bare numbers. A JS caller who reads
`db.getScalarMetadata(...).dataType` has no named constant to compare it against.

The other bindings expose the full enum: Python exports `DataType`/`LogLevel`
(`bindings/python/src/quiverdb/__init__.py`, `metadata.py`: `class DataType(IntEnum)` with
`NULL = 4`), Dart exports `quiver_log_level_t` (CHANGELOG 0.10.x "Dart: `quiver_log_level_t` is
exported") and `quiver_data_type_t`, Julia re-exports `QUIVER_DATA_TYPE_*` in `Quiver.jl`.

The two lists drift because they are two lists: `git log -p bindings/js/mod.ts bindings/js/src/index.ts`
shows them edited separately. All 21 test files import `../src/index.ts`, never `../mod.ts`, so CI
cannot see the gap.

Principles: Homogeneity (uniform surface across bindings), and "one list, not two" (simplicity /
delete duplication).

## Constraints and decisions

- **Maintainer decision (binding):** export `DATA_TYPE_INTEGER/FLOAT/STRING/DATE_TIME` **and**
  `DATA_TYPE_NULL`, for parity with the full enum Dart and Python expose.
- Keep `mod.ts` as the entry point: it carries the `@module` JSDoc used by package docs, and
  `bindings/js/AGENTS.md` already documents it as "Package entry point (re-exports src/index.ts)"
  (line ~11). Repointing `package.json` `exports`/`types`/`files` at `src/index.ts` and deleting
  `mod.ts` was considered and rejected: more churn (three manifest fields) for the same result.
- JS keeps its string-based datetime surface (root Design Decision) — irrelevant here, but do not
  export any datetime helpers.
- `lua-api.ts`'s `LUA_DB_API_REFERENCE` export is unchanged.
- Do not export internal helpers. `export *` from `src/index.ts` only re-exports what `index.ts`
  itself exports; `types.ts`'s `Allocation` stays internal because `index.ts` does not export it.

## Changes

### 1. `bindings/js/mod.ts` — replace both export statements with one star re-export

Current (whole file after the doc comment):

```ts
export type {
  ArrayValue,
  CsvOptions,
  ElementData,
  GroupColumns,
  GroupMetadata,
  QueryParam,
  ScalarMetadata,
  ScalarValue,
  TimeSeriesData,
  Value,
} from "./src/index.ts";
export { Database, LUA_DB_API_REFERENCE, LuaRunner, QuiverError } from "./src/index.ts";
```

New (keep the existing `/** @module ... */` comment above it unchanged):

```ts
export * from "./src/index.ts";
```

`export *` re-exports both values and types (TypeScript/Bun handle `export type` members of the
source module through a star re-export).

### 2. `bindings/js/src/index.ts` — add the data-type constants to the value export

Current:

```ts
export {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_INFO,
  LOG_LEVEL_OFF,
  LOG_LEVEL_WARN,
} from "./types.ts";
```

New:

```ts
export {
  DATA_TYPE_DATE_TIME,
  DATA_TYPE_FLOAT,
  DATA_TYPE_INTEGER,
  DATA_TYPE_NULL,
  DATA_TYPE_STRING,
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_INFO,
  LOG_LEVEL_OFF,
  LOG_LEVEL_WARN,
} from "./types.ts";
```

(Alphabetical order matches biome's `organizeImports`/sorted-export style used in this file; run
`biome check` to confirm.)

No other source file changes. `composites.ts` and `group-columns.ts` keep importing the constants
from `./types.ts` directly.

## Tests

### New test file `bindings/js/test/package-entry.test.ts`

The first and only test that imports the published entry point:

```ts
import { describe, expect, test } from "bun:test";
import * as quiver from "../mod.ts";

describe("package entry (mod.ts)", () => {
  test("exports the public classes", () => {
    expect(typeof quiver.Database).toEqual("function");
    expect(typeof quiver.LuaRunner).toEqual("function");
    expect(typeof quiver.QuiverError).toEqual("function");
    expect(typeof quiver.LUA_DB_API_REFERENCE).toEqual("string");
  });

  test("exports the log-level constants", () => {
    expect(quiver.LOG_LEVEL_DEBUG).toEqual(0);
    expect(quiver.LOG_LEVEL_INFO).toEqual(1);
    expect(quiver.LOG_LEVEL_WARN).toEqual(2);
    expect(quiver.LOG_LEVEL_ERROR).toEqual(3);
    expect(quiver.LOG_LEVEL_OFF).toEqual(4);
  });

  test("exports the data-type constants matching quiver_data_type_t", () => {
    expect(quiver.DATA_TYPE_INTEGER).toEqual(0);
    expect(quiver.DATA_TYPE_FLOAT).toEqual(1);
    expect(quiver.DATA_TYPE_STRING).toEqual(2);
    expect(quiver.DATA_TYPE_DATE_TIME).toEqual(3);
    expect(quiver.DATA_TYPE_NULL).toEqual(4);
  });

  test("a DatabaseOptions value built from exported constants opens a database", () => {
    const options: quiver.DatabaseOptions = { consoleLevel: quiver.LOG_LEVEL_OFF };
    const schema = `${import.meta.dir}/../../../tests/schemas/valid/basic.sql`;
    const db = quiver.Database.fromSchema(":memory:", schema, options);
    try {
      expect(db.getScalarMetadata("Configuration", "integer_attribute").dataType).toEqual(
        quiver.DATA_TYPE_INTEGER,
      );
    } finally {
      db.close();
    }
  });
});
```

Before the change, the log-level and data-type tests fail (`undefined !== 0`), and
`quiver.DatabaseOptions` does not type-check (Bun does not type-check at runtime, so that line
alone would still run — the runtime assertions are what fail).

Verify the fixture names before writing: `tests/schemas/valid/basic.sql` must contain
`Configuration.integer_attribute`. It does at HEAD (`basic.sql:8`:
`integer_attribute INTEGER DEFAULT 6,`). `ScalarMetadata.dataType: number` is at
`src/metadata.ts:15`, and `Database.fromSchema(dbPath, schemaPath, options?: DatabaseOptions)` is at
`src/database.ts:19`. Re-check all three with grep if earlier plans have landed.

No other test changes. Existing tests keep importing `../src/index.ts`.

## Docs and changelog

- `bindings/js/AGENTS.md` line ~11 already says `mod.ts  # Package entry point (re-exports src/index.ts)`,
  which this change makes literally true. Line ~18
  (`src/types.ts  # Central DATA_TYPE_* / LOG_LEVEL_* constants and DatabaseOptions type`) is also
  accurate. Append " — all re-exported from the package root" to line 18.
- `bindings/js/README.md`: if it has an exports/types section, mention the constants (plan 74
  rewrites the Types list; if 74 has already landed, add one line: "`DATA_TYPE_*` and
  `LOG_LEVEL_*` constants").
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Added` (create the `### Added` heading
  under 0.12.0 if it does not exist yet; keep the section order Changed / Added / Fixed / Removed
  used in the file):

  ```markdown
  - **JS: `LOG_LEVEL_*`, `DATA_TYPE_*` and the `DatabaseOptions` type are exported from
    `quiverdb`.** `mod.ts` now re-exports `src/index.ts` whole, so the package root can no longer
    drift from it. The `consoleLevel` values `DatabaseOptions` documents were previously not
    reachable from outside the package, and `ScalarMetadata.dataType` had no named constants.
  ```

## Verification

From the repo root (`C:\Development\Quiver\quiver1`):

1. `cmake --build build --config Debug` (only needed if the native libs are stale; the JS tests
   load `build/bin`).
2. `bindings/js/test/test.bat test/package-entry.test.ts` — the four new tests pass.
3. `bindings/js/test/test.bat` — whole JS suite passes.
4. `cd bindings/js && bunx biome check mod.ts src/index.ts test/package-entry.test.ts` — no new
   lint errors in the touched files (pre-existing debt elsewhere is out of scope per root
   "Do Not Fix").
5. `scripts/format.bat`.

## Acceptance criteria

- [x] `mod.ts` contains the doc comment plus exactly `export * from "./src/index.ts";`.
- [x] `src/index.ts` exports all five `DATA_TYPE_*` constants.
- [x] `test/package-entry.test.ts` exists, imports `../mod.ts`, and passes.
- [x] Full JS suite green.
- [x] CHANGELOG `### Added` entry under 0.12.0 (under `## [0.12.6] — unreleased`, see Implementation notes).

## Pitfalls

- Bun type-strips without type-checking; a wrong type-only export will not fail at runtime. The
  runtime assertions in the new test are what prove the value exports.
- `export *` does not re-export a module's default export — `src/index.ts` has none, so nothing is
  lost.
- If plan 18 adds a `GroupData` type or new methods, they are exported automatically via
  `export *`; do not add them to `mod.ts` by hand.
- `DATA_TYPE_NULL` is only ever used as a query-parameter tag in `query.ts`'s marshalling; it never
  appears in metadata. It is exported for parity anyway (maintainer decision).

## Out of scope

- Converting the constants to a TS `enum` or a frozen object (would change the public shape).
- README rewrite (plan 74).
- Any change to `package.json` `exports`/`files`.

## Implementation notes

This was implemented on `rs/plan32`. At planning time the branch sat at master `5358778`. By the time implementation started it had been fast-forwarded to master `3608708`, which brought in plans 24-31 (#336-#343). So `git merge origin/master` was a no-op, and every result below refers to `3608708` plus this change. Plan 31 had touched `bindings/js/AGENTS.md` and `CHANGELOG.md`, but none of the lines this plan edits.

Before any edit, three read-only reviewers checked the plan: one on the code claims, one on overlap with other plans and the CHANGELOG, and one arguing against it. Their verdict was **implement**. They confirmed:
- `export *` is safe under Bun 1.3.14, TS `isolatedModules` and `verbatimModuleSyntax`, and `bun build --compile`.
- Nothing internal leaks: `Allocation`, the `ffi-helpers`, `check`, `numericCells` and the loader stay unexported.
- `claw`'s `LUA_DB_API_REFERENCE` import is unchanged.
- Biome's recommended set does not include `noReExportAll` / `noBarrelFile`.
- Plain `export const` constants leave room for an additive enum-like type later.

After the change, two more read-only reviewers went over the diff, one on correctness and one on docs. Both said **ship**. Their one nit is fixed below (`tests/AGENTS.md`).

### Drift fixed

- **CHANGELOG.**
  - `## [0.12.0] — unreleased` no longer exists; 0.12.0 was released 2026-09-27. `v0.12.5` is tagged at `7c8bf7a`, and the manifests are at 0.12.6.
  - When implementation started, plan 24 had already opened `## [0.12.6] — unreleased`, with `### Changed` and `### Fixed`.
  - The entry is the plan's text verbatim, under a new `### Added` placed first in that section. The file's real subsection order is Added / Changed / Removed / Fixed, not "Changed / Added / Fixed / Removed".
  - The still-undated `## [0.12.5] — unreleased` heading was left for the maintainer, who dates headings by hand (`73094f0`, `d401081`).
  - There is no manifest bump (the change is additive and 0.12.6 is already set) and no compare link (plan 78).
- **Test 4 checks every scalar type against the core.** Instead of asserting only `integer_attribute`, it asserts `integer_attribute` / `float_attribute` / `string_attribute` / `date_attribute` of `basic.sql` against `DATA_TYPE_INTEGER` / `FLOAT` / `STRING` / `DATE_TIME`. Tests 2 and 3 only restate literals copied from `types.ts`. No JS test checked a `dataType` *value* before (`database-metadata.test.ts` checks `typeof`), so this is what actually ties the constants to `quiver_data_type_t`.
- **Schema path.** It uses `join(import.meta.dir, "..", …)` from `node:path`, like the other tests, instead of a template string. Biome then wrapped it one segment per line, as in `database-metadata.test.ts`.
- **README.** It has a `## Types` section and plan 74 depends on 32, so the "if 74 has already landed" branch cannot happen. Two bullets were added after `CsvOptions` in **plan 74's wording** (74's steps 2-3):
  - the `DatabaseOptions` line verbatim;
  - the constants line, verbatim plus a short trailing description.

  That way 74 finds them and skips its step 3. The stale existing bullets (`ScalarValue` / `ArrayValue` / `QueryParam`, README:181-185) are left to plans 33 and 74.
- **`bindings/js/AGENTS.md`.** The appended text went on a `#` continuation line to keep the layout block within width.
- **`tests/AGENTS.md`.** Its list of JS test files without the `database-` prefix now includes `package-entry.test.ts` (the post-review nit).
- **Commands and paths.**
  - The repo is `C:\Development\Quiver\quiver9`, not `quiver1`.
  - Verification step 2 (`test.bat test/package-entry.test.ts`) runs the **whole** suite. `test.bat` is `bun test test %*`, bun ORs positional filename filters, and "test" matches every file. To run the one file, from `bindings/js` with `build/bin` on `PATH`: `bun test package-entry`.
  - Step 1's rebuild was required, not optional. The stale `build/bin` lacked `quiver_database_free_masks`.
- **Plan text** (no action needed):
  - There were 20 test files, not 21; `lua-api-sync.test.ts` imports `src/lua-api.ts`.
  - Plan 18 added no `GroupData` type.
  - Julia defines four `QUIVER_DATA_TYPE_*` consts (no `NULL`) and does not export them.
  - `fromSchema` is at `database.ts:18`.
  - `query.ts`, `time-series.ts` and `ffi-helpers.ts` also import the constants from `./types.ts` directly. They are unchanged.

### Results

- **Red first.** With only the new test in place, `bun test package-entry` gave `1 pass, 3 fail`:
  - `exports the log-level constants`: `Expected: 0 Received: undefined`.
  - `exports the data-type constants ...`: `Expected: 0 Received: undefined`.
  - The options/metadata test: `Expected: undefined Received: 0`. It also printed the core's INFO log lines, because `consoleLevel: undefined` fell back to `LOG_LEVEL_INFO`.
- **Green.**
  - `bun test package-entry` gives `4 pass, 0 fail, 18 expect() calls`, with no log output, since `LOG_LEVEL_OFF` now reaches the core.
  - `Object.keys` of `mod.ts` is `DATA_TYPE_{DATE_TIME,FLOAT,INTEGER,NULL,STRING}, Database, LOG_LEVEL_{DEBUG,ERROR,INFO,OFF,WARN}, LUA_DB_API_REFERENCE, LuaRunner, QuiverError`.
  - `bindings/js/test/test.bat` gives `237 pass, 0 fail`, 21 files.
- **Lint.**
  - `bunx biome lint mod.ts src/index.ts test/package-entry.test.ts` is clean.
  - `bunx biome check` on the same three files is clean too, once `format.bat` has normalized them to LF. Before that, `biome check` on the untouched files reports format errors, because the Windows checkout (`core.autocrlf=true`) leaves them CRLF.
- **`scripts/format.bat`** exited 0.
  - clang-format, JuliaFormatter, dart format and ruff left the tree unchanged.
  - Biome "fixed" 43 JS files. 40 of them were line-ending-only rewrites of untouched files; `git diff` showed no content change. I restored them with `git checkout --` on exactly those 40 (not a blanket `bindings/js` restore, since this plan edits files there).
- **`git diff --stat`.** Exactly the planned files changed: `mod.ts`, `src/index.ts`, `test/package-entry.test.ts` (new), `bindings/js/AGENTS.md`, `bindings/js/README.md`, `tests/AGENTS.md`, `CHANGELOG.md`, and this file.

### For later plans

- **33** (JS bigint writers): the README Types lines it edits (`QueryParam`, README:185) are untouched. The two new bullets sit after `CsvOptions`.
- **74** (JS README): the `DatabaseOptions` and constants bullets are already there in 74's wording, so its step 3 is satisfied. Its Types rewrite should keep both bullets.
- **Batch 4 CHANGELOG**: `## [0.12.6] — unreleased` now has `### Added` (this entry) above `### Changed` / `### Fixed`. Later plans add to it instead of opening a section.
- **Not done (optional):** an exact `Object.keys(mod.ts)` snapshot test was suggested as a guard against accidental exports. It was left out because it brings back a second hand-kept list of the public surface, which is what this plan removes. With `export *`, `src/index.ts` is the one reviewed list.
