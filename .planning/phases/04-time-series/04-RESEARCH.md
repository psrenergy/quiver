# Phase 4: Time Series - Research

**Researched:** 2026-03-09
**Domain:** bun:ffi FFI binding for multi-column time series read/write + time series files CRUD
**Confidence:** HIGH

## Summary

Phase 4 adds 6 time series methods to the JS/Bun binding: `readTimeSeriesGroup`, `updateTimeSeriesGroup`, `hasTimeSeriesFiles`, `listTimeSeriesFilesColumns`, `readTimeSeriesFiles`, and `updateTimeSeriesFiles`. All C API functions already exist and are tested. The Julia, Dart, and Python bindings already implement these, providing proven patterns to follow.

The primary challenge is the columnar typed-arrays FFI pattern used by `readTimeSeriesGroup` and `updateTimeSeriesGroup`. These functions pass parallel arrays of column names, column types (int enum), and column data (void** where each element is typed differently based on the corresponding type enum). This is the most complex FFI marshaling in the project, but the `query.ts` `marshalParams` pattern already demonstrates similar type-dispatched pointer table construction in the JS binding.

**Primary recommendation:** Create a single new file `time-series.ts` containing all 6 methods, following the module augmentation pattern used by `read.ts`, `metadata.ts`, etc. Add the 7 required FFI symbols to `loader.ts`. The `readTimeSeriesGroup` return type should be `Record<string, (number | string)[]>` -- a map of column names to typed arrays, consistent with Dart's `Map<String, List<Object>>`. The `updateTimeSeriesGroup` input should accept the same shape.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| JSTS-01 | User can read time series group data | `readTimeSeriesGroup` wraps `quiver_database_read_time_series_group` with columnar output deserialization; C API returns column_names/types/data/counts |
| JSTS-02 | User can update time series group data | `updateTimeSeriesGroup` wraps `quiver_database_update_time_series_group` with columnar input marshaling; uses typed pointer tables similar to `marshalParams` in query.ts |
| JSTS-03 | User can check/list/read/update time series files | 4 methods wrapping `has_time_series_files`, `list_time_series_files_columns`, `read_time_series_files`, `update_time_series_files`; parallel string array patterns already proven in read.ts |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | Bun built-in | FFI calls to libquiver_c | Project standard, all prior phases use it |
| BigInt64Array / Float64Array / Int32Array | JS built-in | Typed array buffers for FFI out-params | Established pattern in read.ts, query.ts |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| bun:test | Bun built-in | Test framework | All JS binding tests |

No new dependencies needed. The existing stack covers everything.

## Architecture Patterns

### File Organization
```
bindings/js/src/
  time-series.ts     # NEW: 6 methods (readTimeSeriesGroup, updateTimeSeriesGroup, 4 files methods)
  loader.ts          # MODIFIED: add 7 new FFI symbol declarations
  index.ts           # MODIFIED: add `import "./time-series"` and export TimeSeriesRow type
bindings/js/test/
  time-series.test.ts  # NEW: tests for all 6 methods
```

### Pattern 1: Module Augmentation (established in prior phases)
**What:** Each feature file declares additional methods on `Database` via TypeScript module augmentation, then implements them on `Database.prototype`.
**When to use:** Always -- this is the project pattern for every binding feature.
**Example:**
```typescript
// time-series.ts
declare module "./database" {
  interface Database {
    readTimeSeriesGroup(collection: string, group: string, id: number): Record<string, (number | string)[]>;
    updateTimeSeriesGroup(collection: string, group: string, id: number, data: Record<string, (number | string)[]>): void;
    hasTimeSeriesFiles(collection: string): boolean;
    listTimeSeriesFilesColumns(collection: string): string[];
    readTimeSeriesFiles(collection: string): Record<string, string | null>;
    updateTimeSeriesFiles(collection: string, data: Record<string, string | null>): void;
  }
}
```

### Pattern 2: Columnar Read Deserialization (new, adapted from Dart)
**What:** The C API returns columnar data: `char** column_names`, `int* column_types`, `void** column_data`, `size_t column_count`, `size_t row_count`. Each `column_data[i]` is typed based on `column_types[i]`: INTEGER -> `int64_t*`, FLOAT -> `double*`, STRING/DATE_TIME -> `char**`.
**When to use:** `readTimeSeriesGroup` only.
**Example:**
```typescript
// Read column_names array (array of char* pointers)
const colNamesPtr = readPointerOut(outColumnNames);
for (let c = 0; c < colCount; c++) {
  const namePtr = readPtrAt(colNamesPtr, c * 8);
  const name = new CString(namePtr).toString();

  // Read column type
  const typesBuf = toArrayBuffer(typesPtr, 0, colCount * 4);
  const type = new Int32Array(typesBuf)[c];

  const dataColPtr = readPtrAt(dataPtr, c * 8);
  if (type === DATA_TYPE_INTEGER) {
    const buf = toArrayBuffer(dataColPtr, 0, rowCount * 8);
    const bigInts = new BigInt64Array(buf);
    result[name] = Array.from(bigInts, (v) => Number(v));
  } else if (type === DATA_TYPE_FLOAT) {
    const buf = toArrayBuffer(dataColPtr, 0, rowCount * 8);
    result[name] = Array.from(new Float64Array(buf));
  } else {
    // STRING or DATE_TIME
    result[name] = [];
    for (let r = 0; r < rowCount; r++) {
      const strPtr = readPtrAt(dataColPtr, r * 8);
      result[name].push(new CString(strPtr).toString());
    }
  }
}
```

### Pattern 3: Columnar Write Marshaling (new, adapted from query.ts marshalParams)
**What:** Build parallel native arrays for column names (pointer table), column types (Int32Array), and column data (pointer table of typed arrays). Requires a `_keepalive` array to prevent GC of intermediate buffers.
**When to use:** `updateTimeSeriesGroup` only.
**Example:**
```typescript
const columnEntries = Object.entries(data);
const columnCount = columnEntries.length;
const rowCount = columnEntries[0][1].length;

const namesPtrTable = new BigInt64Array(columnCount);
const typesArr = new Int32Array(columnCount);
const dataPtrTable = new BigInt64Array(columnCount);
const keepalive: unknown[] = [];

for (let c = 0; c < columnCount; c++) {
  const [colName, colValues] = columnEntries[c];
  const nameBuf = toCString(colName);
  keepalive.push(nameBuf);
  namesPtrTable[c] = BigInt(ptr(nameBuf));

  if (typeof colValues[0] === "number") {
    if (Number.isInteger(colValues[0])) {
      typesArr[c] = DATA_TYPE_INTEGER;
      const arr = BigInt64Array.from(colValues as number[], (v) => BigInt(v));
      keepalive.push(arr);
      dataPtrTable[c] = BigInt(ptr(arr));
    } else {
      typesArr[c] = DATA_TYPE_FLOAT;
      const arr = Float64Array.from(colValues as number[]);
      keepalive.push(arr);
      dataPtrTable[c] = BigInt(ptr(arr));
    }
  } else {
    typesArr[c] = DATA_TYPE_STRING;
    const strPtrTable = new BigInt64Array(rowCount);
    for (let r = 0; r < rowCount; r++) {
      const buf = toCString(colValues[r] as string);
      keepalive.push(buf);
      strPtrTable[r] = BigInt(ptr(buf));
    }
    keepalive.push(strPtrTable);
    dataPtrTable[c] = BigInt(ptr(strPtrTable));
  }
}
```

### Pattern 4: Boolean Out-Parameter (established in transaction.ts)
**What:** `hasTimeSeriesFiles` returns boolean via `Int32Array(1)` out-param, same as `inTransaction`.
**Example:**
```typescript
const outResult = new Int32Array(1);
check(lib.quiver_database_has_time_series_files(handle, toCString(collection), ptr(outResult)));
return outResult[0] !== 0;
```

### Pattern 5: Parallel String Arrays for Files (similar to readTimeSeriesFiles in Dart)
**What:** `readTimeSeriesFiles` returns parallel arrays of column names and paths (nullable). `updateTimeSeriesFiles` accepts them.
**Example for read:**
```typescript
// Read two parallel pointer arrays: char** columns, char** paths
const columnsArrPtr = readPointerOut(outColumns);
const pathsArrPtr = readPointerOut(outPaths);
const result: Record<string, string | null> = {};
for (let i = 0; i < count; i++) {
  const col = new CString(readPtrAt(columnsArrPtr, i * 8)).toString();
  const pathPtr = readPtrAt(pathsArrPtr, i * 8);
  result[col] = pathPtr ? new CString(pathPtr).toString() : null;
}
```

### Anti-Patterns to Avoid
- **Returning BigInt from integer time series columns:** The project convention is to use `Number()` for all integer values (see readScalarIntegers pattern). Stay consistent.
- **Forgetting to free C memory:** Every read must call the corresponding free function. `readTimeSeriesGroup` must call `quiver_database_free_time_series_data`. `readTimeSeriesFiles` must call `quiver_database_free_time_series_files`.
- **Not handling empty results:** When `column_count == 0`, return `{}` immediately before attempting pointer reads. Dereferencing null pointers crashes.
- **GC of intermediate buffers:** When building pointer tables for `updateTimeSeriesGroup`, all intermediate `Buffer` and `TypedArray` objects must survive until the FFI call returns. Use a `keepalive` array (established pattern from `query.ts`).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Type dispatch for columnar data | Custom type system | Switch on C API enum values (0=INTEGER, 1=FLOAT, 2=STRING, 3=DATE_TIME) | Matches C API exactly, proven in Dart/Julia |
| String pointer table construction | Novel approach | Copy pattern from `setElementArray` in create.ts (BigInt64Array of ptr values) | Identical pointer-table marshaling already works |
| FFI symbol declarations | Guess signatures | Copy from C API header `include/quiver/c/database.h` | Mismatched signatures cause silent corruption |

## Common Pitfalls

### Pitfall 1: column_types is int* not int**
**What goes wrong:** Declaring the FFI for `out_column_types` as `ptr` (pointer to pointer) when it should be `ptr` (pointer to int array). Both are `FFIType.ptr` in bun:ffi, but the read pattern differs.
**Why it happens:** Confusion between `int**` (out-param returning heap pointer) and `int*` (direct array pointer).
**How to avoid:** The C API uses `int** out_column_types` -- so it IS a pointer-to-pointer. Use `allocPointerOut()` + `readPointerOut()`, then `toArrayBuffer()` to read the int array.
**Warning signs:** Getting garbage values for column types.

### Pitfall 2: void*** column_data triple indirection
**What goes wrong:** `out_column_data` is `void***` -- a pointer to an array of void pointers. Each void pointer is itself a typed array (int64_t*, double*, or char**). Three levels of indirection.
**Why it happens:** This is the most complex pointer pattern in the entire C API.
**How to avoid:** Step by step: (1) `readPointerOut(outColumnData)` gets the `void**` array. (2) `readPtrAt(void**, c * 8)` gets the `void*` for column c. (3) `toArrayBuffer()` or `readPtrAt()` to read the actual typed data.
**Warning signs:** Segfaults, reading wrong memory.

### Pitfall 3: Null pointer for nullable path values
**What goes wrong:** `readTimeSeriesFiles` returns `char** paths` where `paths[i]` can be NULL (no path set). Reading a null pointer as CString crashes.
**Why it happens:** Forgetting the nullable case in the C API.
**How to avoid:** Check `readPtrAt(pathsArrPtr, i * 8)` for falsy/zero before constructing CString.
**Warning signs:** Crash when reading files table with unset paths.

### Pitfall 4: updateTimeSeriesGroup empty/clear case
**What goes wrong:** Passing non-null arrays with column_count=0 or row_count=0 triggers C API validation errors.
**Why it happens:** The C API has specific rules: pass `column_count == 0 && row_count == 0` with NULL arrays to clear.
**How to avoid:** Handle empty data (`Object.keys(data).length === 0`) by passing null/0/0. The C API explicitly checks for this.
**Warning signs:** C API returns error "column_count must be > 0 when row_count > 0".

### Pitfall 5: Integer type detection for update columns
**What goes wrong:** JS numbers don't distinguish int vs float. `Number.isInteger(1.0)` returns `true`, so `[1.0, 2.0, 3.0]` would be sent as INTEGER.
**Why it happens:** JavaScript's number type is always IEEE 754 double.
**How to avoid:** Follow the existing `setElementField` pattern from create.ts -- use `Number.isInteger()` for the first element to detect type. For time series, the schema dictates the type, so this heuristic is sufficient (schema validation happens in C API).
**Warning signs:** Type mismatch errors from the C API like "column 'value' has type FLOAT but received INTEGER".

## Code Examples

### FFI Symbol Declarations for loader.ts
```typescript
// Time series read/update
quiver_database_read_time_series_group: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64,
         FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_update_time_series_group: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64,
         FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.u64],
  returns: FFIType.i32,
},
quiver_database_free_time_series_data: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.u64],
  returns: FFIType.i32,
},

// Time series files
quiver_database_has_time_series_files: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_list_time_series_files_columns: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_time_series_files: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_update_time_series_files: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
quiver_database_free_time_series_files: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
```

### Test Schema and Pattern
The `collections.sql` schema has the required time series table (`Collection_time_series_data` with `date_time TEXT` + `value REAL`) and `Collection_time_series_files` table. The `mixed_time_series.sql` schema has a multi-column time series (`Sensor_time_series_readings` with `date_time TEXT`, `temperature REAL`, `humidity INTEGER`, `status TEXT`).

Tests should use `collections.sql` for basic single-column time series and `mixed_time_series.sql` for multi-column multi-type validation.

### TypeScript Return Type Design
```typescript
// readTimeSeriesGroup returns columnar data
// Keys are column names, values are typed arrays
// INTEGER columns -> number[], FLOAT columns -> number[], STRING/DATE_TIME -> string[]
type TimeSeriesData = Record<string, (number | string)[]>;

// readTimeSeriesFiles returns column-to-path mapping
type TimeSeriesFiles = Record<string, string | null>;
```

## State of the Art

No changes needed. The C API time series functions are stable and tested. All other bindings (Julia, Dart, Python) already implement these same functions. This is pure binding work.

## Open Questions

None. All APIs are well-defined, all patterns are established from prior phases.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built into Bun) |
| Config file | none (uses bun defaults) |
| Quick run command | `cd bindings/js && bun test test/time-series.test.ts` |
| Full suite command | `bindings/js/test/test.bat` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| JSTS-01 | Read time series group (single col, multi col, empty, multi-type) | unit | `cd bindings/js && bun test test/time-series.test.ts` | No -- Wave 0 |
| JSTS-02 | Update time series group (write + read-back, replace, clear) | unit | `cd bindings/js && bun test test/time-series.test.ts` | No -- Wave 0 |
| JSTS-03 | has/list/read/update time series files (check, list cols, read/write paths, nullable) | unit | `cd bindings/js && bun test test/time-series.test.ts` | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cd bindings/js && bun test test/time-series.test.ts`
- **Per wave merge:** `bindings/js/test/test.bat`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/time-series.test.ts` -- covers JSTS-01, JSTS-02, JSTS-03

*(Test infrastructure (bun:test, test.bat, setup.ts) already exists from prior phases.)*

## Sources

### Primary (HIGH confidence)
- `src/c/database_time_series.cpp` -- C API implementation (read directly)
- `include/quiver/c/database.h` -- C API header with all function signatures (read directly)
- `bindings/dart/lib/src/database_read.dart` lines 990-1077 -- Dart readTimeSeriesGroup implementation (read directly)
- `bindings/dart/lib/src/database_update.dart` lines 47-195 -- Dart updateTimeSeriesGroup + updateTimeSeriesFiles (read directly)
- `bindings/dart/lib/src/database_metadata.dart` lines 278-331 -- Dart hasTimeSeriesFiles + listTimeSeriesFilesColumns (read directly)
- `bindings/js/src/query.ts` -- marshalParams pattern for typed pointer tables (read directly)
- `bindings/js/src/read.ts` -- string array reading patterns (read directly)
- `bindings/js/src/create.ts` -- setElementArray pointer table pattern (read directly)
- `tests/test_database_time_series.cpp` -- C++ time series test patterns (read directly)
- `tests/schemas/valid/collections.sql` -- schema with time series tables (read directly)
- `tests/schemas/valid/mixed_time_series.sql` -- multi-column time series schema (read directly)

### Secondary (MEDIUM confidence)
None needed -- all information obtained from primary sources.

### Tertiary (LOW confidence)
None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - same stack as all prior phases, no new dependencies
- Architecture: HIGH - all patterns established in prior phases, C API fully implemented and tested
- Pitfalls: HIGH - identified from direct C API source code analysis and existing binding implementations

**Research date:** 2026-03-09
**Valid until:** Indefinite (stable C API, no expected changes)
