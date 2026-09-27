# 32 — JS: `mod.ts` re-exports `src/index.ts`; export the `DATA_TYPE_*` constants

**Batch** 4 · **Severity** medium · **Breaking** no (additive: new exports) · **Size** S · **Layers** JS binding only (+ CHANGELOG, bindings/js/CLAUDE.md check)
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
  `bindings/js/CLAUDE.md` already documents it as "Package entry point (re-exports src/index.ts)"
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

- `bindings/js/CLAUDE.md` line ~11 already says `mod.ts  # Package entry point (re-exports src/index.ts)`,
  which this change makes literally true. Line ~18
  (`src/types.ts  # Central DATA_TYPE_* / LOG_LEVEL_* constants and DatabaseOptions type`) is also
  accurate. Append " — all re-exported from the package root" to line 18.
- `bindings/js/README.md`: if it has an exports/types section, mention the constants (plan 74
  rewrites the Types list; if 74 has already landed, add one line: "`DATA_TYPE_*` and
  `LOG_LEVEL_*` constants").
- `CHANGELOG.md`, under `## [0.11.0] — unreleased` → `### Added` (create the `### Added` heading
  under 0.11.0 if it does not exist yet; keep the section order Changed / Added / Fixed / Removed
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

- [ ] `mod.ts` contains the doc comment plus exactly `export * from "./src/index.ts";`.
- [ ] `src/index.ts` exports all five `DATA_TYPE_*` constants.
- [ ] `test/package-entry.test.ts` exists, imports `../mod.ts`, and passes.
- [ ] Full JS suite green.
- [ ] CHANGELOG `### Added` entry under 0.11.0.

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
