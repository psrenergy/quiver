# 31 — JS: per-cell numeric check and boolean normalization in both array marshallers

**Batch** 4 · **Severity** medium · **Breaking** yes, for JS callers who pass a numeric array or
group column containing a non-number cell (it now throws instead of silently coercing) · **Size** S
· **Layers** JS binding only (`bindings/js/src/group-columns.ts`, `bindings/js/src/create.ts`,
JS tests, `bindings/js/CLAUDE.md`, `CHANGELOG.md`)

**Depends on** none · **Overlaps with**
- **24** (Python + Dart numeric column typing). Its notes say to "fix the Dart/JS comments citing
  Python int(v)". The JS one is the comment in `updateGroupColumns` (`group-columns.ts`, currently
  ~L98-102, "...and matches Python's per-cell `int(v)`"). This plan deletes that comment and moves
  its content into the `numericCells` doc comment, without the Python reference. When plan 24 runs,
  the JS half is already done, so it only needs to fix the Dart comment.
- **33** (JS bigint in group writers and query params). It edits the same numeric branch of
  `updateGroupColumns` and the `GroupColumns` type. After this plan, that branch runs through
  `numericCells`, which currently rejects `bigint`. Plan 33 has to widen `numericCells` (accept
  `bigint`, return `(number | bigint | null)[]`) and the INTEGER test, instead of editing the old
  inline `nonNull`/`sanitized` code, which is gone.
- **32** (JS package exports) edits `src/index.ts`. `numericCells` is module-internal and must
  **not** be added to `src/index.ts` or `mod.ts`.
- **41** (stale alignment comments) edits `bindings/js/CLAUDE.md` too, but a different paragraph
  (the "Time-series NULL cells" bullet, ~L95). No textual conflict.

## Why

The JS binding has two array marshallers. Both pick a column's type from **one** cell and then
cast every other cell to that type without checking it.

### 1. Group writers (`updateGroupColumns`)

`bindings/js/src/group-columns.ts`, the per-column loop of `updateGroupColumns` (currently ~L96-149):

```ts
    const isStringColumn = typeof rawValues.find((v) => v !== null) === "string";
    const values = isStringColumn
      ? rawValues
      : rawValues.map((v) => (typeof v === "boolean" ? (v ? 1 : 0) : v));
    const first = values.find((v) => v !== null);
    ...
    } else if (typeof first === "number") {
      const nonNull = values.filter((v) => v !== null) as number[];
      const sanitized = values.map((v) => (v === null ? 0 : (v as number)));
      if (nonNull.every((v) => Number.isInteger(v))) {
        ...allocNativeInt64(sanitized)
      } else {
        ...allocNativeFloat64(sanitized)
      }
```

Say the first non-null cell is a number and a later cell is not (for example a string). That cell
fails `Number.isInteger`, so the whole column goes FLOAT. `allocNativeFloat64`
(`bindings/js/src/ffi-helpers.ts`, currently ~L115-122) then calls
`dv.setFloat64(i * 8, values[i], true)`, and `setFloat64` applies ToNumber with no error:

- `"abc"` becomes NaN. The core binds it with `sqlite3_bind_double` (`src/database.cpp` ~L161),
  and SQLite stores NaN as NULL.
- `"2"` becomes 2.

All six group writers share this loop: `updateTimeSeriesGroup`, `updateVectorGroup`,
`updateSetGroup`, and the `ByLabel` form of each. The payload is **type-valid**, because
`GroupColumns = Record<string, (number | string | boolean | null)[]>` (`group-columns.ts` ~L14). A
caller who respects the declared contract loses data. Reproduction with
`tests/schemas/valid/nullable_time_series.sql` (`temperature REAL`, nullable):

| Call | Now | Expected |
|---|---|---|
| `updateTimeSeriesGroup("Sensor","readings",id,{date_time:["2024-01-01","2024-01-02"], temperature:[1.5,"abc"]})` | succeeds; `temperature` reads back `[1.5, null]` | throws, naming the column |
| same, `temperature:[1,"2"]` | succeeds; reads back `[1, 2]` | throws |
| same, `temperature:["2"]` (string column) | core rejects: `Cannot update_time_series_group: column 'temperature' has type REAL but received TEXT` | unchanged |

The last row shows the inconsistency. A `"2"` on its own is rejected by the core, but the same
`"2"` after a number is silently turned into 2.

(These results come from the review's Bun probe against the built library, and the code trace
above confirms them. `src/` has no NaN guard on the write path.)

### 2. Element arrays (`setElementArray`)

`bindings/js/src/create.ts`, `setElementArray` (currently ~L38-42):

```ts
  if (typeof first === "boolean") {
    const arr = allocNativeInt64((values as boolean[]).map((v) => (v ? 1 : 0)));
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.buf, values.length, null));
    return;
  }
```

A leading boolean truthiness-maps the whole array. So
`createElement("AllTypes", {label: "A", count_value: [true, 5, false, 7]})` stores `[1, 1, 0, 1]`.
The same payload through `updateVectorGroup` stores `[1, 5, 0, 7]`, and
`test/database-boolean.test.ts` ("a mixed boolean/integer group column keeps its integer cells")
pins that. `bindings/js/CLAUDE.md` (~L106-110) names exactly this `[true, 5]` → `[1, 1]` failure as
the reason the group marshaller converts per cell, but the fix never reached `setElementArray`.
Its number branch (currently ~L44-56) has the same string-to-NaN hole as the group writers:
`createElement("AllTypes", {label: "A", score: [1.5, "2"]})` stores `[1.5, 2.0]` in the REAL
column `score` with no error.

`ArrayValue = number[] | bigint[] | boolean[] | string[]` (`types.ts` ~L26) forbids a mixed array
statically, so only plain-JS or casting callers reach this half. The maintainer decided to fix it
anyway, so that both writers agree cell for cell.

**Principles violated:** root CLAUDE.md "One scalar typing policy lives in C++ … bindings never
coerce schema-dependently". Here the binding coerces a string into a number, which the core would
have rejected. Also **Homogeneity**: Dart (`database_update.dart` `_marshalGroupColumn`,
`element.dart`) and Lua (`lua_table_to_vector`) both check every cell.

**Corrections to the review finding:**
- Its claim that "Python raises `ValueError` and converts per cell" is only partly true. An
  int-first Python column silently runs `int()` on its cells, and plan 24 owns that. Python is not
  the model here.
- Severity is medium, not high. The createElement half is reachable only by untyped callers.
- Only `setFloat64` matters for the coercion. A non-number cell always fails `Number.isInteger`, so
  it never reaches `allocNativeInt64`.
- The original proposal was a `marshalColumn` classifier that returns allocations. The maintainer
  rejected it (see Constraints).

## Constraints and decisions

- **Maintainer decision (binding):** "One small exported `numericCells(caller, name, values)`
  helper in `group-columns.ts`, used by `updateGroupColumns` and `setElementArray`. No
  `marshalColumn` classifier returning allocations. String columns keep current behaviour."
- Root CLAUDE.md **Error Messages**: the one exception to "all messages come from C++" is a pre-FFI
  type-marshalling error, "crafted locally and should name the offending column and type". The new
  message names the column, the JS type and the cell index. It uses Pattern 1,
  `Cannot {operation}: {reason}`, where `{operation}` is the JS method the caller used, as the
  existing `updateGroupColumns` messages already do.
- Root CLAUDE.md **Boolean** decision: "On the write side a boolean is accepted wherever an integer
  is … mapping it to INTEGER 1/0". JS conversion happens in `setElementField` / `setElementArray` /
  `marshalParams` / `updateGroupColumns` / `upsertRowColumns`. That list stays true.
- Root CLAUDE.md **Element arrays accept NULL cells**: "Julia/Python/JS pass a dense (NULL) mask
  and keep their non-null surfaces". So this plan does not add null support to JS element arrays.
- `bindings/js/CLAUDE.md`, the Bun FFI house rule: build masks by direct `Uint8Array` indexing,
  never with a `DataView`. The mask loop is kept exactly as it is.
- `bindings/js/CLAUDE.md`: "There is pre-existing lint debt in untouched files — fix only what
  your change orphans".
- Status is WIP and breaking changes are acceptable. 0.11.0 is already a minor bump, so no manifest
  change is needed.

Rejected alternatives:
- **`marshalColumn` returning `{type, data, keepalive}` for both writers.** Rejected by the
  maintainer. Element arrays take `bigint` and pass no mask; group columns carry a mask and no
  `bigint`. Merging them is a refactor beyond the bug.
- **A separate inline guard in each marshaller** (the verifiers' corrected proposal). Rejected by
  the maintainer in favour of one shared helper, so the two writers cannot drift apart again (they
  already did once, over booleans).
- **Also rejecting non-string cells in string columns** (`['a', true]`). Rejected by the
  maintainer: string columns keep current behaviour.
- **Converting `"2"` with `Number()`.** That is binding-side coercion that depends on the schema,
  which the root typing policy forbids.
- **Wording "mixes number and string cells"** (the verifiers' suggestion). Instead this plan keeps
  JS's existing `column '<name>' has unsupported value type <T>` wording, used by
  `updateGroupColumns` and `upsertRowColumns`, and adds `in cell <r>`, as Dart does
  (`Unsupported value type <T> in cell <r> of column '<c>'`). One grep then finds every JS
  marshalling rejection.
- **Accepting `bigint` in `numericCells` now.** Plan 33 owns `bigint` for the group writers.
  Accepting it here without plan 33's INTEGER-test change would send `[1, 5n]` to `setFloat64`,
  which throws a raw `TypeError`.

## Changes

### Step 1 — `bindings/js/src/group-columns.ts`: add `numericCells`

Insert the new exported function between the `ColumnUpdateFn` type (ends currently ~L32 with
`) => number;`) and the doc comment of `updateGroupColumns`
(`/**\n * Marshal a column-oriented payload and forward it ...`, currently ~L34). The imports
already present (`QuiverError` from `./errors.ts`) cover it.

```ts
/**
 * Check and normalize the cells of a numeric column: one whose first non-null cell is a number or
 * a boolean. Shared by updateGroupColumns and setElementArray (create.ts) so the group writers and
 * the element arrays agree on every cell. String columns are never passed here.
 *
 * - A boolean becomes INTEGER 1/0, since SQLite has no boolean type. Per cell, because mapping the
 *   whole column from a leading boolean truthiness-mapped the rest and rewrote [true, 5] as
 *   [1, 1]. Before the INTEGER/FLOAT choice, because Number.isInteger(true) is false.
 * - Any other non-null cell that is not a number throws, naming the column and the cell. It would
 *   fail Number.isInteger, tag the column FLOAT and reach DataView.setFloat64, which converts it
 *   with no error: "abc" to NaN, which SQLite stores as NULL, and "2" to 2.
 */
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

`undefined`, objects, `bigint` and strings all reach the `throw`. `typeof v` gives `"undefined"`,
`"object"`, `"bigint"` or `"string"`, and `null` is handled first, so it never reports `"object"`.

### Step 2 — `bindings/js/src/group-columns.ts`: route the numeric branch of `updateGroupColumns` through it

Replace the whole per-column `for` loop body of `updateGroupColumns` (currently ~L96-149, from
`for (let c = 0; c < columnCount; c++) {` through its closing `}` just before
`const typesAlloc: Allocation = ...`).

Current:

```ts
  for (let c = 0; c < columnCount; c++) {
    const [colName, rawValues] = entries[c];
    // SQLite has no boolean type: a boolean is INTEGER 1/0, as in setElementField and
    // marshalParams. Normalizing per cell before the dispatch (rather than adding a boolean
    // branch after it) is what makes a mixed [true, 5] column write 1 and 5 instead of
    // truthiness-mapping every cell, and matches Python's per-cell `int(v)`. A string column is
    // left alone so normalizing cannot change what a mixed ['a', true] column already wrote.
    const isStringColumn = typeof rawValues.find((v) => v !== null) === "string";
    const values = isStringColumn
      ? rawValues
      : rawValues.map((v) => (typeof v === "boolean" ? (v ? 1 : 0) : v));
    const first = values.find((v) => v !== null);

    // Mask via direct indexing — never a DataView, to avoid the documented
    // .buffer-materialization pitfall between ptr() and the FFI call.
    const maskBuf = new Uint8Array(rowCount);
    for (let r = 0; r < rowCount; r++) maskBuf[r] = values[r] === null ? 0 : 1;
    ...
    } else if (typeof first === "number") {
      const nonNull = values.filter((v) => v !== null) as number[];
      const sanitized = values.map((v) => (v === null ? 0 : (v as number)));
      if (nonNull.every((v) => Number.isInteger(v))) {
```

New (complete loop):

```ts
  for (let c = 0; c < columnCount; c++) {
    const [colName, values] = entries[c];
    const first = values.find((v) => v !== null);

    // Mask via direct indexing — never a DataView, to avoid the documented
    // .buffer-materialization pitfall between ptr() and the FFI call.
    const maskBuf = new Uint8Array(rowCount);
    for (let r = 0; r < rowCount; r++) maskBuf[r] = values[r] === null ? 0 : 1;
    const maskAlloc: Allocation = { ptr: ptr(maskBuf), buf: maskBuf };
    keepalive.push(maskAlloc);
    maskPtrs.push(maskAlloc.ptr);

    if (first === undefined) {
      // All-null column
      typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
      const p = allocNativeFloat64(new Array(rowCount).fill(0));
      keepalive.push(p);
      dataPtrs.push(p.ptr);
    } else if (typeof first === "string") {
      // Not checked by numericCells: a mixed ['a', true] column still writes what it always wrote.
      typesDv.setInt32(c * 4, DATA_TYPE_STRING, true);
      const { table, keepalive: strPtrs } = allocNativeStringArray(
        values.map((v) => (v === null ? null : (v as string))),
      );
      keepalive.push(table, ...strPtrs);
      dataPtrs.push(table.ptr);
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
    } else {
      throw new QuiverError(
        `Cannot ${caller}: column '${colName}' has unsupported value type ${typeof first}`,
      );
    }
  }
```

What changes:
- `rawValues` / `isStringColumn` / the pre-mapped `values` are gone. `first` now comes from the raw
  cells, so the numeric dispatch also accepts `typeof first === "boolean"`.
- The mask is computed from the raw cells. That is equivalent, because the boolean normalization
  never turns a cell into `null` or out of it.
- `nonNull` is replaced by `cells.every((v) => v === null || Number.isInteger(v))`, which is the
  same predicate.
- The string branch, the all-null branch and the final `unsupported value type` branch (for an
  object / `bigint` / `undefined` **first** cell) are byte-for-byte unchanged.
- The old comment ("...matches Python's per-cell `int(v)`...") is deleted. Its reasoning now lives
  in the `numericCells` doc comment.

### Step 3 — `bindings/js/src/create.ts`: import the helper

Current (currently ~L11):

```ts
import { type GroupColumns, updateGroupColumns } from "./group-columns.ts";
```

New:

```ts
import { type GroupColumns, numericCells, updateGroupColumns } from "./group-columns.ts";
```

### Step 4 — `bindings/js/src/create.ts`: `setElementArray` takes a caller and uses `numericCells`

Replace the whole `setElementArray` function (currently ~L17-67).

Current (excerpt):

```ts
function setElementArray(
  lib: Symbols,
  elemPtr: NativePointer,
  name: string,
  values: unknown[],
): void {
  ...
  if (typeof first === "boolean") {
    const arr = allocNativeInt64((values as boolean[]).map((v) => (v ? 1 : 0)));
    ...
  }

  if (typeof first === "number") {
    const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
```

New (complete function):

```ts
function setElementArray(
  lib: Symbols,
  elemPtr: NativePointer,
  caller: string,
  name: string,
  values: unknown[],
): void {
  const nameBuf = toCString(name);

  if (values.length === 0) {
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, null, 0, null));
    return;
  }

  const first = values[0];

  if (typeof first === "bigint") {
    const arr = allocNativeInt64(values as bigint[]);
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.buf, values.length, null));
    return;
  }

  if (typeof first === "number" || typeof first === "boolean") {
    // ArrayValue has no null cell (JS element arrays pass a dense mask), so the cast only narrows.
    const cells = numericCells(caller, name, values) as number[];
    if (cells.every((v) => Number.isInteger(v))) {
      const arr = allocNativeInt64(cells);
      check(
        lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.buf, values.length, null),
      );
    } else {
      const arr = allocNativeFloat64(cells);
      check(lib.quiver_element_set_array_float(elemPtr, nameBuf.buf, arr.buf, values.length, null));
    }
    return;
  }

  if (typeof first === "string") {
    const { table, keepalive: _keepalive } = allocNativeStringArray(values as string[]);
    check(
      lib.quiver_element_set_array_string(elemPtr, nameBuf.buf, table.buf, values.length, null),
    );
    return;
  }

  throw new QuiverError(`Unsupported array element type for '${name}': ${typeof first}`);
}
```

What changes:
- The new `caller` parameter.
- The `typeof first === "boolean"` branch is deleted.
- The number branch now also takes a leading boolean and runs every cell through `numericCells`.

The `bigint`, string and final-throw branches are unchanged. So is the
`Unsupported array element type` message.

### Step 5 — `bindings/js/src/create.ts`: thread `caller` through `setElementField` and its three callers

`setElementField` (currently ~L69), current:

```ts
function setElementField(lib: Symbols, elemPtr: NativePointer, name: string, value: Value): void {
```

New:

```ts
function setElementField(
  lib: Symbols,
  elemPtr: NativePointer,
  caller: string,
  name: string,
  value: Value,
): void {
```

Inside it, the array dispatch (currently ~L102-105), current:

```ts
  if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, name, value);
    return;
  }
```

New:

```ts
  if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, caller, name, value);
    return;
  }
```

The rest of `setElementField` is unchanged, including
`Unsupported value type for '${name}': ${typeof value}`, which
`test/database-create.test.ts` pins.

The three call sites, each currently `setElementField(lib, elemPtr, key, value);`:
- in `Database.prototype.createElement` (currently ~L125) → `setElementField(lib, elemPtr, "createElement", key, value);`
- in `Database.prototype.updateElement` (currently ~L153) → `setElementField(lib, elemPtr, "updateElement", key, value);`
- in `Database.prototype.updateElementByLabel` (currently ~L179) → `setElementField(lib, elemPtr, "updateElementByLabel", key, value);`

`grep -n "setElementField\|setElementArray" bindings/js/src` must show exactly these three calls
plus the one inside `setElementField`. There are no other callers anywhere (checked: `src/`,
`test/`, `mod.ts`).

### Layers with no change

The C++ core, the C API, the FFI declarations (`bindings/js/src/loader.ts`, Julia `c_api.jl`,
Dart `bindings.dart`, Python `_c_api.py`), the Julia/Dart/Python wrappers and Lua all stay as they
are. The change sits entirely before the FFI call in the JS binding. No generator run is needed.

## Tests

Only the JS binding shows the behaviour. The C++, C API, Lua, Julia, Dart and Python suites get no
new tests. No new schema files are needed: the tests use the existing
`tests/schemas/valid/nullable_time_series.sql` and `tests/schemas/valid/all_types.sql`.

Write the three tests first and run them against the unmodified source (see Verification). All
three must fail before Steps 1-5 and pass after.

### Test 1 — `bindings/js/test/database-time-series-nulls.test.ts`

Add this as the last test inside the existing
`describe("readTimeSeriesGroup / updateTimeSeriesGroup (NULL cells)", ...)` block, after
`"null cells survive a read-modify-write round-trip"`. The file already defines
`NULLABLE_TS_SCHEMA` and imports `Database`.

```ts
  test("rejects a non-number cell in a numeric column instead of writing NULL", () => {
    const db = Database.fromSchema(":memory:", NULLABLE_TS_SCHEMA);
    try {
      const id = db.createElement("Sensor", { label: "Sensor1" });
      db.updateTimeSeriesGroup("Sensor", "readings", id, {
        date_time: ["2024-01-01"],
        temperature: [10.5],
      });

      // GroupColumns admits these payloads. "abc" used to reach setFloat64 as NaN, which SQLite
      // stores as NULL, and "2" was written as 2.0 -- although a column holding only "2" is
      // rejected by the core as TEXT.
      expect(() =>
        db.updateTimeSeriesGroup("Sensor", "readings", id, {
          date_time: ["2024-01-01", "2024-01-02"],
          temperature: [1.5, "abc"],
        }),
      ).toThrow(
        "Cannot updateTimeSeriesGroup: numeric column 'temperature' has unsupported value type string in cell 1",
      );
      expect(() =>
        db.updateTimeSeriesGroupByLabel("Sensor", "readings", "Sensor1", {
          date_time: ["2024-01-01", "2024-01-02"],
          temperature: [1, "2"],
        }),
      ).toThrow(
        "Cannot updateTimeSeriesGroupByLabel: numeric column 'temperature' has unsupported value type string in cell 1",
      );

      // Rejected before the FFI call, so the stored rows are untouched.
      const result = db.readTimeSeriesGroup("Sensor", "readings", id);
      expect(result.date_time).toEqual(["2024-01-01"]);
      expect(result.temperature).toEqual([10.5]);
    } finally {
      db.close();
    }
  });
```

Why it fails before the fix: the first `updateTimeSeriesGroup` succeeds and writes
`[1.5, null]`, so `toThrow` fails. (Bun's `toThrow(string)` is a substring match on the message.)

### Test 2 — `bindings/js/test/database-boolean.test.ts`

Add this directly after the existing test
`"a mixed boolean/integer group column keeps its integer cells"` (currently ~L102-115), inside
`describe("boolean convenience methods", ...)`. The file already defines `SCHEMA_PATH`
(all_types.sql).

```ts
  test("a mixed boolean/integer element array keeps its integer cells", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      // ArrayValue forbids a mixed array, but a plain-JS caller can pass one. createElement used
      // to dispatch on the leading boolean and truthiness-map the whole array to [1, 1, 0, 1];
      // it now normalizes per cell, like the group writers in the test above.
      const id = db.createElement("AllTypes", {
        label: "Mixed",
        count_value: [true, 5, false, 7] as unknown as number[],
      });

      expect(db.readVectorIntegersById("AllTypes", "count_value", id)).toEqual([1, 5, 0, 7]);
    } finally {
      db.close();
    }
  });
```

Why it fails before the fix: the read returns `[1, 1, 0, 1]`.

### Test 3 — `bindings/js/test/database-create.test.ts`

Add this as the last test inside `describe("createElement with arrays", ...)`, after
`"bigint array stores values beyond Number.MAX_SAFE_INTEGER without precision loss"` (currently
~L190-200). The file already imports `Database` and `type { ElementData, Value }` and defines
`SCHEMA_PATH` (all_types.sql, where `score` is the REAL column of `AllTypes_vector_scores`).

```ts
  test("rejects a non-number cell in a numeric array, naming the method and the column", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      // ArrayValue forbids a mixed array, but a plain-JS caller can pass one. "2" used to be
      // written to the REAL column as 2.0 with no error.
      const mixed = [1.5, "2"] as unknown as Value;

      expect(() => db.createElement("AllTypes", { label: "Item1", score: mixed })).toThrow(
        "Cannot createElement: numeric column 'score' has unsupported value type string in cell 1",
      );
      expect(db.numberOfElements("AllTypes")).toBe(0);

      const id = db.createElement("AllTypes", { label: "Item1", score: [9.5] });
      expect(() => db.updateElement("AllTypes", id, { score: mixed })).toThrow(
        "Cannot updateElement: numeric column 'score' has unsupported value type string in cell 1",
      );
      expect(() => db.updateElementByLabel("AllTypes", "Item1", { score: mixed })).toThrow(
        "Cannot updateElementByLabel: numeric column 'score' has unsupported value type string in cell 1",
      );
      expect(db.readVectorFloatsById("AllTypes", "score", id)).toEqual([9.5]);
    } finally {
      db.close();
    }
  });
```

Why it fails before the fix: `createElement` succeeds and writes `[1.5, 2.0]`. This test also pins
the `caller` threading from Step 5. If a call site is missed, the message reads
`Cannot undefined: ...` and the substring match fails.

### Existing tests that pin the changed code

All of these stay green without edits. Re-run them; do not change them:
- `database-boolean.test.ts`:
  - "writes booleans as integers": `count_value: [true, false]` and `code: [true]` still reach the
    INTEGER path.
  - "writes booleans as integers through the group writers".
  - "a mixed boolean/integer group column keeps its integer cells": `[true, 5, false, 7]` → the
    first cell is a boolean → `numericCells` → `[1, 5, 0, 7]`.
- `database-time-series-nulls.test.ts`:
  - "null cells round-trip…": `counter: [null, 7]` is INTEGER, `temperature: [10.5, null]` is
    FLOAT.
  - "all-null value column…": the `first === undefined` branch, unchanged.
  - "read-modify-write".
- `database-create.test.ts`:
  - "throws on unsupported type (object)" pins `"Unsupported value type for 'some_integer': object"`.
    That message is unchanged.
  - The integer/float/bigint/string/empty array tests.
- `database-update.test.ts`: every `updateVectorGroup`/`updateSetGroup` test, including
  `parent_ref: [parentA, null, parentB]` (numeric with a null) and `parent_ref: ["Parent B"]`
  (string column, unchanged).
- `database-time-series-row.test.ts` "rejects a cell that is neither string, number, bigint nor
  boolean" exercises `upsertRowColumns`, which this plan does not touch.

## Docs and changelog

### `bindings/js/CLAUDE.md`

1. Layout block (currently ~L16). Old:

   ```
   src/group-columns.ts # Shared columnar marshaller for the group writers (by id and by label)
   ```

   New:

   ```
   src/group-columns.ts # Shared columnar marshaller for the group writers (by id and by label),
                        # plus numericCells, the per-cell numeric check setElementArray shares
   ```

2. The boolean bullet (`- **\`integerToBoolean\` throws \`RangeError\`...`). Replace the lines
   currently ~L101-111. Old text, verbatim:

   ```
     has no column to name. On writes a `boolean` is an INTEGER 1/0 — `setElementField`,
     `setElementArray` and `marshalParams` each carry a `typeof === "boolean"` branch, and
     `ScalarValue`/`ArrayValue`/`QueryParam`/`GroupColumns` include it. The two group/row marshallers
     instead **normalize per cell before the type dispatch** — `updateGroupColumns`
     (`group-columns.ts`) and `upsertRowColumns` (`time-series.ts`) both map `boolean → 1/0` first, so
     no boolean branch is needed at all. Both halves of that are load-bearing: *before* the dispatch,
     because `Number.isInteger(true)` is `false` and a boolean otherwise falls through to the FLOAT
     fallback and lands in the column as FLOAT 1.0 with no error; *per cell*, because a boolean branch
     chosen from the first cell would truthiness-map the rest and silently rewrite a mixed
     `[true, 5]` column to `[1, 1]`. `updateGroupColumns` skips the normalization for a string column,
     so it cannot change what a mixed `['a', true]` column already wrote. `upsertRowColumns`'s last
   ```

   New text:

   ```
     has no column to name. On writes a `boolean` is an INTEGER 1/0 — `setElementField` and
     `marshalParams` each carry a `typeof === "boolean"` branch, and
     `ScalarValue`/`ArrayValue`/`QueryParam`/`GroupColumns` include it. The row and array marshallers
     instead **normalize per cell before the INTEGER/FLOAT choice**: `upsertRowColumns`
     (`time-series.ts`) maps its one cell, and `updateGroupColumns` (`group-columns.ts`) and
     `setElementArray` (`create.ts`) pass every numeric column — first non-null cell a number or a
     boolean — through `numericCells` (`group-columns.ts`), so no boolean branch is needed at all.
     Both halves of that are load-bearing. *Before* the choice, because `Number.isInteger(true)` is
     `false`, and a boolean would otherwise land in the column as FLOAT 1.0 with no error. *Per
     cell*, because a boolean branch chosen from the first cell truthiness-maps the rest:
     `setElementArray` had one until 0.11.0 and rewrote a mixed `[true, 5]` array to `[1, 1]`.
     `numericCells` also **throws on any other non-null cell**, with `Cannot <caller>: numeric
     column '<name>' has unsupported value type <typeof> in cell <r>`. Such a cell fails
     `Number.isInteger` and tags the column FLOAT, and `setFloat64` would convert it with no error:
     `"abc"` to NaN, which SQLite stores as NULL, and `"2"` to 2. A string column is not checked, so
     a mixed `['a', true]` column still writes the text `"true"`. A `bigint[]` element array keeps
     its own branch in `setElementArray` and is not checked either. `upsertRowColumns`'s last
   ```

   The following lines (`branch is \`typeof value === "number"\`, not an untyped \`else\`…`) stay as
   they are.

No other CLAUDE.md changes. The root CLAUDE.md boolean decision lists
"`setElementField` / `setElementArray` / `marshalParams` / `updateGroupColumns` /
`upsertRowColumns` in JS" as the places where JS converts a boolean, and that is still true.
`bindings/js/README.md` (owned by plan 74), `docs/*.md` and `bindings/js/src/lua-api.ts` are
unaffected.

### `CHANGELOG.md`

Put this under `## [0.11.0] — unreleased` → `### Changed`, as the last bullet of that subsection,
immediately before `### Fixed`. Earlier plans may have added bullets there; append after them.

```markdown
- **BREAKING — JavaScript: a numeric array or group column with a non-number cell throws.** When a
  column's first non-null cell is a number or a boolean, every other non-null cell must be a number
  or a boolean too. This applies to `createElement` / `updateElement` / `updateElementByLabel`
  arrays and to the six group writers (`updateTimeSeriesGroup`, `updateVectorGroup`,
  `updateSetGroup` and their `ByLabel` forms). The binding used to type the column from one cell
  and convert the rest with no error: in a nullable REAL column, `[1.5, "abc"]` stored
  `[1.5, NULL]` and `[1.5, "2"]` stored `[1.5, 2.0]`. It now throws
  `Cannot <method>: numeric column '<name>' has unsupported value type string in cell 1`. String
  columns are unchanged. `createElement` and `updateElement` now also map a boolean array cell to
  1/0 one cell at a time, as the group writers already did, so `[true, 5, false, 7]` stores
  `[1, 5, 0, 7]` instead of `[1, 1, 0, 1]`.

  *Adapt:* make every cell of a numeric column a number (or a boolean); convert strings with
  `Number(...)` before the call.
```

## Verification

Run from the repo root (`C:\Development\Quiver\quiver3`), in this order.

1. `cmake --build build --config Debug`. No C++ changes; this only makes sure the `build/bin`
   libraries the JS tests load are current.
2. **Before** Steps 1-5, add only the three tests, then run from Git Bash:
   ```bash
   cd bindings/js && PATH="$PWD/../../build/bin:$PATH" bun test test/database-time-series-nulls.test.ts test/database-boolean.test.ts test/database-create.test.ts
   ```
   Expected: exactly three failures:
   - `rejects a non-number cell in a numeric column instead of writing NULL` (no throw)
   - `a mixed boolean/integer element array keeps its integer cells` (`[1, 1, 0, 1]`)
   - `rejects a non-number cell in a numeric array, naming the method and the column` (no throw)
3. Apply Steps 1-5 and re-run the same command. Expected: all tests in the three files pass.
4. `bindings/js/test/test.bat`: the full JS suite, all green.
5. `cd bindings/js && bun run lint`. Run `bun install` first if `node_modules` is missing, since
   biome is a devDependency. Expected: no new diagnostics in `src/group-columns.ts`,
   `src/create.ts` or the three test files. If biome reports import ordering in `create.ts`, apply
   its fix with `bunx biome check --write src/create.ts`.
6. `scripts/format.bat`. Then `git diff --stat` should list only
   `bindings/js/src/group-columns.ts`, `bindings/js/src/create.ts`, the three test files,
   `bindings/js/CLAUDE.md` and `CHANGELOG.md`.
7. `scripts/test-all.bat`: all six suites plus the CLI smoke test. Only the JS suite is affected,
   and it must stay green.

## Acceptance criteria

- [ ] `numericCells(caller, name, values)` is exported from `bindings/js/src/group-columns.ts` and
      is **not** re-exported from `src/index.ts` or `mod.ts`.
- [ ] `updateGroupColumns` sends every column whose first non-null cell is a number or a boolean
      through `numericCells`. Its string, all-null and unsupported-first-cell branches are
      unchanged. The `isStringColumn` / `rawValues` pre-mapping and the Python `int(v)` comment are
      gone.
- [ ] `setElementArray` has no `typeof first === "boolean"` branch. It takes a `caller` and sends
      number- or boolean-first arrays through `numericCells`.
- [ ] `setElementField` threads `caller`. `createElement`, `updateElement` and
      `updateElementByLabel` pass their own names.
- [ ] `[1.5, "abc"]` through `updateTimeSeriesGroup` throws
      `Cannot updateTimeSeriesGroup: numeric column 'temperature' has unsupported value type string in cell 1`
      and writes nothing.
- [ ] `createElement` with `count_value: [true, 5, false, 7]` stores `[1, 5, 0, 7]`.
- [ ] The three new JS tests fail before the change and pass after. The whole JS suite is green.
- [ ] `bindings/js/CLAUDE.md` (layout line + boolean bullet) and the `CHANGELOG.md` **BREAKING**
      entry are updated as specified.
- [ ] No change outside `bindings/js/` except `CHANGELOG.md`.

## Pitfalls

- **Thread `caller` into all three `setElementField` call sites.** Bun does not type-check. If one
  is missed, the code runs and the message reads `Cannot undefined: …`. Test 3 catches it for all
  three methods.
- **Keep the mask built by direct `Uint8Array` indexing.** Do not switch it to a `DataView` or move
  it after a `.buffer` access. That is the documented Bun FFI pitfall in `bindings/js/CLAUDE.md`
  ("Do not 'fix'").
- **Do not run the string branch through `numericCells`**, and do not reject non-string cells in a
  string column. That is the maintainer's decision. A foreign-key column written by label
  (`parent_ref: ["Parent B"]`) must keep working, and it does, because it is a string column.
- **The package ships `.ts` source** (`package.json` `exports` → `mod.ts` → `src/*.ts`), so a
  consumer's `tsc` type-checks these files. Keep the code type-correct. The `as number[]` cast in
  `setElementArray` is needed because `numericCells` returns `(number | null)[]`, and
  `allocNativeFloat64` takes `number[]`.
- **`Number.isInteger(null)` is `false`.** In `updateGroupColumns` the INTEGER test must be
  `v === null || Number.isInteger(v)`. Otherwise a nullable integer column such as
  `counter: [null, 7]` flips to FLOAT, and the core rejects a FLOAT for an INTEGER column. The
  existing "null cells round-trip" test catches this.
- **Messages in tests are substring-matched** (`toThrow("…")`). Copy the message exactly: the word
  `numeric`, the JS method name as caller, and `in cell 1` with a 0-based index.
- **Line numbers are approximate.** Plans 24 and 33 also edit `group-columns.ts`, but they run
  after this one. Anchor on the quoted excerpts.
- **CHANGELOG:** earlier plans may already have added bullets under `### Changed`. Append; do not
  reorder or rewrap the existing ones.
- No `.bat` file is edited, so the CRLF caveat does not apply. Do not open or resave
  `bindings/js/test/test.bat` or `bindings/js/format.bat`.

## Out of scope

- **`bigint` in group columns and query params**, and folding `setElementArray`'s separate `bigint`
  branch into the `numericCells` path. Plan 33 owns both. Until then, a mixed
  `[5n, "7"]` element array is still converted by `BigInt("7")`, which is reachable only by untyped
  callers, since `ArrayValue` has `bigint[]` alone.
- **`null` cells in JS element arrays.** A plain-JS `[1, null]` is still written as `[1.0, 0.0]`,
  because `setFloat64(null)` gives 0. `ArrayValue` has no null. Supporting nulls would mean passing
  a presence mask, and the root "Element arrays accept NULL cells" decision keeps the JS surface
  dense. No plan owns this.
- **`undefined` or hole cells before the first real cell of a group column** (`[undefined, 1]`).
  That still takes the all-null branch with mask 1. It is outside `GroupColumns`, and no plan owns
  it.
- **String columns with non-string cells** (`['a', 5]` writes the text `"5"`). Kept by maintainer
  decision.
- **Python and Dart numeric column typing.** Plan 24 owns it, including the Dart comment that
  cites Python's `int(v)`.
- **Rewording the other `setElementField` / `setElementArray` messages**
  (`Unsupported value type for '<name>'`, `Unsupported array element type for '<name>'`) into
  `Cannot <caller>: …` form. They are pinned by `database-create.test.ts`, and no plan owns this.
- **NaN number cells.** NaN is a `number`, so it passes `numericCells` and SQLite stores it as
  NULL. That is not a marshalling-type issue, and no plan owns it.
