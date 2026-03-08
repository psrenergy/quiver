# Architecture Patterns

**Domain:** Bun JS/TS binding for existing C API (libquiver_c)
**Researched:** 2026-03-08

## Recommended Architecture

The JS/TS binding follows the same layered FFI pattern as Julia, Dart, and Python bindings: thin wrapper over the C API using `bun:ffi`. No logic in the binding layer. All intelligence stays in the C++ core.

```
libquiver.dll/.so/.dylib     (C++ core)
       |
libquiver_c.dll/.so/.dylib   (C API - opaque handles, binary error codes)
       |
bun:ffi dlopen()             (Bun runtime FFI - loads shared library)
       |
bindings/js/src/ffi.ts       (Symbol declarations + library loader)
       |
bindings/js/src/database.ts  (Database class - thin wrapper)
       |
bindings/js/src/index.ts     (Public exports)
```

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| `src/ffi.ts` | Load shared libraries, declare all C function symbols, export `lib` handle | bun:ffi, filesystem |
| `src/errors.ts` | `check()` helper, `DatabaseError` class, reads `quiver_get_last_error` | ffi.ts |
| `src/memory.ts` | Typed memory allocation helpers (`allocPtr`, `allocI64`, `allocF64`, `readPtr`, `readI64`, `readF64`, `readCString`) | bun:ffi primitives |
| `src/element.ts` | `Element` builder class wrapping `quiver_element_t` opaque handle | ffi.ts, errors.ts, memory.ts |
| `src/database.ts` | `Database` class - lifecycle, CRUD, queries, transactions | ffi.ts, errors.ts, memory.ts, element.ts |
| `src/index.ts` | Public barrel export | database.ts, element.ts, errors.ts |

### Data Flow

**Opening a database:**
```
User calls Database.fromSchema(dbPath, schemaPath)
  -> Encode strings to Buffer (null-terminated UTF-8)
  -> Allocate out-parameter buffer (pointer-sized)
  -> Call lib.symbols.quiver_database_from_schema(...)
  -> check() reads return code, throws on error
  -> read.ptr() extracts the opaque handle from out-parameter
  -> Store handle as number (pointer) in Database instance
```

**Reading scalar integers:**
```
User calls db.readScalarIntegers(collection, attribute)
  -> Encode strings, allocate out-parameter buffers (ptr + size_t)
  -> Call lib.symbols.quiver_database_read_scalar_integers(...)
  -> check() validates return code
  -> Read pointer and count from out-parameter buffers
  -> toArrayBuffer(pointer, 0, count * 8) to get Int64Array view
  -> Copy values to JS array
  -> Call lib.symbols.quiver_database_free_integer_array(pointer) to free C memory
  -> Return JS array
```

## Patterns to Follow

### Pattern 1: Library Loading with Dependency Pre-loading

On Windows, `libquiver_c.dll` depends on `libquiver.dll`. The C API DLL must find its dependency. All existing bindings handle this by pre-loading the core library first.

**What:** Load `libquiver.dll` before `libquiver_c.dll` using `dlopen`.
**When:** Always, on all platforms (harmless no-op if not needed).

```typescript
// src/ffi.ts
import { dlopen, suffix, FFIType, ptr, read, CString, toArrayBuffer } from "bun:ffi";
import { resolve, dirname } from "path";

const LIBRARY_NAME = `libquiver_c.${suffix}`;
const CORE_LIBRARY_NAME = `libquiver.${suffix}`;

function loadLibrary() {
  // Strategy 1: Check for libraries adjacent to package (bundled)
  // Strategy 2: Fall back to system PATH

  // Pre-load core library (Windows dependency chain)
  try {
    dlopen(CORE_LIBRARY_NAME, {});
  } catch {
    // Acceptable - may not be needed or may be found via PATH
  }

  return dlopen(LIBRARY_NAME, symbols);
}
```

**Confidence:** HIGH -- this pattern is proven across all three existing bindings (Julia, Dart, Python).

### Pattern 2: Out-Parameter Marshaling via TypedArray + read

bun:ffi does not have a managed memory allocator like Dart's `Arena`. Instead, use `TypedArray` buffers to create memory for out-parameters, then use `read.*` or `DataView` to extract values.

**What:** Allocate fixed-size TypedArray buffers for each out-parameter, pass their pointer to C, then read values back.
**When:** Every C API call with out-parameters (which is nearly all of them).

```typescript
// For a C function: quiver_error_t func(quiver_database_t* db, int64_t* out_value, int* out_has_value)
//
// out_value needs 8 bytes (int64_t), out_has_value needs 4 bytes (int32_t)

function readScalarIntegerById(collection: string, attribute: string, id: number): number | null {
  const outValue = new BigInt64Array(1);      // 8 bytes for int64_t
  const outHasValue = new Int32Array(1);      // 4 bytes for int

  const err = lib.symbols.quiver_database_read_scalar_integer_by_id(
    this._ptr,
    Buffer.from(collection + "\0"),
    Buffer.from(attribute + "\0"),
    id,
    outValue,                                  // bun:ffi converts TypedArray to pointer
    outHasValue,
  );
  check(err);

  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
}
```

**Key insight:** When a C function expects `int64_t*`, pass a `BigInt64Array(1)`. When it expects `double*`, pass a `Float64Array(1)`. When it expects `int*`, pass an `Int32Array(1)`. bun:ffi automatically converts `TypedArray` arguments in pointer positions to their underlying buffer pointers.

**Confidence:** HIGH -- documented bun:ffi behavior.

### Pattern 3: Pointer-to-Pointer Out-Parameters

Many C API functions return opaque handles through `quiver_database_t** out_db` (pointer to pointer). bun:ffi cannot directly allocate a "pointer slot" -- you need a buffer large enough for a pointer (8 bytes on 64-bit).

**What:** Use a typed array sized for a pointer, then use `read.ptr()` to extract the pointer value.
**When:** Factory functions (`from_schema`, `from_migrations`, `open`), `quiver_element_create`.

```typescript
// For: quiver_error_t quiver_database_from_schema(..., quiver_database_t** out_db)
//
// out_db is a pointer-to-pointer: we need a buffer that holds one pointer value

const outDbBuf = new BigInt64Array(1);  // 8 bytes to hold a pointer on 64-bit
const err = lib.symbols.quiver_database_from_schema(
  Buffer.from(dbPath + "\0"),
  Buffer.from(schemaPath + "\0"),
  optionsBuf,         // pointer to options struct
  outDbBuf,           // pointer-to-pointer: will receive the db handle
);
check(err);

// Extract the pointer value
const dbPtr = read.ptr(ptr(outDbBuf), 0);  // read pointer at offset 0
```

**Alternative approach** using a raw pointer buffer:

```typescript
const outDbBuf = new Uint8Array(8);  // pointer-sized buffer
// ... pass outDbBuf to function ...
const view = new DataView(outDbBuf.buffer);
const dbPtr = Number(view.getBigUint64(0, true));  // little-endian on x86
```

**Confidence:** MEDIUM -- the TypedArray-to-pointer conversion is well-documented, but the pointer-to-pointer pattern specifically requires testing to confirm `read.ptr` works correctly on the resulting buffer. This is the most critical pattern to validate early.

### Pattern 4: String Encoding for C API

All C API functions accept `const char*` (null-terminated UTF-8). JavaScript strings must be converted.

**What:** Use `Buffer.from(str + "\0")` to create null-terminated UTF-8 buffers.
**When:** Every string argument passed to C API.

```typescript
// Encoding strings for C
const cStr = Buffer.from(myString + "\0");

// Reading C strings (returned as ptr)
// Use CString from bun:ffi
import { CString } from "bun:ffi";
const jsString = new CString(somePtr);  // reads until \0, transcodes to JS string
```

**Confidence:** HIGH -- CString is a documented bun:ffi export; Buffer.from produces UTF-8 by default.

### Pattern 5: Error Checking (check pattern)

Mirrors the Dart/Python `check()` helper exactly.

**What:** After every C API call, check the return code and throw if non-zero, reading the error message from `quiver_get_last_error`.
**When:** Every C API call that returns `quiver_error_t`.

```typescript
// src/errors.ts
import { CString } from "bun:ffi";

export class DatabaseError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "DatabaseError";
  }
}

export function check(err: number, lib: LibSymbols): void {
  if (err !== 0) {
    const msgPtr = lib.quiver_get_last_error();
    const message = msgPtr ? new CString(msgPtr).toString() : "Unknown error";
    throw new DatabaseError(message);
  }
}
```

**Confidence:** HIGH -- direct translation of existing binding pattern.

### Pattern 6: Struct Layout via ArrayBuffer + DataView

The C API uses small structs like `quiver_database_options_t` (contains `int read_only` + `int console_level`). bun:ffi has no struct support -- you must manually lay out memory.

**What:** Use ArrayBuffer + DataView to construct C struct layouts with correct field offsets and alignment.
**When:** `quiver_database_options_t` for factory methods.

```typescript
// quiver_database_options_t { int read_only; quiver_log_level_t console_level; }
// Both fields are int (4 bytes), total struct size: 8 bytes

function createDefaultOptions(): Uint8Array {
  const buf = new Uint8Array(8);
  const view = new DataView(buf.buffer);
  view.setInt32(0, 0, true);  // read_only = 0 (false), little-endian
  view.setInt32(4, 4, true);  // console_level = QUIVER_LOG_OFF (4), little-endian
  return buf;
}
```

**Alternative:** Call `quiver_database_options_default()` which returns the struct by value. However, bun:ffi cannot handle struct returns (only scalar returns). So the binding must construct the struct manually or we need to evaluate whether `quiver_database_options_default` can be called and the returned value captured.

**Important:** `quiver_database_options_default()` returns a struct by value. bun:ffi does NOT support struct return values. The binding must manually construct the options struct in JavaScript with the known default values (`read_only = 0`, `console_level = QUIVER_LOG_OFF = 4`).

**Confidence:** HIGH for the manual layout approach. The C struct layout is stable and simple (two int32 fields).

### Pattern 7: Memory Cleanup After Reads

C API allocates memory for returned arrays/strings. The binding must free them using the matching `quiver_database_free_*` function.

**What:** After reading data from C-allocated memory, immediately copy to JS values, then call the matching free function.
**When:** Every read operation that returns allocated memory.

```typescript
function readScalarIntegers(collection: string, attribute: string): number[] {
  const outValues = new BigInt64Array(1);  // ptr to int64_t*
  const outCount = new BigInt64Array(1);   // size_t (8 bytes on 64-bit)

  const err = lib.symbols.quiver_database_read_scalar_integers(
    this._ptr,
    Buffer.from(collection + "\0"),
    Buffer.from(attribute + "\0"),
    outValues,
    outCount,
  );
  check(err);

  const valuesPtr = read.ptr(ptr(outValues), 0);
  const count = Number(read.u64(ptr(outCount), 0));  // size_t

  if (count === 0 || valuesPtr === 0) return [];

  // Copy C data to JS array
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const int64View = new BigInt64Array(buffer);
  const result = Array.from(int64View, (v) => Number(v));

  // Free C-allocated memory
  lib.symbols.quiver_database_free_integer_array(valuesPtr);

  return result;
}
```

**Confidence:** MEDIUM -- the `toArrayBuffer` + `read.ptr` combination needs validation, but the pattern logic is proven across all bindings.

### Pattern 8: Naming Convention (snake_case C -> camelCase JS)

Following the project's cross-layer conventions from CLAUDE.md.

**What:** Mechanically convert C++ `snake_case` method names to JavaScript `camelCase`.
**When:** All public API methods.

| C++ | C API | JS/TS |
|-----|-------|-------|
| `from_schema` | `quiver_database_from_schema` | `Database.fromSchema()` |
| `create_element` | `quiver_database_create_element` | `db.createElement()` |
| `read_scalar_integers` | `quiver_database_read_scalar_integers` | `db.readScalarIntegers()` |
| `begin_transaction` | `quiver_database_begin_transaction` | `db.beginTransaction()` |
| `query_string` | `quiver_database_query_string` | `db.queryString()` |
| `delete_element` | `quiver_database_delete_element` | `db.deleteElement()` |
| `read_scalar_integer_by_id` | `quiver_database_read_scalar_integer_by_id` | `db.readScalarIntegerById()` |

**Confidence:** HIGH -- mirrors existing Dart naming exactly.

## Recommended File Structure

```
bindings/js/
  package.json              # npm package config (name: "quiverdb")
  tsconfig.json             # TypeScript configuration (target ESNext)
  src/
    index.ts                # Public barrel export
    ffi.ts                  # dlopen, symbol declarations, library loading
    errors.ts               # DatabaseError, check() helper
    memory.ts               # TypedArray allocation helpers, struct builders
    element.ts              # Element builder class (wraps quiver_element_t)
    database.ts             # Database class (wraps quiver_database_t)
  test/
    test.bat                # Windows test runner (matches other bindings)
    test.sh                 # Unix test runner
    database_lifecycle.test.ts
    database_create.test.ts
    database_read.test.ts
    database_update.test.ts
    database_delete.test.ts
    database_query.test.ts
    database_transaction.test.ts
  generator/
    generator.bat           # (future) auto-generate ffi declarations
```

### File Responsibilities

**`src/ffi.ts`** -- The heaviest file. Contains:
- Library path resolution and loading
- All `dlopen` symbol declarations mapping C function names to FFI type signatures
- Exported `lib` object with `.symbols` access
- The `suffix` constant for platform-specific library extensions

**`src/memory.ts`** -- Helpers for allocating and reading out-parameter buffers:
- `encodeString(str)` -- returns `Buffer.from(str + "\0")`
- `allocPtr()` -- returns `new BigInt64Array(1)` (for pointer out-params)
- `allocI64()` -- returns `new BigInt64Array(1)`
- `allocF64()` -- returns `new Float64Array(1)`
- `allocI32()` -- returns `new Int32Array(1)`
- `readPtrFromBuf(buf)` -- reads a pointer from a TypedArray
- `createDefaultOptions()` -- builds `quiver_database_options_t` struct

**`src/errors.ts`** -- Error checking:
- `DatabaseError` extends `Error`
- `check(err)` -- checks return code, reads `quiver_get_last_error`, throws

**`src/element.ts`** -- Element builder:
- `Element` class wrapping opaque `quiver_element_t*`
- `.set(name, value)` polymorphic setter (mirrors Dart's Element)
- `.setInteger()`, `.setFloat()`, `.setString()`, `.setNull()`
- `.setArrayInteger()`, `.setArrayFloat()`, `.setArrayString()`
- `.dispose()` -- frees native memory
- Internal `_ptr` property (number -- the opaque handle)

**`src/database.ts`** -- Database class:
- Static factory methods: `fromSchema()`, `fromMigrations()`
- Instance methods: all CRUD, query, transaction operations
- `close()` -- closes database, marks as closed
- `using` support via `Symbol.dispose` for `using db = Database.fromSchema(...)`

**`src/index.ts`** -- Public API:
```typescript
export { Database } from "./database";
export { Element } from "./element";
export { DatabaseError } from "./errors";
```

## bun:ffi Symbol Declaration Map

The core of the FFI layer is the symbol declaration object passed to `dlopen`. Each C function needs its argument types and return type specified.

### FFI Type Mappings for Quiver C API

| C Type | bun:ffi Type | Notes |
|--------|-------------|-------|
| `quiver_error_t` (return) | `"i32"` | Enum returns as int32 |
| `quiver_database_t*` | `"ptr"` | Opaque pointer handle |
| `quiver_element_t*` | `"ptr"` | Opaque pointer handle |
| `const char*` | `"ptr"` | Pass Buffer.from(str + "\0") |
| `char**` (out) | `"ptr"` | Pass allocPtr() buffer |
| `int64_t*` (out) | `"ptr"` | Pass BigInt64Array(1) |
| `double*` (out) | `"ptr"` | Pass Float64Array(1) |
| `int*` / `bool*` (out) | `"ptr"` | Pass Int32Array(1) |
| `size_t*` (out) | `"ptr"` | Pass BigUint64Array(1) - 8 bytes on 64-bit |
| `int64_t` (value) | `"i64"` | Returns BigInt, convert with Number() |
| `double` (value) | `"f64"` | Direct number |
| `size_t` (value) | `"u64"` | Platform-dependent, 8 bytes on 64-bit |
| `const char*` (return) | `"ptr"` | Use CString to decode |
| `void` (return) | `"void"` | For quiver_clear_last_error |
| `quiver_database_options_t` (struct by value) | N/A | Cannot use dlopen for struct returns; construct manually |
| `quiver_database_options_t*` (in-param) | `"ptr"` | Pass TypedArray with manual struct layout |

### Representative Symbol Declarations

```typescript
const symbols = {
  // Utility
  quiver_version:         { args: [],       returns: "ptr" },    // const char*
  quiver_get_last_error:  { args: [],       returns: "ptr" },    // const char*
  quiver_clear_last_error:{ args: [],       returns: "void" },

  // Lifecycle
  quiver_database_from_schema: {
    args: ["ptr", "ptr", "ptr", "ptr"],  // db_path, schema_path, options*, out_db**
    returns: "i32",                       // quiver_error_t
  },
  quiver_database_from_migrations: {
    args: ["ptr", "ptr", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_open: {
    args: ["ptr", "ptr", "ptr"],         // path, options*, out_db**
    returns: "i32",
  },
  quiver_database_close: {
    args: ["ptr"],                        // db*
    returns: "i32",
  },
  quiver_database_is_healthy: {
    args: ["ptr", "ptr"],                 // db*, out_healthy*
    returns: "i32",
  },
  quiver_database_path: {
    args: ["ptr", "ptr"],                 // db*, out_path**
    returns: "i32",
  },

  // Transaction
  quiver_database_begin_transaction: { args: ["ptr"], returns: "i32" },
  quiver_database_commit:            { args: ["ptr"], returns: "i32" },
  quiver_database_rollback:          { args: ["ptr"], returns: "i32" },
  quiver_database_in_transaction:    { args: ["ptr", "ptr"], returns: "i32" },

  // Element
  quiver_element_create:       { args: ["ptr"],                   returns: "i32" },
  quiver_element_destroy:      { args: ["ptr"],                   returns: "i32" },
  quiver_element_clear:        { args: ["ptr"],                   returns: "i32" },
  quiver_element_set_integer:  { args: ["ptr", "ptr", "i64"],     returns: "i32" },
  quiver_element_set_float:    { args: ["ptr", "ptr", "f64"],     returns: "i32" },
  quiver_element_set_string:   { args: ["ptr", "ptr", "ptr"],     returns: "i32" },
  quiver_element_set_null:     { args: ["ptr", "ptr"],            returns: "i32" },
  quiver_element_set_array_integer: { args: ["ptr", "ptr", "ptr", "i32"], returns: "i32" },
  quiver_element_set_array_float:   { args: ["ptr", "ptr", "ptr", "i32"], returns: "i32" },
  quiver_element_set_array_string:  { args: ["ptr", "ptr", "ptr", "i32"], returns: "i32" },

  // CRUD
  quiver_database_create_element: {
    args: ["ptr", "ptr", "ptr", "ptr"],  // db*, collection, element*, out_id*
    returns: "i32",
  },
  quiver_database_update_element: {
    args: ["ptr", "ptr", "i64", "ptr"],  // db*, collection, id, element*
    returns: "i32",
  },
  quiver_database_delete_element: {
    args: ["ptr", "ptr", "i64"],         // db*, collection, id
    returns: "i32",
  },

  // Read scalar
  quiver_database_read_scalar_integers: {
    args: ["ptr", "ptr", "ptr", "ptr", "ptr"],  // db, collection, attribute, out_values**, out_count*
    returns: "i32",
  },
  quiver_database_read_scalar_floats: {
    args: ["ptr", "ptr", "ptr", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_read_scalar_strings: {
    args: ["ptr", "ptr", "ptr", "ptr", "ptr"],
    returns: "i32",
  },

  // Read scalar by ID
  quiver_database_read_scalar_integer_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],  // db, coll, attr, id, out_value*, out_has*
    returns: "i32",
  },
  quiver_database_read_scalar_float_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_read_scalar_string_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },

  // Read vector by ID
  quiver_database_read_vector_integers_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_read_vector_floats_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_read_vector_strings_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },

  // Read set by ID
  quiver_database_read_set_integers_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_read_set_floats_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_read_set_strings_by_id: {
    args: ["ptr", "ptr", "ptr", "i64", "ptr", "ptr"],
    returns: "i32",
  },

  // Read element IDs
  quiver_database_read_element_ids: {
    args: ["ptr", "ptr", "ptr", "ptr"],
    returns: "i32",
  },

  // Query
  quiver_database_query_string: {
    args: ["ptr", "ptr", "ptr", "ptr"],  // db, sql, out_value**, out_has*
    returns: "i32",
  },
  quiver_database_query_integer: {
    args: ["ptr", "ptr", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_query_float: {
    args: ["ptr", "ptr", "ptr", "ptr"],
    returns: "i32",
  },

  // Parameterized query
  quiver_database_query_string_params: {
    args: ["ptr", "ptr", "ptr", "ptr", "u64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_query_integer_params: {
    args: ["ptr", "ptr", "ptr", "ptr", "u64", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_query_float_params: {
    args: ["ptr", "ptr", "ptr", "ptr", "u64", "ptr", "ptr"],
    returns: "i32",
  },

  // Free functions
  quiver_database_free_integer_array: { args: ["ptr"], returns: "i32" },
  quiver_database_free_float_array:   { args: ["ptr"], returns: "i32" },
  quiver_database_free_string_array:  { args: ["ptr", "u64"], returns: "i32" },
  quiver_database_free_string:        { args: ["ptr"], returns: "i32" },
  quiver_database_free_integer_vectors: { args: ["ptr", "ptr", "u64"], returns: "i32" },
  quiver_database_free_float_vectors:   { args: ["ptr", "ptr", "u64"], returns: "i32" },
  quiver_database_free_string_vectors:  { args: ["ptr", "ptr", "u64"], returns: "i32" },

  // Schema inspection
  quiver_database_describe: { args: ["ptr"], returns: "i32" },
} as const;
```

## Integration Points with Existing C API

### No C API Changes Required

The JS/TS binding uses the existing `libquiver_c` shared library as-is. Zero modifications to the C or C++ layers.

**One exception:** `quiver_database_options_default()` returns a struct by value, which bun:ffi cannot handle. The binding must hardcode the default values:
- `read_only = 0`
- `console_level = QUIVER_LOG_OFF (4)`

This is acceptable because the defaults are stable and documented in the C header.

### Shared Test Schemas

All test schemas live in `tests/schemas/valid/` and `tests/schemas/invalid/`. The JS/TS tests reference these same files using relative paths, identical to how Julia, Dart, and Python tests work.

### Shared Library Discovery

The binding needs `libquiver_c.dll` (and `libquiver.dll` on Windows) available at runtime. Two strategies:

1. **Development:** Libraries in `build/bin/` directory. Test script adds this to PATH.
2. **Distribution (npm):** Bundle prebuilt binaries alongside the package, or document that users must have the libraries in their PATH.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Using struct returns from C API
**What:** Calling `quiver_database_options_default()` or `quiver_csv_options_default()` via FFI.
**Why bad:** bun:ffi cannot return structs by value. The call will silently produce garbage or crash.
**Instead:** Manually construct the struct in JavaScript using DataView with the correct field layout.

### Anti-Pattern 2: Forgetting to free C-allocated memory
**What:** Reading arrays/strings from C API and not calling the matching `free_*` function.
**Why bad:** Memory leak. The C API allocates with `new[]` -- only the matching free function can deallocate.
**Instead:** Always call `quiver_database_free_*` after copying data to JS. Use try/finally to ensure cleanup.

### Anti-Pattern 3: Passing JS strings directly to C functions
**What:** Passing a JavaScript string where the C API expects `const char*`.
**Why bad:** bun:ffi will not automatically convert JS strings to C strings for pointer-typed parameters.
**Instead:** Always use `Buffer.from(str + "\0")` to create null-terminated UTF-8 buffers.

### Anti-Pattern 4: Using Number for 64-bit pointer arithmetic
**What:** Performing arithmetic on pointer values stored as JavaScript numbers.
**Why bad:** JavaScript numbers lose precision above 2^53. While most pointers fit in 53 bits, the combination of pointer arithmetic and large addresses can overflow.
**Instead:** Use `read.ptr()` for reading pointers, and avoid manual pointer arithmetic. Let bun:ffi handle conversions.

### Anti-Pattern 5: Adding logic to the binding layer
**What:** Implementing validation, query building, or data transformation in JS.
**Why bad:** Violates the project's "intelligence lives in C++" principle. Creates divergent behavior across bindings.
**Instead:** Keep the binding as a mechanical translation layer. If new logic is needed, add it to the C++ core first, expose through the C API, then wrap in all bindings.

## Suggested Build Order

This ordering minimizes risk by validating the most uncertain pattern (pointer-to-pointer out-parameters) first.

### Phase 1: FFI Foundation + Lifecycle
1. `src/ffi.ts` -- Library loading, minimal symbol declarations (version, last_error, from_schema, close)
2. `src/errors.ts` -- `check()` helper
3. `src/memory.ts` -- Core allocation helpers
4. `src/database.ts` -- `Database.fromSchema()` and `close()` only
5. First test: open a database, close it

This phase validates the critical pointer-to-pointer pattern and library loading.

### Phase 2: Element Builder + Create
1. Add Element symbols to `ffi.ts`
2. `src/element.ts` -- Element class with setters
3. `database.ts` -- `createElement()`
4. Tests: create elements with various types

### Phase 3: Read Operations
1. Add read symbols to `ffi.ts`
2. `database.ts` -- scalar reads (integers, floats, strings), by-ID reads
3. Tests: round-trip create + read

### Phase 4: Update + Delete
1. Add update/delete symbols to `ffi.ts`
2. `database.ts` -- `updateElement()`, `deleteElement()`
3. Tests: full CRUD cycle

### Phase 5: Queries + Transactions
1. Add query/transaction symbols to `ffi.ts`
2. `database.ts` -- query methods (plain + parameterized), transaction control
3. `transaction()` convenience wrapper
4. Tests: queries, transaction commit/rollback

### Phase 6: Package + Polish
1. `src/index.ts` -- public exports
2. `package.json` -- npm package metadata
3. `tsconfig.json` -- TypeScript configuration
4. Test scripts (`test.bat`, `test.sh`)
5. Full test suite pass

## Key Technical Decisions

### TypedArray for out-parameters (not manual pointer allocation)
bun:ffi converts TypedArray arguments to pointers automatically. This is the safest pattern -- the GC manages the memory, and alignment is correct by construction.

### Buffer.from for strings (not TextEncoder)
`Buffer.from(str + "\0")` produces null-terminated UTF-8 directly. Simpler than using `TextEncoder` + manual null byte append.

### Number for opaque handles (not BigInt)
bun:ffi represents pointers as JavaScript `number` (53-bit precision). This is sufficient for virtual addresses on 64-bit systems (52-bit addressable space). Using `number` avoids BigInt overhead and matches bun:ffi's `read.ptr()` return type.

### `using` support via Symbol.dispose
Bun supports TC39 explicit resource management (`using` declarations). The Database class should implement `Symbol.dispose` as an alias for `close()`:
```typescript
class Database {
  [Symbol.dispose]() { this.close(); }
}
// Usage: using db = Database.fromSchema(path, schema);
```

### bun:test for testing
Bun has a built-in test runner (`bun test`) that supports TypeScript natively. No need for Jest or Vitest.

## Sources

- [bun:ffi Documentation](https://bun.sh/docs/runtime/ffi) -- Official FFI docs (HIGH confidence)
- [bun:ffi API Reference](https://bun.com/reference/bun/ffi) -- Type reference (HIGH confidence)
- [bun:ffi CString Reference](https://bun.com/reference/bun/ffi/CString) -- String handling (HIGH confidence)
- [bun:ffi read Reference](https://bun.com/reference/bun/ffi/read) -- Pointer reading functions (HIGH confidence)
- Existing Dart binding (`bindings/dart/`) -- Proven FFI patterns (HIGH confidence)
- Existing Python binding (`bindings/python/`) -- Proven loading patterns (HIGH confidence)
- Project CLAUDE.md -- Cross-layer naming conventions, architecture principles (HIGH confidence)
