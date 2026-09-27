# 33 — JS: accept `bigint` in the group writers and query parameters

**Batch** 4 · **Severity** low · **Breaking** no (additive: inputs that threw now work) · **Size** S · **Layers** JS binding only (+ bindings/js/CLAUDE.md, README types, CHANGELOG)
**Depends on** 31 (introduces `numericCells` in `src/group-columns.ts`; this plan extends it) · **Overlaps with** 22 (changes the query C entry points and `marshalParams`' call sites in `src/query.ts`; this plan only touches `marshalParams`' body), 74 (JS README type list)

## Why

JS already treats `bigint` as the exact-int64 input type on some write paths. `bindings/js/CLAUDE.md`,
"int64 handling": *"input params accept `number | bigint` — `allocNativeInt64` writes each element
with `DataView.setBigInt64`, so `bigint` inputs (scalar or array) are preserved exactly, never
coerced through `Number`."* `createElement` honours that (`src/create.ts`: `if (typeof first ===
"bigint")` for arrays, `if (typeof value === "bigint")` for scalars), and so does
`upsertTimeSeriesRow` (`src/time-series.ts` `upsertRowColumns`:
`} else if (typeof value === "bigint" || Number.isInteger(value)) {`). A test pins the exactness:
`test/database-create.test.ts` "bigint array stores values beyond Number.MAX_SAFE_INTEGER without
precision loss" (2^53+1 round-trips via `CAST(count_value AS TEXT)`).

Two write paths reject `bigint`:

1. **The group writers** (`updateVectorGroup`, `updateSetGroup`, `updateTimeSeriesGroup` and their
   `ByLabel` forms) all go through `updateGroupColumns` in `src/group-columns.ts`. Its cell type:

   ```ts
   export type GroupColumns = Record<string, (number | string | boolean | null)[]>;
   ```

   At HEAD a `bigint` first cell falls into the final branch:
   ```ts
    } else {
      throw new QuiverError(
        `Cannot ${caller}: column '${colName}' has unsupported value type ${typeof first}`,
      );
   ```
   After plan 31, a `bigint` in any cell of a numeric column reaches `numericCells`'s throw
   (`numeric column '...' has unsupported value type bigint in cell r`).

   Repro: `db.updateVectorGroup("AllTypes", "counts", id, { count_value: [9007199254740993n] })`
   throws, although `db.createElement("AllTypes", { label: "x", count_value: [9007199254740993n] })`
   writes it exactly.

2. **Query parameters** — `src/types.ts`:
   ```ts
   export type QueryParam = number | boolean | string | null;
   ```
   and `marshalParams` in `src/query.ts` has no `bigint` branch:
   ```ts
    } else {
      throw new QuiverError(`Unsupported query parameter type at index ${i}: ${typeof p}`);
    }
   ```
   Repro: `db.queryString("SELECT CAST(? AS TEXT)", [9007199254740993n])` throws; passing
   `Number(9007199254740993n)` silently loses the last digit.

Principle: Homogeneity — one binding, one int64 input rule on every write path.

## Constraints and decisions

- **Maintainer decision (binding):** map a `bigint` through `Number()` only in the FLOAT fallback of a
  mixed column (int-for-REAL rule; lossy past 2^53 exactly as any JS number). Tests go above 2^53 and
  read back through `CAST(... AS TEXT)`.
- The C API is unchanged: `DATA_TYPE_INTEGER` data is an `int64_t*`, which `allocNativeInt64`
  already fills from `(number | bigint)[]` via `setBigInt64(BigInt(v))`.
- Keep the Bun FFI house style (TypedArray marshalling, no DataView between `ptr()` and the call) —
  root "Do Not Fix" protects it.
- Plan 31's contract for `numericCells` (per-cell boolean → 1/0, throw on non-numeric, named
  column/cell) stays; this plan only adds `bigint` as an accepted numeric cell.

## Changes

### 1. `bindings/js/src/group-columns.ts` — widen `GroupColumns`

Current:
```ts
/** Column-oriented group payload: one array of cells per column name, `null` for SQL NULL. */
export type GroupColumns = Record<string, (number | string | boolean | null)[]>;
```
New:
```ts
/**
 * Column-oriented group payload: one array of cells per column name, `null` for SQL NULL. A
 * `bigint` cell is written as an exact int64 (like createElement); a `boolean` as INTEGER 1/0.
 */
export type GroupColumns = Record<string, (number | bigint | string | boolean | null)[]>;
```

### 2. `bindings/js/src/group-columns.ts` — `numericCells` accepts `bigint`

(As introduced by plan 31.) Current:
```ts
export function numericCells(
  caller: string,
  name: string,
  values: readonly unknown[],
): (number | null)[] {
  return values.map((v, r) => {
    if (v === null || typeof v === "number") return v;
    if (typeof v === "boolean") return v ? 1 : 0;
    throw new QuiverError(
      `Cannot ${caller}: numeric column '${name}' has unsupported value type ${typeof v} in cell ${r}`,
    );
  });
}
```
New:
```ts
export function numericCells(
  caller: string,
  name: string,
  values: readonly unknown[],
): (number | bigint | null)[] {
  return values.map((v, r) => {
    if (v === null || typeof v === "number" || typeof v === "bigint") return v;
    if (typeof v === "boolean") return v ? 1 : 0;
    throw new QuiverError(
      `Cannot ${caller}: numeric column '${name}' has unsupported value type ${typeof v} in cell ${r}`,
    );
  });
}
```
Update its doc comment's bullet list with one line: "- A `bigint` stays a `bigint`, so an INTEGER
column keeps it exact; the caller maps it through `Number()` only if the column turns out FLOAT."
Remove `bigint` from the "`undefined`, objects, `bigint` and strings all reach the `throw`" sentence
if plan 31 left such a sentence in the comment.

### 3. `bindings/js/src/group-columns.ts` — the numeric branch of `updateGroupColumns`

(As left by plan 31.) Current:
```ts
    } else if (typeof first === "number" || typeof first === "boolean") {
      const cells = numericCells(caller, colName, values);
      const sanitized = cells.map((v) => v ?? 0);
      if (cells.every((v) => v === null || Number.isInteger(v))) {
        typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
        const p = allocNativeInt64(sanitized);
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      } else {
        typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
        const p = allocNativeFloat64(sanitized);
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      }
```
New:
```ts
    } else if (typeof first === "number" || typeof first === "boolean" || typeof first === "bigint") {
      const cells = numericCells(caller, colName, values);
      if (cells.every((v) => v === null || typeof v === "bigint" || Number.isInteger(v))) {
        typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
        const p = allocNativeInt64(cells.map((v) => v ?? 0));
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      } else {
        // A fractional cell makes the column FLOAT; a bigint cell then follows the int-for-REAL
        // rule through Number(), exact up to 2^53 like any JS number.
        typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
        const p = allocNativeFloat64(cells.map((v) => (v === null ? 0 : Number(v))));
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      }
```
Why `Number(v)` in the FLOAT branch: `allocNativeFloat64` calls `DataView.setFloat64(i*8, values[i])`,
which throws a raw `TypeError: Cannot convert a BigInt value to a number` on a `bigint`.
`Number.isInteger(5n)` is `false` (it is not a `number`), hence the explicit `typeof v === "bigint"`
test in the INTEGER predicate.

If plan 31 has **not** landed (numeric branch still reads `typeof first === "number"` with
`nonNull`/`sanitized`), stop and land 31 first — this plan edits 31's shape.

### 4. `bindings/js/src/types.ts` — widen `QueryParam`

Current:
```ts
export type QueryParam = number | boolean | string | null;
```
New:
```ts
/** A `bigint` binds as an exact INTEGER; a `boolean` as INTEGER 1/0. */
export type QueryParam = number | bigint | boolean | string | null;
```

### 5. `bindings/js/src/query.ts` — `marshalParams` binds a `bigint` as INTEGER

Current (the boolean branch):
```ts
    } else if (typeof p === "boolean") {
      typesDv.setInt32(i * 4, DATA_TYPE_INTEGER, true);
      const native = allocNativeInt64([p ? 1 : 0]);
      keepalive.push(native);
      valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
    } else if (typeof p === "number") {
```
New — insert a `bigint` branch right after the boolean one:
```ts
    } else if (typeof p === "boolean") {
      typesDv.setInt32(i * 4, DATA_TYPE_INTEGER, true);
      const native = allocNativeInt64([p ? 1 : 0]);
      keepalive.push(native);
      valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
    } else if (typeof p === "bigint") {
      typesDv.setInt32(i * 4, DATA_TYPE_INTEGER, true);
      const native = allocNativeInt64([p]);
      keepalive.push(native);
      valuesDv.setBigInt64(i * 8, nativeAddress(native.ptr), true);
    } else if (typeof p === "number") {
```
(`valuesDv` is a DataView over a buffer that is only `ptr()`-ed after the loop, so writing the
address into it here is the existing pattern, not the pitfall.)

If plan 22 has landed, `marshalParams`' callers change but its body is the same; apply step 5 to
the body wherever it now lives (`grep -n "function marshalParams" bindings/js/src/*.ts`).

No change is needed in `create.ts` (already bigint-aware) or `time-series.ts` `upsertRowColumns`
(already bigint-aware).

## Tests

All in the JS suite; the core is unchanged.

### `bindings/js/test/database-update.test.ts` — group writer keeps a bigint exact

Add inside the existing `describe("updateVectorGroup / updateSetGroup", ...)` block (currently ~L148).
The file's `SCHEMA_PATH` constant is already `all_types.sql`:

```ts
test("updateVectorGroup keeps a bigint cell beyond Number.MAX_SAFE_INTEGER exact", () => {
  const db = Database.fromSchema(":memory:", SCHEMA_PATH); // all_types.sql, set at the top of the file
  try {
    const id = db.createElement("AllTypes", { label: "Item1" });
    const big = 9007199254740993n; // 2^53 + 1
    db.updateVectorGroup("AllTypes", "counts", id, { count_value: [big, 7n] });
    expect(
      db.queryString(
        "SELECT CAST(count_value AS TEXT) FROM AllTypes_vector_counts WHERE vector_index = 1",
      ),
    ).toBe("9007199254740993");
    expect(db.readVectorIntegersById("AllTypes", "count_value", id)).toEqual([
      9007199254740992, // readers return number (documented), so the last digit is not exact here
      7,
    ]);
  } finally {
    db.close();
  }
});

test("a mixed bigint/fractional column is written as FLOAT", () => {
  const db = Database.fromSchema(":memory:", SCHEMA_PATH); // all_types.sql, set at the top of the file
  try {
    const id = db.createElement("AllTypes", { label: "Item1" });
    db.updateVectorGroup("AllTypes", "scores", id, { score: [5n, 1.5] });
    expect(db.readVectorFloatsById("AllTypes", "score", id)).toEqual([5, 1.5]);
  } finally {
    db.close();
  }
});
```

Verified at HEAD: `vector_index` is 1-based (`src/database_impl.h`: "vector tables get a 1-based
vector_index column"), the file's `SCHEMA_PATH` is `all_types.sql`, the group names are `counts`
(`AllTypes_vector_counts.count_value INTEGER NOT NULL`) and `scores` (`AllTypes_vector_scores.score
REAL NOT NULL`), and `readVectorIntegersById`/`readVectorFloatsById` exist in `src/read.ts`.
`Number(9007199254740993n) === 9007199254740992`, which is why the reader assertion shows the
rounded value: readers return `number` by design.

Before the change, both tests throw `QuiverError` (`unsupported value type bigint`); the second
would also have hit a raw `TypeError` in `setFloat64` if only the dispatch were widened.

### `bindings/js/test/database-query.test.ts` — query parameter keeps a bigint exact

Add inside `describe("queryString", ...)`:
```ts
test("binds a bigint parameter exactly", () => {
  const db = Database.fromSchema(":memory:", SCHEMA_PATH);
  try {
    expect(db.queryString("SELECT CAST(? AS TEXT)", [9007199254740993n])).toBe("9007199254740993");
  } finally {
    db.close();
  }
});
```
And inside `describe("queryInteger", ...)` (currently ~L55):
```ts
test("binds a bigint parameter", () => {
  const db = Database.fromSchema(":memory:", SCHEMA_PATH);
  try {
    expect(db.queryInteger("SELECT ? + 1", [41n])).toBe(42);
  } finally {
    db.close();
  }
});
```
Before the change both throw `Unsupported query parameter type at index 0: bigint`.

### Existing tests that must keep passing unchanged

- `test/database-time-series-row.test.ts` "rejects a cell that is neither string, number, bigint nor
  boolean" — row upsert path, untouched.
- Plan 31's `numericCells` tests (if 31 added a test that a `bigint` cell throws in a group column,
  that test must be **flipped** to assert success: search with
  `grep -n "bigint" bindings/js/test/*.ts` after 31 lands).

## Docs and changelog

- `bindings/js/CLAUDE.md`, "int64 handling" bullet. Current:
  > input params accept `number | bigint` — `allocNativeInt64` writes each element with
  > `DataView.setBigInt64`, so `bigint` inputs (scalar or array) are preserved exactly, never
  > coerced through `Number`.

  New:
  > input params accept `number | bigint` on every write path — `createElement`/`updateElement`
  > scalars and arrays, the group writers (`GroupColumns`), `upsertTimeSeriesRow` and query
  > parameters (`QueryParam`). `allocNativeInt64` writes each element with `DataView.setBigInt64`,
  > so a `bigint` is preserved exactly, never coerced through `Number` — except in a group column
  > that also holds a fractional cell, which is written FLOAT and maps the `bigint` through
  > `Number()` (int-for-REAL).

  In the boolean paragraph ("`ScalarValue`/`ArrayValue`/`QueryParam`/`GroupColumns` include it"),
  no change.
- `bindings/js/README.md` type list (currently ~L179-183): change
  `QueryParam -- number | string | null` to `QueryParam -- number | bigint | boolean | string | null`.
  If plan 74 already rewrote the Types section, just make sure `QueryParam` and `GroupColumns` list
  `bigint`.
- `CHANGELOG.md`, `## [0.11.0] — unreleased` → `### Fixed`:
  ```markdown
  - **JS: `bigint` is accepted by the group writers and as a query parameter.** `updateVectorGroup`,
    `updateSetGroup`, `updateTimeSeriesGroup` (and their `ByLabel` forms) and every `query*` method
    now take a `bigint` cell or parameter and write it as an exact int64, as `createElement` and
    `upsertTimeSeriesRow` already did. Previously they threw `unsupported value type bigint`.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug` (only if natives are stale).
2. `bindings/js/test/test.bat test/database-update.test.ts test/database-query.test.ts`
3. `bindings/js/test/test.bat` — full JS suite green.
4. `cd bindings/js && bunx biome check src/group-columns.ts src/query.ts src/types.ts` (touched
   files only).

## Acceptance criteria

- [ ] `GroupColumns` and `QueryParam` include `bigint`.
- [ ] `numericCells` returns `bigint` cells unchanged; the INTEGER branch writes them exactly.
- [ ] A mixed `[5n, 1.5]` column writes FLOAT `[5, 1.5]` with no raw `TypeError`.
- [ ] `marshalParams` binds a `bigint` as INTEGER.
- [ ] New tests pass; full JS suite green; CHANGELOG + CLAUDE.md + README updated.

## Pitfalls

- `Number.isInteger(5n)` is `false`; the INTEGER predicate needs its own `typeof v === "bigint"`.
- `DataView.setFloat64` throws on a `bigint`; map through `Number()` in the FLOAT branch.
- The readers still return `number` (documented, deliberate) — do not "fix" them to `bigint` here.
- Plan 31 must already be in; otherwise this plan's quoted "current" code will not match.

## Out of scope

- Returning `bigint` from readers.
- Any change to `create.ts` / `upsertRowColumns` (already correct).
- The query C API shape (plan 22).
