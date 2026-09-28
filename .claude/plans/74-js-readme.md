# 74 — JS README: `describe` returns a string; list the missing methods and types

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** `bindings/js/README.md` only (shipped on npm)
**Depends on** 32 (exports `LOG_LEVEL_*`/`DATA_TYPE_*`/`DatabaseOptions` from the package root) and 33 (adds `bigint` to `QueryParam`/`GroupColumns`); land after both so the list matches the code · **Overlaps with** 18 (adds `readVectorGroupById`/`readSetGroupById` — add them to the README if 18 has landed)

## Why

`bindings/js/README.md` disagrees with the code.

- Introspection (~L163): ``- `describe()` -- Print schema info to stdout``.
  `bindings/js/src/introspection.ts` has `Database.prototype.describe = function (this: Database):
  string`, which returns the report and prints nothing. `describeCollection(collection)` and
  `summarizeCollection(collection)` (same file, ~L36-54) are missing from the list.
- Types (~L179-183):
  ```
  - `ScalarValue` -- `number | bigint | string | null`
  - `ArrayValue` -- `number[] | bigint[] | string[]`
  ...
  - `QueryParam` -- `number | string | null`
  ```
  `bindings/js/src/types.ts` has `ScalarValue = number | bigint | boolean | string | null`,
  `ArrayValue = number[] | bigint[] | boolean[] | string[]` and
  `QueryParam = number | boolean | string | null`. After plan 33 it also includes `bigint`. README
  ~L136 even says that parameters accept `boolean`, which contradicts its own ~L183.
  `GroupColumns`, the write type of the group writers, is not listed.

## Changes — `bindings/js/README.md`

1. Introspection list:
   ```markdown
   - `describe()` -- Whole-database text report (returns a string)
   - `describeCollection(collection)` -- One collection's structure (returns a string)
   - `summarizeCollection(collection)` -- Per-attribute null/value summary (returns a string)
   - `isHealthy()` -- Check database health
   - `path()` -- Get database file path
   - `currentVersion()` -- Get current schema version
   ```
2. Types list: set each line to what `src/types.ts` / `src/group-columns.ts` declare **at the time
   you run this plan**. Read them first, because plans 32 and 33 changed them. At HEAD plus plan 33:
   ```markdown
   - `ScalarValue` -- `number | bigint | boolean | string | null` (a `boolean` is stored as INTEGER 1/0)
   - `ArrayValue` -- `number[] | bigint[] | boolean[] | string[]`
   - `Value` -- `ScalarValue | ArrayValue`
   - `ElementData` -- `Record<string, Value | undefined>`
   - `QueryParam` -- `number | bigint | boolean | string | null`
   - `GroupColumns` -- `Record<string, (number | bigint | string | boolean | null)[]>`, the column-oriented
     payload of `updateTimeSeriesGroup` / `updateVectorGroup` / `updateSetGroup` (and their `ByLabel` forms)
   - `DatabaseOptions` -- `{ readOnly?: boolean; consoleLevel?: number }` (a `LOG_LEVEL_*` constant)
   ```
   Keep the existing `QuiverError`, `ScalarMetadata`, `GroupMetadata`, `TimeSeriesData` and
   `CsvOptions` lines.
3. Add a short "Constants" bullet if the README has no section for them:
   ``- `LOG_LEVEL_DEBUG | INFO | WARN | ERROR | OFF`, `DATA_TYPE_INTEGER | FLOAT | STRING | DATE_TIME | NULL` ``.
4. If plan 18 has landed, add `readVectorGroupById(collection, group, id)` and
   `readSetGroupById(collection, group, id)` to the read list, noting they return rows and keep NULL
   cells as `null`.

## Tests

None (docs).

## Docs and changelog

No CHANGELOG entry.

## Verification

- `grep -n "stdout" bindings/js/README.md` prints nothing.
- Cross-check each listed type against `bindings/js/src/types.ts` and `src/group-columns.ts`.

## Acceptance criteria

- [ ] `describe` is documented as returning a string, and the other two report methods are listed.
- [ ] The type lines match the source exactly.

## Pitfalls

- None beyond keeping the lists in sync with plans 32/33/18.

## Out of scope

- Rewriting the README's structure.
