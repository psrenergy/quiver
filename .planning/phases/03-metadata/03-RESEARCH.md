# Phase 3: Metadata - Research

**Researched:** 2026-03-09
**Domain:** JS binding for C API metadata structs via bun:ffi
**Confidence:** HIGH

## Summary

Phase 3 adds metadata inspection methods to the JS binding. The C API already exposes 8 metadata functions (4 get + 4 list) with corresponding free functions. The JS binding needs to: (1) declare FFI symbols for these functions, (2) define TypeScript types for ScalarMetadata and GroupMetadata, (3) implement struct deserialization from raw memory using bun:ffi read primitives, (4) add methods to Database via module augmentation (same pattern as read.ts).

The primary challenge is reading C struct fields from raw memory. The `quiver_scalar_metadata_t` struct is 56 bytes with mixed pointer/int fields and padding gaps. The `quiver_group_metadata_t` struct is 32 bytes and contains a pointer to an array of scalar metadata structs. All struct layouts have been verified by compiling against the project's toolchain.

**Primary recommendation:** Create a single `metadata.ts` file following the read.ts pattern, with struct-reading helper functions for `quiver_scalar_metadata_t` (56 bytes) and `quiver_group_metadata_t` (32 bytes). Use `collections.sql` schema for tests since it has all four attribute types (scalar, vector, set, time series).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| JSMETA-01 | User can get scalar/vector/set/time-series metadata | 4 C API get functions identified with exact signatures; struct layouts verified; deserialization pattern documented |
| JSMETA-02 | User can list scalar attributes and vector/set/time-series groups | 4 C API list functions identified; array-of-structs pattern documented with free functions |
</phase_requirements>

## Architecture Patterns

### Recommended File Structure
```
bindings/js/src/
  metadata.ts          # NEW: all 8 metadata methods + struct reading helpers
bindings/js/test/
  metadata.test.ts     # NEW: tests for get/list metadata
```

### Pattern: Module Augmentation for Database Methods

The JS binding extends the Database class via `declare module` + prototype assignment. This is the established pattern in create.ts, read.ts, query.ts, transaction.ts.

```typescript
// metadata.ts
import { CString, ptr, read, toArrayBuffer } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { allocPointerOut, readPointerOut, readPtrAt, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

declare module "./database" {
  interface Database {
    getScalarMetadata(collection: string, attribute: string): ScalarMetadata;
    getVectorMetadata(collection: string, groupName: string): GroupMetadata;
    getSetMetadata(collection: string, groupName: string): GroupMetadata;
    getTimeSeriesMetadata(collection: string, groupName: string): GroupMetadata;
    listScalarAttributes(collection: string): ScalarMetadata[];
    listVectorGroups(collection: string): GroupMetadata[];
    listSetGroups(collection: string): GroupMetadata[];
    listTimeSeriesGroups(collection: string): GroupMetadata[];
  }
}
```

### Pattern: C Struct Deserialization via bun:ffi

The core challenge. bun:ffi provides `read.ptr()`, `read.i32()`, `read.u64()` for reading raw memory at byte offsets. Pointers are 8 bytes on x64, ints are 4 bytes, enums are 4 bytes.

### Verified Struct Layout: quiver_scalar_metadata_t (56 bytes)

```
Offset  Size  Field                    Type           bun:ffi read
------  ----  -----                    ----           -----------
 0       8    name                     const char*    read.ptr() -> CString
 8       4    data_type                enum (int)     read.i32()
12       4    not_null                 int            read.i32()
16       4    primary_key              int            read.i32()
20       4    (padding)                -              -
24       8    default_value            const char*    read.ptr() (nullable)
32       4    is_foreign_key           int            read.i32()
36       4    (padding)                -              -
40       8    references_collection    const char*    read.ptr() (nullable)
48       8    references_column        const char*    read.ptr() (nullable)
```

### Verified Struct Layout: quiver_group_metadata_t (32 bytes)

```
Offset  Size  Field                Type                          bun:ffi read
------  ----  -----                ----                          -----------
 0       8    group_name           const char*                   read.ptr() -> CString
 8       8    dimension_column     const char*                   read.ptr() (nullable)
16       8    value_columns        quiver_scalar_metadata_t*     read.ptr() -> array of 56-byte structs
24       8    value_column_count   size_t                        read.u64() -> Number()
```

### Pattern: Allocating Out-Parameter Structs for Single Metadata Get

For `get_scalar_metadata`, the C API writes into a caller-allocated struct. In bun:ffi, allocate a buffer of the struct's size and pass it as a pointer:

```typescript
// Allocate 56 bytes for quiver_scalar_metadata_t
const outMetadata = new Uint8Array(56);
check(lib.quiver_database_get_scalar_metadata(
  handle, toCString(collection), toCString(attribute), ptr(outMetadata)
));
const result = readScalarMetadata(ptr(outMetadata), 0);
lib.quiver_database_free_scalar_metadata(ptr(outMetadata));
```

For `get_vector_metadata` / `get_set_metadata` / `get_time_series_metadata`:
```typescript
// Allocate 32 bytes for quiver_group_metadata_t
const outMetadata = new Uint8Array(32);
check(lib.quiver_database_get_vector_metadata(
  handle, toCString(collection), toCString(groupName), ptr(outMetadata)
));
const result = readGroupMetadata(ptr(outMetadata), 0);
lib.quiver_database_free_group_metadata(ptr(outMetadata));
```

### Pattern: Reading Arrays of Structs for List Functions

For `list_scalar_attributes`, the C API allocates an array of structs and returns it via `quiver_scalar_metadata_t**`. The JS side needs a pointer-to-pointer and a size out-param:

```typescript
const outMetadata = allocPointerOut(); // BigInt64Array(1) for pointer-to-array
const outCount = new BigUint64Array(1);
check(lib.quiver_database_list_scalar_attributes(
  handle, toCString(collection), ptr(outMetadata), ptr(outCount)
));
const count = Number(outCount[0]);
if (count === 0) return [];

const arrPtr = readPointerOut(outMetadata);
const result: ScalarMetadata[] = new Array(count);
for (let i = 0; i < count; i++) {
  result[i] = readScalarMetadata(arrPtr, i * 56);  // stride = sizeof(quiver_scalar_metadata_t)
}
lib.quiver_database_free_scalar_metadata_array(arrPtr, count);
```

### TypeScript Types (new in types.ts or metadata.ts)

```typescript
export interface ScalarMetadata {
  name: string;
  dataType: number;       // QUIVER_DATA_TYPE_* enum value
  notNull: boolean;
  primaryKey: boolean;
  defaultValue: string | null;
  isForeignKey: boolean;
  referencesCollection: string | null;
  referencesColumn: string | null;
}

export interface GroupMetadata {
  groupName: string;
  dimensionColumn: string | null;  // null for vector/set, populated for time series
  valueColumns: ScalarMetadata[];
}
```

### Anti-Patterns to Avoid
- **Wrong struct size/offsets:** Struct padding is architecture-dependent. The offsets above are verified for x64 Windows (the target platform). Do not guess.
- **Forgetting to free:** Every get/list call allocates C memory. Must call the corresponding free function after reading the data into JS objects.
- **Reading nullable pointer as CString without null check:** `default_value`, `dimension_column`, `references_collection`, `references_column` can be NULL. Must check `read.ptr() !== 0` before constructing CString.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Struct reading | Manual offset arithmetic scattered across methods | Centralized `readScalarMetadata(basePtr, offset)` and `readGroupMetadata(basePtr, offset)` helpers | Single source of truth for offsets; reused by both get and list functions |
| Pointer-to-pointer out-params | New allocation pattern | Existing `allocPointerOut()` / `readPointerOut()` from ffi-helpers.ts | Already proven in read.ts |

## Common Pitfalls

### Pitfall 1: Struct Padding Between int and pointer Fields
**What goes wrong:** Reading a pointer at offset 20 instead of 24 for `default_value` because `primary_key` is only 4 bytes at offset 16, but the next pointer at offset 24 is aligned to 8.
**Why it happens:** C compilers insert 4 bytes of padding after `primary_key` (offset 16+4=20) to align `default_value` to an 8-byte boundary.
**How to avoid:** Use the verified offset table above. Same padding exists after `is_foreign_key` (offset 32+4=36, but `references_collection` starts at 40).
**Warning signs:** Garbage pointer values, segfaults, or reading zeros when strings are expected.

### Pitfall 2: Null Pointer Strings
**What goes wrong:** Calling `new CString(0)` or `new CString(null)` when a nullable field (default_value, dimension_column, references_*) is NULL.
**Why it happens:** C API returns NULL pointer for optional fields.
**How to avoid:** Check pointer value before constructing CString: `const p = read.ptr(base, offset); return p ? new CString(p).toString() : null;`

### Pitfall 3: Forgetting to Free Metadata After Reading
**What goes wrong:** Memory leak - C-allocated strings inside the struct are never freed.
**Why it happens:** Reading fields into JS objects does not free the C memory.
**How to avoid:** Always call `quiver_database_free_scalar_metadata` / `quiver_database_free_group_metadata` (for single get) or `quiver_database_free_scalar_metadata_array` / `quiver_database_free_group_metadata_array` (for list) after copying data to JS.

### Pitfall 4: Wrong Stride When Iterating Struct Arrays
**What goes wrong:** Reading overlapping or out-of-bounds memory when iterating `list_*` results.
**Why it happens:** Using wrong sizeof when computing `base + i * stride`.
**How to avoid:** `quiver_scalar_metadata_t` stride = 56, `quiver_group_metadata_t` stride = 32. These are compile-verified.

## Code Examples

### Reading a quiver_scalar_metadata_t from a Pointer

```typescript
// Source: verified struct layout from project compilation
import { CString, type Pointer, read } from "bun:ffi";

const SCALAR_METADATA_SIZE = 56;

function readScalarMetadata(base: Pointer, byteOffset: number): ScalarMetadata {
  const namePtr = read.ptr(base, byteOffset + 0);
  const defaultPtr = read.ptr(base, byteOffset + 24);
  const refCollPtr = read.ptr(base, byteOffset + 40);
  const refColPtr = read.ptr(base, byteOffset + 48);

  return {
    name: new CString(namePtr).toString(),
    dataType: read.i32(base, byteOffset + 8),
    notNull: read.i32(base, byteOffset + 12) !== 0,
    primaryKey: read.i32(base, byteOffset + 16) !== 0,
    defaultValue: defaultPtr ? new CString(defaultPtr).toString() : null,
    isForeignKey: read.i32(base, byteOffset + 32) !== 0,
    referencesCollection: refCollPtr ? new CString(refCollPtr).toString() : null,
    referencesColumn: refColPtr ? new CString(refColPtr).toString() : null,
  };
}
```

### Reading a quiver_group_metadata_t from a Pointer

```typescript
const GROUP_METADATA_SIZE = 32;

function readGroupMetadata(base: Pointer, byteOffset: number): GroupMetadata {
  const groupNamePtr = read.ptr(base, byteOffset + 0);
  const dimColPtr = read.ptr(base, byteOffset + 8);
  const valueColsPtr = read.ptr(base, byteOffset + 16);
  const valueColCount = Number(read.u64(base, byteOffset + 24));

  const valueColumns: ScalarMetadata[] = new Array(valueColCount);
  for (let i = 0; i < valueColCount; i++) {
    valueColumns[i] = readScalarMetadata(valueColsPtr, i * SCALAR_METADATA_SIZE);
  }

  return {
    groupName: new CString(groupNamePtr).toString(),
    dimensionColumn: dimColPtr ? new CString(dimColPtr).toString() : null,
    valueColumns,
  };
}
```

### FFI Symbol Declarations (to add to loader.ts)

```typescript
// Metadata get functions
quiver_database_get_scalar_metadata: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_get_vector_metadata: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_get_set_metadata: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_get_time_series_metadata: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},

// Metadata free functions
quiver_database_free_scalar_metadata: {
  args: [FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_free_group_metadata: {
  args: [FFIType.ptr],
  returns: FFIType.i32,
},

// Metadata list functions
quiver_database_list_scalar_attributes: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_list_vector_groups: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_list_set_groups: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_list_time_series_groups: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},

// Metadata array free functions
quiver_database_free_scalar_metadata_array: {
  args: [FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
quiver_database_free_group_metadata_array: {
  args: [FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
```

### Test Schema

Use `collections.sql` for metadata tests -- it has all 4 attribute types:
- Scalar: `some_integer`, `some_float` on `Collection`
- Vector: `Collection_vector_values` (group name: `values`)
- Set: `Collection_set_tags` (group name: `tags`)
- Time series: `Collection_time_series_data` (group name: `data`, dimension_column: `date_time`)

### Registration in index.ts

```typescript
import "./metadata";
// Add ScalarMetadata and GroupMetadata to type exports
export type { ScalarMetadata, GroupMetadata } from "./metadata";
```

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in) |
| Config file | none (bun:test auto-discovers *.test.ts) |
| Quick run command | `cd bindings/js && bun test test/metadata.test.ts` |
| Full suite command | `cd bindings/js && bun test` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| JSMETA-01 | getScalarMetadata returns correct fields | unit | `cd bindings/js && bun test test/metadata.test.ts` | No - Wave 0 |
| JSMETA-01 | getVectorMetadata returns group with columns | unit | same | No - Wave 0 |
| JSMETA-01 | getSetMetadata returns group with columns | unit | same | No - Wave 0 |
| JSMETA-01 | getTimeSeriesMetadata returns dimensionColumn | unit | same | No - Wave 0 |
| JSMETA-02 | listScalarAttributes returns all scalars | unit | same | No - Wave 0 |
| JSMETA-02 | listVectorGroups returns all vector groups | unit | same | No - Wave 0 |
| JSMETA-02 | listSetGroups returns all set groups | unit | same | No - Wave 0 |
| JSMETA-02 | listTimeSeriesGroups returns all ts groups | unit | same | No - Wave 0 |

### Wave 0 Gaps
- [ ] `bindings/js/test/metadata.test.ts` -- covers JSMETA-01, JSMETA-02

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- quiver_scalar_metadata_t and quiver_group_metadata_t struct definitions, all metadata C API function signatures
- `src/c/database_metadata.cpp` -- C API metadata implementation (get, list, free functions)
- `src/c/database_helpers.h` -- convert_scalar_to_c, convert_group_to_c, free_scalar_fields, free_group_fields helpers
- Compiled struct layout check (g++ on Windows x64) -- verified all offsets and sizes

### Secondary (HIGH confidence)
- `bindings/dart/lib/src/metadata.dart` -- Dart ScalarMetadata/GroupMetadata classes (reference for JS type design)
- `bindings/dart/lib/src/database_metadata.dart` -- Dart metadata method implementations (reference for flow)
- `tests/test_c_api_database_metadata.cpp` -- C API metadata tests (reference for what to test)
- `bindings/js/src/read.ts` -- Established JS binding pattern for FFI struct reading

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - bun:ffi is the only option, already in use
- Architecture: HIGH - follows established patterns from read.ts/create.ts, struct layouts compile-verified
- Pitfalls: HIGH - struct layout verified by compilation, nullable fields identified from C header

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable -- C API and bun:ffi are both mature in this project)
