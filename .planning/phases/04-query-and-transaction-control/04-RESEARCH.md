# Phase 4: Query and Transaction Control - Research

**Researched:** 2026-03-09
**Domain:** bun:ffi parameterized query marshaling, transaction lifecycle, bool out-parameter handling
**Confidence:** HIGH

## Summary

Phase 4 adds two independent feature sets to the JS/TS binding: (1) query operations (plain and parameterized SQL returning typed scalar results) and (2) transaction control (begin, commit, rollback, inTransaction). These are orthogonal features that share nothing technically except both being methods on the Database class.

The query operations follow the established optional-value pattern from Phase 3's by-ID reads: each query returns a typed value plus a `has_value` boolean flag via out-parameters. The plain query variants (`queryString`, `queryInteger`, `queryFloat`) take just SQL; the parameterized variants add parallel arrays of `param_types[]` and `param_values[]` (typed void pointers). The parameter marshaling is the most technically demanding part: JS values (number, string, null) must be converted to typed C pointers in parallel arrays, with proper memory alignment and lifetime management.

Transaction control is straightforward -- three void-returning methods (`beginTransaction`, `commit`, `rollback`) that delegate directly to C API functions, plus `inTransaction()` which reads a `bool*` out-parameter. The `bool` type in MSVC is 1 byte (`_Bool`), requiring a `Uint8Array(1)` buffer for the out-parameter (not `Int32Array`).

**Primary recommendation:** Create two new source files (`query.ts` and `transaction.ts`) following the same prototype-extension pattern established in Phases 2-3. For parameterized queries, build a `marshalParams` helper that constructs parallel `Int32Array` (types) and `BigInt64Array` (pointer table) arrays. The Dart and Python bindings provide direct reference implementations.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| QUERY-01 | User can execute SQL and get string/integer/float results | `quiver_database_query_string/integer/float` C API functions with `out_value` + `out_has_value` pattern; same optional-value pattern as readScalarByIdById from Phase 3 |
| QUERY-02 | User can execute parameterized SQL with typed parameters | `quiver_database_query_string/integer/float_params` C API with parallel `param_types[]` + `param_values[]` arrays; marshalParams helper builds typed pointer table |
| TXN-01 | User can begin, commit, and rollback explicit transactions | `quiver_database_begin_transaction/commit/rollback` C API functions; simple ptr-only calls with check() |
| TXN-02 | User can check if a transaction is active via `inTransaction()` | `quiver_database_in_transaction` C API with `bool* out_active`; 1-byte bool requires Uint8Array(1) out-parameter |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | Built-in (Bun 1.3.10) | C library function calling, pointer handling, typed arrays | Established in Phase 1; all FFI primitives needed are already in use |
| bun:test | Built-in (Bun 1.3.10) | Test runner | Established in Phase 1 |
| TypeScript | Built-in (Bun 1.3.10) | Type safety | Established in Phase 1 |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Phase 1 helpers | N/A (local) | `check()`, `toCString()`, `allocPointerOut()`, `readPointerOut()` | Every C API call |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Separate `queryString`/`queryStringParams` methods | Single `queryString(sql, params?)` that dispatches | Single method is more ergonomic (Python does this); but Dart exposes them separately. Recommend: single method with optional params, dispatching to the appropriate C API function internally |
| `Uint8Array(1)` for bool* | `Int32Array(1)` | bool in C (_Bool/MSVC) is 1 byte, not 4. Using Int32Array would read 3 extra uninitialized bytes. Use Uint8Array(1) for correctness. |

**Installation:**
```bash
# No new dependencies -- all built-in to Bun
```

## Architecture Patterns

### Recommended Project Structure
```
bindings/js/
  src/
    index.ts          # Add side-effect imports for query.ts and transaction.ts
    loader.ts         # Add FFI symbol definitions for query and transaction functions
    errors.ts         # Existing (unchanged)
    database.ts       # Existing (unchanged)
    ffi-helpers.ts    # Existing (unchanged)
    types.ts          # Existing (unchanged)
    create.ts         # Existing (unchanged)
    read.ts           # Existing (unchanged)
    query.ts          # NEW: queryString, queryInteger, queryFloat (plain + parameterized)
    transaction.ts    # NEW: beginTransaction, commit, rollback, inTransaction
  test/
    database.test.ts  # Existing lifecycle tests
    create.test.ts    # Existing CRUD tests
    read.test.ts      # Existing read tests
    query.test.ts     # NEW: query operation tests
    transaction.test.ts # NEW: transaction control tests
    test.bat          # Existing test runner
```

### Pattern 1: Prototype Extension for Query/Transaction Methods
**What:** Add methods to Database class from `query.ts` and `transaction.ts` using `declare module` augmentation, same as Phase 2-3.
**When to use:** All new methods in this phase.
**Example:**
```typescript
// Source: Established project pattern from create.ts, read.ts
// query.ts
import { ptr, CString } from "bun:ffi";
import { Database } from "./database";
import { getSymbols } from "./loader";
import { check } from "./errors";
import { toCString, allocPointerOut, readPointerOut } from "./ffi-helpers";

declare module "./database" {
  interface Database {
    queryString(sql: string, params?: QueryParam[]): string | null;
    queryInteger(sql: string, params?: QueryParam[]): number | null;
    queryFloat(sql: string, params?: QueryParam[]): number | null;
  }
}
```

### Pattern 2: Unified Query Method with Optional Params
**What:** Single method signature with optional `params` that dispatches to the plain or parameterized C API function.
**When to use:** All three query methods (queryString, queryInteger, queryFloat).
**Example:**
```typescript
// Source: Python database.py pattern (single method dispatching based on params presence)
Database.prototype.queryString = function (
  this: Database,
  sql: string,
  params?: QueryParam[],
): string | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = allocPointerOut();
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const marshaled = marshalParams(params);
    check(lib.quiver_database_query_string_params(
      handle,
      toCString(sql),
      ptr(marshaled.types),
      ptr(marshaled.values),
      params.length,
      ptr(outValue),
      ptr(outHasValue),
    ));
  } else {
    check(lib.quiver_database_query_string(
      handle,
      toCString(sql),
      ptr(outValue),
      ptr(outHasValue),
    ));
  }

  if (outHasValue[0] === 0) return null;

  const strPtr = readPointerOut(outValue);
  const result = new CString(strPtr).toString();
  lib.quiver_database_free_string(strPtr);
  return result;
};
```

### Pattern 3: Parameter Marshaling (Parallel Typed Arrays)
**What:** Convert JS values to parallel `int[]` (types) and `void*[]` (values) arrays for the C API's parameterized query functions.
**When to use:** All parameterized query calls.
**Example:**
```typescript
// Source: C API database_query.cpp convert_params + Dart _marshalParams + Python _marshal_params
type QueryParam = number | string | null;

interface MarshaledParams {
  types: Int32Array;
  values: BigInt64Array;
  _keepalive: unknown[]; // prevent GC of typed buffers
}

function marshalParams(params: QueryParam[]): MarshaledParams {
  const n = params.length;
  const types = new Int32Array(n);
  const values = new BigInt64Array(n);
  const keepalive: unknown[] = [];

  for (let i = 0; i < n; i++) {
    const p = params[i];
    if (p === null) {
      types[i] = 4; // QUIVER_DATA_TYPE_NULL
      values[i] = BigInt(0); // nullptr
    } else if (typeof p === "number") {
      if (Number.isInteger(p)) {
        types[i] = 0; // QUIVER_DATA_TYPE_INTEGER
        const buf = new BigInt64Array([BigInt(p)]);
        keepalive.push(buf);
        values[i] = BigInt(ptr(buf));
      } else {
        types[i] = 1; // QUIVER_DATA_TYPE_FLOAT
        const buf = new Float64Array([p]);
        keepalive.push(buf);
        values[i] = BigInt(ptr(buf));
      }
    } else if (typeof p === "string") {
      types[i] = 2; // QUIVER_DATA_TYPE_STRING
      const buf = toCString(p);
      keepalive.push(buf);
      values[i] = BigInt(ptr(buf));
    }
  }

  return { types, values, _keepalive: keepalive };
}
```

### Pattern 4: Transaction Control (Simple Delegation)
**What:** Transaction methods delegate directly to C API with no complex marshaling.
**When to use:** beginTransaction, commit, rollback.
**Example:**
```typescript
// Source: C API database_transaction.cpp + Dart database_transaction.dart
// transaction.ts

declare module "./database" {
  interface Database {
    beginTransaction(): void;
    commit(): void;
    rollback(): void;
    inTransaction(): boolean;
  }
}

Database.prototype.beginTransaction = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_begin_transaction(this._handle));
};

Database.prototype.commit = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_commit(this._handle));
};

Database.prototype.rollback = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_rollback(this._handle));
};
```

### Pattern 5: Boolean Out-Parameter (1-byte bool)
**What:** The `in_transaction` C API uses `bool* out_active`. C `bool` (_Bool) is 1 byte on MSVC/Windows. Use `Uint8Array(1)` to receive it.
**When to use:** `inTransaction()`.
**Example:**
```typescript
// Source: C API database_transaction.cpp quiver_database_in_transaction signature
Database.prototype.inTransaction = function (this: Database): boolean {
  const lib = getSymbols();
  const outActive = new Uint8Array(1);
  check(lib.quiver_database_in_transaction(this._handle, ptr(outActive)));
  return outActive[0] !== 0;
};
```

### Anti-Patterns to Avoid
- **Using Int32Array for bool* out-parameter:** C `bool` (_Bool) is 1 byte. An Int32Array(1) is 4 bytes; the C function writes only 1 byte, leaving 3 bytes uninitialized. On little-endian (x86), the Int32Array would read the correct byte plus garbage. Use Uint8Array(1) for safety.
- **Forgetting `_keepalive` in parameter marshaling:** The `BigInt64Array` for `param_values` stores raw pointers to typed buffers. If those buffers are GC'd before the FFI call completes, the pointers become dangling. The keepalive array prevents premature GC.
- **Passing `params.length` as FFIType.i32 when it should be size_t:** The C API uses `size_t param_count`. On 64-bit, `size_t` is 8 bytes. However, since param_count is passed by value (not pointer), and bun:ffi handles argument conversion, passing a JS number for a `u64` parameter works. Use the FFI declaration that matches the C signature.
- **Not freeing query string results:** `queryString` with a result allocates a C string via `new_c_str()`. Must call `quiver_database_free_string` after reading. Integer and float query results do not allocate (stack-based out-params).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter type detection | Manual typeof chains per query call | `marshalParams()` helper function | Centralizes type detection, pointer creation, keepalive management |
| Query result unwrapping | Separate code paths for has/no value | Same optional-value pattern as readScalarByIdById | Pattern already established and tested in Phase 3 |
| Transaction error handling | Custom error messages in JS | C API error messages via `check()` | Per project principle: "Error messages are defined in the C++/C API layer" |
| Bool out-parameter reading | DataView or manual byte extraction | `Uint8Array(1)` + direct index read | Simplest correct approach for 1-byte bool |

**Key insight:** The query operations are structurally identical to Phase 3's scalar-by-ID reads (value + has_value out-params). The only new complexity is parameter marshaling, which follows a well-established cross-binding pattern (Dart's `_marshalParams`, Python's `_marshal_params`, C API's `convert_params`). Transaction control is the simplest feature in the entire binding -- just direct delegation.

## Common Pitfalls

### Pitfall 1: GC Collecting Param Buffers Before FFI Call
**What goes wrong:** The `marshalParams` function creates typed arrays (BigInt64Array for int values, Float64Array for float values, Buffer for strings) and stores their pointers in the values array. If these typed arrays are garbage-collected before the FFI function reads them, the pointers become dangling.
**Why it happens:** JavaScript GC is non-deterministic. If the only reference to a typed buffer is the raw pointer number stored in the values BigInt64Array, the GC may free the buffer.
**How to avoid:** Keep a `_keepalive` array that holds references to all intermediate typed buffers. Ensure this array stays in scope until after the `check()` call returns.
**Warning signs:** Intermittent crashes or wrong parameter values, especially under memory pressure.

### Pitfall 2: Bool Size Mismatch
**What goes wrong:** Using `Int32Array(1)` (4 bytes) for a `bool*` out-parameter that the C function writes as 1 byte. On little-endian systems, reading the int32 value includes 3 uninitialized bytes.
**Why it happens:** The C `bool` type (_Bool) is 1 byte on MSVC, not 4 bytes like `int`.
**How to avoid:** Use `Uint8Array(1)` for `bool*` out-parameters. Check `outActive[0] !== 0`.
**Warning signs:** `inTransaction()` returns seemingly random true/false values.

### Pitfall 3: Integer vs Float Parameter Detection
**What goes wrong:** A parameter like `42.0` is detected as integer because `Number.isInteger(42.0)` returns `true`, but the user intended a float.
**Why it happens:** JavaScript makes no distinction between `42` and `42.0` -- both are the same number with `Number.isInteger` returning `true`.
**How to avoid:** Document that `queryFloat("SELECT ? + 0.5", [42])` will pass 42 as an integer. Users who need a float parameter must pass `42.1` or similar. This matches the Dart and Python behavior. Alternatively, accept explicit type annotations, but that increases API complexity unnecessarily.
**Warning signs:** SQLite type coercion usually hides this (SQLite is flexible), so it rarely causes actual bugs.

### Pitfall 4: Forgetting to Free Query String Results
**What goes wrong:** `queryString` returns a C-allocated string. Not freeing it leaks memory.
**Why it happens:** `queryInteger` and `queryFloat` use stack-allocated out-params (no free needed), so developers may assume the same for string queries.
**How to avoid:** After reading the CString, call `quiver_database_free_string(strPtr)`. This mirrors the pattern from `readScalarStringById` in Phase 3.
**Warning signs:** Growing memory usage with repeated string queries.

### Pitfall 5: param_count Type for size_t Parameter
**What goes wrong:** The C API declares `param_count` as `size_t` (8 bytes on 64-bit). If the FFI symbol declares this as `FFIType.i32`, only 4 bytes are passed, which could corrupt the call stack.
**Why it happens:** Easy to mistake `size_t` for `int` in FFI declarations.
**How to avoid:** Use `FFIType.u64` for `size_t` parameters passed by value. Consistent with how `quiver_database_free_string_array` already declares its count parameter as `FFIType.u64` in the existing loader.ts.
**Warning signs:** Crashes or wrong parameter counts when param_count > 0.

## Code Examples

Verified patterns from official sources and existing code:

### FFI Symbol Definitions for Query Operations
```typescript
// Source: include/quiver/c/database.h query function signatures

// Plain query functions
quiver_database_query_string: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  // (db, sql, out_value: char**, out_has_value: int*)
  returns: FFIType.i32,
},
quiver_database_query_integer: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  // (db, sql, out_value: int64_t*, out_has_value: int*)
  returns: FFIType.i32,
},
quiver_database_query_float: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  // (db, sql, out_value: double*, out_has_value: int*)
  returns: FFIType.i32,
},

// Parameterized query functions
quiver_database_query_string_params: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr, FFIType.ptr],
  // (db, sql, param_types: int*, param_values: void**, param_count: size_t, out_value: char**, out_has_value: int*)
  returns: FFIType.i32,
},
quiver_database_query_integer_params: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_query_float_params: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
```

### FFI Symbol Definitions for Transaction Operations
```typescript
// Source: include/quiver/c/database.h transaction function signatures

quiver_database_begin_transaction: {
  args: [FFIType.ptr],
  // (db)
  returns: FFIType.i32,
},
quiver_database_commit: {
  args: [FFIType.ptr],
  // (db)
  returns: FFIType.i32,
},
quiver_database_rollback: {
  args: [FFIType.ptr],
  // (db)
  returns: FFIType.i32,
},
quiver_database_in_transaction: {
  args: [FFIType.ptr, FFIType.ptr],
  // (db, out_active: bool*)
  returns: FFIType.i32,
},
```

### Complete marshalParams Implementation
```typescript
// Source: Dart _marshalParams (database.dart:90-121) + Python _marshal_params (database.py:1430-1462)
// Adapted for bun:ffi pointer/typed array semantics

type QueryParam = number | string | null;

// C API data type enum values (from include/quiver/c/database.h)
const DATA_TYPE_INTEGER = 0;
const DATA_TYPE_FLOAT = 1;
const DATA_TYPE_STRING = 2;
const DATA_TYPE_NULL = 4;

interface MarshaledParams {
  types: Int32Array;
  values: BigInt64Array;
  _keepalive: unknown[];
}

function marshalParams(params: QueryParam[]): MarshaledParams {
  const n = params.length;
  const types = new Int32Array(n);       // param_types: int[]
  const values = new BigInt64Array(n);   // param_values: void*[] (pointers as bigints)
  const keepalive: unknown[] = [];

  for (let i = 0; i < n; i++) {
    const p = params[i];
    if (p === null) {
      types[i] = DATA_TYPE_NULL;
      values[i] = 0n; // nullptr
    } else if (typeof p === "number") {
      if (Number.isInteger(p)) {
        types[i] = DATA_TYPE_INTEGER;
        const buf = new BigInt64Array([BigInt(p)]);
        keepalive.push(buf);
        values[i] = BigInt(ptr(buf));
      } else {
        types[i] = DATA_TYPE_FLOAT;
        const buf = new Float64Array([p]);
        keepalive.push(buf);
        values[i] = BigInt(ptr(buf));
      }
    } else if (typeof p === "string") {
      types[i] = DATA_TYPE_STRING;
      const buf = toCString(p);
      keepalive.push(buf);
      values[i] = BigInt(ptr(buf));
    }
  }

  return { types, values, _keepalive: keepalive };
}
```

### Query Integer with Optional Params
```typescript
// Source: C API quiver_database_query_integer + quiver_database_query_integer_params
Database.prototype.queryInteger = function (
  this: Database,
  sql: string,
  params?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = new BigInt64Array(1);
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const marshaled = marshalParams(params);
    check(lib.quiver_database_query_integer_params(
      handle,
      toCString(sql),
      ptr(marshaled.types),
      ptr(marshaled.values),
      params.length,
      ptr(outValue),
      ptr(outHasValue),
    ));
  } else {
    check(lib.quiver_database_query_integer(
      handle,
      toCString(sql),
      ptr(outValue),
      ptr(outHasValue),
    ));
  }

  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
};
```

### Query Float with Optional Params
```typescript
// Source: C API quiver_database_query_float + quiver_database_query_float_params
Database.prototype.queryFloat = function (
  this: Database,
  sql: string,
  params?: QueryParam[],
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = new Float64Array(1);
  const outHasValue = new Int32Array(1);

  if (params && params.length > 0) {
    const marshaled = marshalParams(params);
    check(lib.quiver_database_query_float_params(
      handle,
      toCString(sql),
      ptr(marshaled.types),
      ptr(marshaled.values),
      params.length,
      ptr(outValue),
      ptr(outHasValue),
    ));
  } else {
    check(lib.quiver_database_query_float(
      handle,
      toCString(sql),
      ptr(outValue),
      ptr(outHasValue),
    ));
  }

  if (outHasValue[0] === 0) return null;
  return outValue[0];
};
```

### inTransaction with Bool Out-Parameter
```typescript
// Source: C API quiver_database_in_transaction(db, bool* out_active)
Database.prototype.inTransaction = function (this: Database): boolean {
  const lib = getSymbols();
  // C bool (_Bool) is 1 byte on MSVC/Windows
  const outActive = new Uint8Array(1);
  check(lib.quiver_database_in_transaction(this._handle, ptr(outActive)));
  return outActive[0] !== 0;
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate plain/params methods | Single method with optional params (Python pattern) | Design choice for this binding | More ergonomic API; queryString(sql) and queryString(sql, [param]) both work |

**Established from Phases 1-3:**
- `allocPointerOut()` + `readPointerOut()` confirmed working for pointer out-params
- `check()` pattern handles all error propagation from C API
- Prototype extension with `declare module` confirmed working
- Side-effect import pattern in `index.ts` confirmed working
- `Int32Array(1)` confirmed working for `int*` out-params (has_value)
- `quiver_database_free_string` confirmed working in Phase 3 (readScalarStringById)

## Open Questions

1. **QueryParam type: should bigint be accepted?**
   - What we know: Phase 2's `createElement` accepts `bigint` values for integer scalars. Query params could also accept bigint.
   - What's unclear: Whether users would pass bigint as query params. The Dart and Python bindings accept their native integer types only.
   - Recommendation: Accept `number | string | null` for now. BigInt support can be added later if needed. This keeps the type simpler and matches the C API's practical usage (SQLite integers fit in number range).

2. **Export QueryParam type?**
   - What we know: Users need to know what types are valid as query parameters. The type could be exported from index.ts.
   - What's unclear: Whether to export it as a named type or just document it.
   - Recommendation: Export `QueryParam` from `types.ts` and re-export from `index.ts`. This follows TypeScript best practices and enables IDE autocompletion.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in, Bun 1.3.10) |
| Config file | none |
| Quick run command | `bun test bindings/js/test/query.test.ts` |
| Full suite command | `bun test bindings/js/test/` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| QUERY-01 | queryString returns string or null | integration | `bun test bindings/js/test/query.test.ts -t "queryString"` | Wave 0 |
| QUERY-01 | queryInteger returns number or null | integration | `bun test bindings/js/test/query.test.ts -t "queryInteger"` | Wave 0 |
| QUERY-01 | queryFloat returns number or null | integration | `bun test bindings/js/test/query.test.ts -t "queryFloat"` | Wave 0 |
| QUERY-01 | query returns null for no rows | integration | `bun test bindings/js/test/query.test.ts -t "no rows"` | Wave 0 |
| QUERY-02 | queryString with string param | integration | `bun test bindings/js/test/query.test.ts -t "params"` | Wave 0 |
| QUERY-02 | queryInteger with integer param | integration | `bun test bindings/js/test/query.test.ts -t "integer param"` | Wave 0 |
| QUERY-02 | queryFloat with float param | integration | `bun test bindings/js/test/query.test.ts -t "float param"` | Wave 0 |
| QUERY-02 | query with null param | integration | `bun test bindings/js/test/query.test.ts -t "null param"` | Wave 0 |
| QUERY-02 | query with multiple params | integration | `bun test bindings/js/test/query.test.ts -t "multiple"` | Wave 0 |
| TXN-01 | begin + create + commit persists | integration | `bun test bindings/js/test/transaction.test.ts -t "commit"` | Wave 0 |
| TXN-01 | begin + create + rollback discards | integration | `bun test bindings/js/test/transaction.test.ts -t "rollback"` | Wave 0 |
| TXN-01 | commit without begin throws | integration | `bun test bindings/js/test/transaction.test.ts -t "without begin"` | Wave 0 |
| TXN-02 | inTransaction reflects state | integration | `bun test bindings/js/test/transaction.test.ts -t "inTransaction"` | Wave 0 |

### Sampling Rate
- **Per task commit:** `bun test bindings/js/test/`
- **Per wave merge:** `bun test bindings/js/test/`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/query.test.ts` -- covers QUERY-01 through QUERY-02
- [ ] `bindings/js/test/transaction.test.ts` -- covers TXN-01 through TXN-02
- [ ] `bindings/js/src/query.ts` -- all query operation implementations
- [ ] `bindings/js/src/transaction.ts` -- all transaction control implementations

## Sources

### Primary (HIGH confidence)
- [bun:ffi official docs](https://bun.com/docs/runtime/ffi) - FFIType table, pointer handling, typed array passing
- [bun:ffi API reference](https://bun.com/reference/bun/ffi) - Complete module exports, CString constructor
- C API header: `include/quiver/c/database.h` (lines 364-405) - All query function signatures with param_types/param_values/param_count
- C API header: `include/quiver/c/database.h` (lines 49-52) - Transaction function signatures with bool* out-parameter
- C API implementation: `src/c/database_query.cpp` - convert_params helper showing type dispatch pattern
- C API implementation: `src/c/database_transaction.cpp` - Transaction implementation (direct delegation)
- Dart binding: `bindings/dart/lib/src/database_query.dart` - Reference implementation for all 6 query variants
- Dart binding: `bindings/dart/lib/src/database_transaction.dart` - Reference implementation for transaction control
- Dart binding: `bindings/dart/lib/src/database.dart` (lines 90-121) - `_marshalParams` reference for parameter marshaling
- Python binding: `bindings/python/src/quiverdb/database.py` (lines 1430-1462) - `_marshal_params` reference
- C++ tests: `tests/test_database_query.cpp` - Complete query test coverage including parameterized queries
- C++ tests: `tests/test_database_transaction.cpp` - Complete transaction test coverage
- C API tests: `tests/test_c_api_database_query.cpp` - C API query tests showing exact parameter marshaling
- C API tests: `tests/test_c_api_database_transaction.cpp` - C API transaction tests including bool* out-param usage
- Existing JS binding: `bindings/js/src/read.ts` - Established optional-value pattern (has_value + typed out-param)
- Existing JS binding: `bindings/js/src/loader.ts` - FFI symbol definition pattern, free_string already declared

### Secondary (MEDIUM confidence)
- None needed -- all patterns are well-established in existing code

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All tools established in Phase 1, no new dependencies
- Architecture: HIGH - Prototype extension pattern proven in Phases 2-3, query out-param pattern same as readByIdById
- Parameter marshaling: HIGH - Three reference implementations (C API, Dart, Python) demonstrate identical pattern
- Bool out-parameter: HIGH - C API tests confirm bool* usage, Uint8Array(1) is the correct bun:ffi approach for 1-byte types
- Transaction control: HIGH - Simplest feature; direct delegation to C API with no marshaling complexity
- Pitfalls: HIGH - GC keepalive is well-known FFI pattern; bool size is verified from C standards and MSVC behavior

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (30 days -- bun:ffi API is stable, C API is stable)
