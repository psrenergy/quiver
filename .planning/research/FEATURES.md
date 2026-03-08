# Feature Landscape

**Domain:** Bun-native JS/TS FFI binding for a C API database wrapper library
**Researched:** 2026-03-08
**Overall confidence:** MEDIUM-HIGH (bun:ffi docs verified via official sources; JS/TS API patterns are well-established)

## Table Stakes

Features users expect from a JS/TS FFI database binding. Missing = product feels incomplete.

| Feature | Why Expected | Complexity | Depends On (C API) |
|---------|--------------|------------|-------------------|
| Database lifecycle (open, close, factory methods) | Cannot use library without it | Low | `quiver_database_open`, `quiver_database_from_schema`, `quiver_database_from_migrations`, `quiver_database_close` |
| Synchronous API | SQLite is inherently sync; JS database libs (better-sqlite3, bun:sqlite) are sync for perf | Low | All C API functions are sync |
| TypeScript type definitions | TS users expect full type safety; untyped FFI wrappers are a non-starter | Med | None (pure TS layer) |
| Error handling via thrown exceptions | JS convention: throw Error, not return codes | Low | `quiver_get_last_error` |
| Custom error class (`QuiverError`) | Lets callers `catch (e) { if (e instanceof QuiverError) }` | Low | None (pure TS layer) |
| Create element (CRUD - C) | Core write operation | Med | `quiver_element_create/set_*/destroy`, `quiver_database_create_element` |
| Read scalar/vector/set values (CRUD - R) | Core read operation | Med | `quiver_database_read_scalar_*`, `read_vector_*`, `read_set_*` and their `_by_id` variants |
| Update element (CRUD - U) | Core write operation | Med | `quiver_database_update_element` |
| Delete element (CRUD - D) | Core write operation | Low | `quiver_database_delete_element` |
| Query operations (plain + parameterized) | SQL queries are fundamental to any SQLite wrapper | Med | `quiver_database_query_string/integer/float` + `_params` variants |
| Transaction control (begin, commit, rollback) | Multi-operation atomicity is essential | Low | `quiver_database_begin_transaction/commit/rollback/in_transaction` |
| Explicit resource cleanup (`close()`) | Native handles must be freed; GC alone is insufficient | Low | `quiver_database_close` |
| camelCase API surface | JS/TS naming convention per CLAUDE.md cross-layer rules | Low | None (naming transformation) |
| Platform-specific DLL/SO loading | Must find and load correct shared library | Low | `bun:ffi` `dlopen` + `suffix` |
| String marshaling (JS string to/from C `char*`) | Every C API call uses strings; must handle null-termination and UTF-8 | Med | `bun:ffi` `CString`, `Buffer.from()` |
| Pointer lifecycle management | C API returns `new`-allocated arrays; JS wrapper must call matching `free_*` | Med | All `quiver_database_free_*` functions |
| Read element IDs | List what elements exist in a collection | Low | `quiver_database_read_element_ids` |

## Differentiators

Features that set the binding apart from a bare minimum. Not expected, but valued.

| Feature | Value Proposition | Complexity | Depends On (C API) |
|---------|-------------------|------------|-------------------|
| `transaction()` callback wrapper | `db.transaction((db) => { ... })` auto-commits/rollbacks; idiomatic JS pattern matching Dart/Julia/Lua bindings | Low | `begin_transaction`, `commit`, `rollback` (composed in JS) |
| Object-based `createElement` / `updateElement` | Accept `{ label: "x", value: 42, tags: ["a", "b"] }` instead of Element builder; idiomatic JS | Med | Element C API (internal) |
| `readScalarsById` / `readVectorsById` / `readSetsById` composite reads | One call returns all typed attributes for an element; matches Dart/Julia/Lua convenience methods | Med | `list_scalar_attributes` + typed reads (composed in JS; requires metadata operations) |
| `readElementById` unified read | Merges all scalars, vectors, and sets into a single `Record<string, unknown>` | Med | Composes composite reads above |
| `using` keyword support (`Symbol.dispose`) | `using db = Database.fromSchema(...)` -- auto-close on scope exit; modern TS 5.2+ pattern | Low | None (add `[Symbol.dispose](): void` to Database class) |
| Date/DateTime parsing convenience | `readScalarDateTimeById` etc. that parse ISO 8601 strings to JS `Date` objects | Low | String read + `new Date()` parsing (composed in JS) |
| Version API | `quiver.version()` for diagnostics | Low | `quiver_version` |
| `isHealthy()` / `path()` | Database inspection methods | Low | `quiver_database_is_healthy`, `quiver_database_path` |
| `describe()` | Print schema info for debugging | Low | `quiver_database_describe` |
| Metadata operations | `getScalarMetadata`, `listScalarAttributes`, etc. for schema introspection | High | `quiver_database_get_*_metadata`, `quiver_database_list_*` + struct unmarshaling |
| Time series read/update | Multi-column typed data operations | High | `quiver_database_read_time_series_group`, `quiver_database_update_time_series_group` |
| npm package with bundled native binary | `bun add quiver` just works with prebuilt DLL | Med | Build/packaging infrastructure |

## Anti-Features

Features to explicitly NOT build in v0.5.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Async/Promise-based API | SQLite is synchronous; async adds overhead and complexity with zero benefit (better-sqlite3 and bun:sqlite both proved sync is faster) | Synchronous API throughout |
| Node.js compatibility | bun:ffi is Bun-specific; Node.js would need node-ffi-napi or N-API addon -- completely different mechanism | Bun-only for v0.5; consider N-API addon as separate future milestone |
| ORM features (schema generation, migrations, model decorators) | Logic belongs in C++ layer; binding should be thin | Use existing C++ schema/migration support |
| Connection pooling | SQLite is single-writer; pooling adds complexity without benefit | Single Database instance |
| CSV import/export in binding | Out of scope per PROJECT.md | Defer to future milestone |
| Blob subsystem in binding | Out of scope per PROJECT.md | Defer to future milestone |
| Lua scripting in binding | Out of scope per PROJECT.md | Defer to future milestone |
| Struct unmarshaling for metadata | bun:ffi has no built-in struct support; manual offset calculation is fragile and error-prone | Defer metadata operations to future milestone unless a clean pattern emerges; use only pointer/scalar C API calls in v0.5 |
| Auto-generated FFI declarations | Other bindings have generators, but bun:ffi's `dlopen` signature object is simple enough to hand-write for the v0.5 scope | Hand-write FFI declarations; consider generator later if API surface grows |
| Runtime validation (zod, etc.) | Adds dependency; type safety comes from TypeScript; C API validates inputs | TypeScript types for compile-time safety; rely on C API runtime validation |
| Custom error messages in binding | CLAUDE.md principle: bindings surface C API error messages, never craft their own | Pass through `quiver_get_last_error()` messages |
| `@property` decorators or getter magic | Keep API explicit; methods not getters (matches Python binding precedent) | Regular methods: `db.path()` not `db.path` |

## Feature Dependencies

```
DLL Loading (platform detection + dlopen) --> All features

Error handling (check + QuiverError) --> All C API wrapper methods

String marshaling (Buffer.from + CString) --> All C API wrapper methods

Database lifecycle (fromSchema, fromMigrations, close) --> All database operations

Element builder (internal) --> createElement, updateElement

Transaction control (beginTransaction, commit, rollback)
  --> transaction() callback wrapper (composed convenience)

Read scalar/vector/set by ID --> readScalarsById, readVectorsById, readSetsById (composed convenience)
  --> readElementById (composed convenience)

Metadata operations (listScalarAttributes, etc.)
  --> readScalarsById (needs attribute list + types to dispatch typed reads)
  --> readVectorsById (needs group list + column types)
  --> readSetsById (needs group list + column types)

NOTE: Composite reads (readScalarsById etc.) depend on metadata operations.
      If metadata is deferred, composite reads must also be deferred.
```

## Complexity Assessment by C API Pattern

The C API functions fall into distinct patterns with varying FFI complexity in bun:ffi:

### Pattern 1: Simple pointer-in, error-code-out (Low complexity)
Functions like `quiver_database_begin_transaction(db)`, `quiver_database_close(db)`.
Just pass the opaque pointer and check the return code.

### Pattern 2: Pointer + strings in, scalar out via pointer (Medium complexity)
Functions like `quiver_database_read_scalar_integer_by_id(db, collection, attribute, id, &out_value, &out_has_value)`.
Requires allocating TypedArrays for out-parameters, passing string buffers, reading results.
**bun:ffi approach:** Use `BigInt64Array(1)` or `Float64Array(1)` as out-parameter buffers; `ptr()` converts them to pointers.

### Pattern 3: Returns allocated arrays (Medium-High complexity)
Functions like `quiver_database_read_scalar_integers(db, collection, attribute, &out_values, &out_count)`.
C allocates memory, returns pointer + count. JS must read the array then call the matching `free_*`.
**bun:ffi approach:** Use `read.ptr()` to get the pointer value from the out-parameter buffer, then `toArrayBuffer(ptr, 0, count * 8)` to read the data, then call the free function.

### Pattern 4: Nested pointer arrays (High complexity)
Functions like `quiver_database_read_vector_integers(db, col, attr, &out_vectors, &out_sizes, &out_count)`.
Returns pointer-to-pointer (array of arrays). Each inner array has its own size.
**bun:ffi approach:** Must dereference pointer-to-pointer using `read.ptr()` at offsets, then read each inner array. Error-prone.

### Pattern 5: Struct out-parameters (Very High complexity)
Functions like `quiver_database_get_scalar_metadata(db, collection, attribute, &out_metadata)`.
Requires allocating and reading C structs by manual offset calculation.
**bun:ffi approach:** No built-in struct support. Must manually calculate field offsets accounting for padding/alignment. Third-party libs (bun-cstruct, bun-ffi-structs) exist but add dependencies.

### Pattern 6: Multi-column typed data (Very High complexity)
Functions like `quiver_database_read_time_series_group` with parallel typed arrays.
Combines patterns 3 and 5: multiple arrays of different types determined by a type discriminator array.

## MVP Recommendation

**v0.5 Core (must ship):**

1. **DLL loading + platform detection** -- foundation for everything
2. **Error handling** (`QuiverError` class, `check()` helper, `quiver_get_last_error`)
3. **Database lifecycle** (`fromSchema`, `fromMigrations`, `close`, `isHealthy`, `path`, `describe`)
4. **Element builder** (internal, used by create/update)
5. **CRUD operations** (`createElement`, `updateElement`, `deleteElement`)
6. **Read scalar by ID** (integer, float, string -- Pattern 2, manageable complexity)
7. **Read vector/set by ID** (integer, float, string -- Pattern 3, allocated arrays)
8. **Read all scalars/vectors/sets** (bulk reads returning arrays/arrays-of-arrays -- Patterns 3+4)
9. **Read element IDs** (list elements in a collection)
10. **Query operations** (plain + parameterized -- string, integer, float)
11. **Transaction control** (begin, commit, rollback, inTransaction)
12. **Transaction callback wrapper** (`db.transaction(fn)`)
13. **TypeScript types** for all public API

**v0.5 Stretch (ship if clean pattern found):**

14. **Object-based createElement/updateElement** (accept plain JS objects instead of forcing Element builder)
15. **`Symbol.dispose` support** (auto-close with `using` keyword)
16. **Date/DateTime convenience methods** (parse ISO strings to JS Date)
17. **`version()`** (trivial, nice diagnostic)

**Defer to v0.6+:**

- Metadata operations (struct unmarshaling complexity)
- Composite reads (`readScalarsById` etc. -- depend on metadata)
- Time series operations (multi-column typed data complexity)
- CSV import/export
- Blob subsystem
- Lua scripting

**Rationale:** v0.5 covers all CRUD, query, and transaction operations using C API patterns 1-4 (pointer, scalar out, allocated arrays, nested arrays). Patterns 5-6 (structs, multi-column typed data) are significantly harder in bun:ffi and should be deferred. This matches the PROJECT.md scoping decision.

## TypeScript Type Patterns

The binding should export these TypeScript types:

```typescript
// Error class
export class QuiverError extends Error {
  constructor(message: string);
}

// Database options
export interface DatabaseOptions {
  readOnly?: boolean;
  consoleLevel?: LogLevel;
}

export enum LogLevel {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
  Off = 4,
}

// Main class
export class Database {
  static fromSchema(dbPath: string, schemaPath: string, options?: DatabaseOptions): Database;
  static fromMigrations(dbPath: string, migrationsPath: string, options?: DatabaseOptions): Database;

  close(): void;
  [Symbol.dispose](): void;  // enables: using db = Database.fromSchema(...)

  isHealthy(): boolean;
  path(): string;
  describe(): void;

  // CRUD
  createElement(collection: string, values: Record<string, unknown>): number;
  updateElement(collection: string, id: number, values: Record<string, unknown>): void;
  deleteElement(collection: string, id: number): void;

  // Read scalar by ID
  readScalarIntegerById(collection: string, attribute: string, id: number): number | null;
  readScalarFloatById(collection: string, attribute: string, id: number): number | null;
  readScalarStringById(collection: string, attribute: string, id: number): string | null;

  // Read all scalars
  readScalarIntegers(collection: string, attribute: string): number[];
  readScalarFloats(collection: string, attribute: string): number[];
  readScalarStrings(collection: string, attribute: string): string[];

  // Read vector by ID
  readVectorIntegersById(collection: string, attribute: string, id: number): number[];
  readVectorFloatsById(collection: string, attribute: string, id: number): number[];
  readVectorStringsById(collection: string, attribute: string, id: number): string[];

  // Read all vectors
  readVectorIntegers(collection: string, attribute: string): number[][];
  readVectorFloats(collection: string, attribute: string): number[][];
  readVectorStrings(collection: string, attribute: string): string[][];

  // Read set by ID
  readSetIntegersById(collection: string, attribute: string, id: number): number[];
  readSetFloatsById(collection: string, attribute: string, id: number): number[];
  readSetStringsById(collection: string, attribute: string, id: number): string[];

  // Read all sets
  readSetIntegers(collection: string, attribute: string): number[][];
  readSetFloats(collection: string, attribute: string): number[][];
  readSetStrings(collection: string, attribute: string): string[][];

  // Read element IDs
  readElementIds(collection: string): number[];

  // Queries
  queryString(sql: string): string | null;
  queryInteger(sql: string): number | null;
  queryFloat(sql: string): number | null;
  queryStringParams(sql: string, params: QueryParam[]): string | null;
  queryIntegerParams(sql: string, params: QueryParam[]): number | null;
  queryFloatParams(sql: string, params: QueryParam[]): number | null;

  // Transactions
  beginTransaction(): void;
  commit(): void;
  rollback(): void;
  inTransaction(): boolean;
  transaction<T>(fn: (db: Database) => T): T;
}

// Query parameter type (matches C API quiver_data_type_t)
export type QueryParam = number | string | null;

// Version
export function version(): string;
```

**Key TypeScript decisions:**
- Use `number` for both integers and floats (JS has no integer type; BigInt only if user needs >2^53)
- Use `null` for "not found" (idiomatic JS/TS)
- Use `Record<string, unknown>` for element values (flexible, matches Python kwargs pattern)
- Return arrays as native JS arrays, not TypedArrays (better DX even if slightly slower)
- `QueryParam` is a union type -- simpler than the Dart/Julia parallel-arrays approach
- Static factory methods (`Database.fromSchema`) not constructors (matches existing binding patterns)

## bun:ffi Implementation Patterns

### String Passing (JS to C)
```typescript
// Encode string + null terminator to Buffer
const buf = Buffer.from(str + "\0");
// Pass ptr(buf) to C function
```

### String Reading (C to JS)
```typescript
import { CString, read } from "bun:ffi";
// For a returned char*:
const ptr = read.ptr(outBuf, 0);  // read pointer from out-parameter buffer
const str = new CString(ptr);     // reads null-terminated UTF-8 string
```

### Out-parameter Pattern
```typescript
// For int64_t* out_value:
const outValue = new BigInt64Array(1);
// For int* out_has_value:
const outHas = new Int32Array(1);
// Call the function with ptr(outValue), ptr(outHas)
// Read results: outValue[0], outHas[0]
```

### Allocated Array Pattern
```typescript
// For int64_t** out_values, size_t* out_count:
const outPtr = new BigUint64Array(1);  // holds the pointer value
const outCount = new BigUint64Array(1);
// Call with ptr(outPtr), ptr(outCount)
// Read: const dataPtr = Number(outPtr[0]);
//        const count = Number(outCount[0]);
//        const buf = toArrayBuffer(dataPtr, 0, count * 8);
//        const result = Array.from(new BigInt64Array(buf));
// Free: lib.symbols.quiver_database_free_integer_array(dataPtr);
```

### Library Loading
```typescript
import { dlopen, suffix, FFIType } from "bun:ffi";

const libPath = `path/to/libquiver_c.${suffix}`;
const lib = dlopen(libPath, {
  quiver_database_from_schema: {
    args: [FFIType.cstring, FFIType.cstring, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  // ... more function declarations
});
```

## Sources

- [Bun FFI Documentation](https://bun.com/docs/runtime/ffi)
- [bun:ffi API Reference](https://bun.com/reference/bun/ffi)
- [bun:ffi CString](https://bun.com/reference/bun/ffi/CString)
- [bun:ffi read](https://bun.com/reference/bun/ffi/read)
- [bun:ffi struct support issue #6139](https://github.com/oven-sh/bun/issues/6139)
- [bun:ffi string passing issue #6709](https://github.com/oven-sh/bun/issues/6709)
- [TypeScript 5.2 Disposable pattern](https://www.typescriptlang.org/docs/handbook/release-notes/typescript-5-2.html)
- [better-sqlite3 API design](https://github.com/WiseLibs/better-sqlite3)
- [bun:sqlite documentation](https://bun.com/docs/runtime/sqlite)
- [Bun C Compiler plugin](https://bun.sh/docs/api/cc)
