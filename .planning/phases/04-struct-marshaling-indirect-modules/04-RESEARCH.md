# Phase 4: Struct Marshaling & Indirect Modules - Research

**Researched:** 2026-04-17
**Domain:** Deno FFI struct reading, metadata marshaling, import path migration
**Confidence:** HIGH

## Summary

Phase 4 has two distinct workstreams: (1) rewriting the metadata struct reading in `metadata.ts` from koffi to Deno `UnsafePointerView`, and (2) fixing all remaining `.js` import extensions across all 16 source files to `.ts` for Deno module resolution compatibility. The metadata rewrite is the technically complex part -- it requires reading C struct fields at exact byte offsets using `UnsafePointerView.getInt32()`, `getPointer()`, and `getCString()`. The import extension fix is mechanical but touches every file.

The koffi removal in `metadata.ts` is the last koffi import in the entire codebase. After this phase, success criterion 4 ("No koffi import remains anywhere in bindings/js/src/") will be satisfied. The indirect modules (database.ts, create.ts, read.ts, transaction.ts, introspection.ts, composites.ts, lua-runner.ts, errors.ts, types.ts, index.ts) are already koffi-free and Deno-compatible in their logic -- their only remaining issue is the `.js` import extensions that cause TS2307 errors under `deno check`.

**Primary recommendation:** Rewrite metadata.ts struct reading using `Deno.UnsafePointerView` at verified byte offsets (56-byte ScalarMetadata, 32-byte GroupMetadata), then fix all `.js` -> `.ts` import extensions across all 16 source files, then verify with `deno check`.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| MARSH-05 | Struct field reading at byte offsets works for metadata (ScalarMetadata, GroupMetadata) | Struct layouts verified from C headers; UnsafePointerView API confirmed in Deno 2.7.10; byte offset map computed and cross-verified against existing koffi code and Dart/Julia bindings |
</phase_requirements>

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Deno FFI | 2.7.10 (built-in) | UnsafePointerView for struct field reading | Already established in Phase 1-3; native to Deno runtime | [VERIFIED: `deno --version` output] |

### Supporting

No additional libraries needed. All struct reading uses built-in `Deno.UnsafePointerView` methods.

## Architecture Patterns

### Struct Layout: quiver_scalar_metadata_t (56 bytes, x64)

Verified from C header `include/quiver/c/database.h` lines 205-214 and cross-referenced against existing koffi metadata.ts offsets: [VERIFIED: source code inspection]

```
Offset  Size  Type        Field                     Notes
------  ----  ----------  ------------------------  -------------------------
 0       8    pointer     name                      const char*
 8       4    int32       data_type                 enum quiver_data_type_t
12       4    int32       not_null                  boolean-as-int
16       4    int32       primary_key               boolean-as-int
20       4    (padding)   ---                       alignment for next pointer
24       8    pointer     default_value             const char*, NULL if none
32       4    int32       is_foreign_key            boolean-as-int
36       4    (padding)   ---                       alignment for next pointer
40       8    pointer     references_collection     const char*, NULL if not FK
48       8    pointer     references_column         const char*, NULL if not FK
------
56 bytes total
```

### Struct Layout: quiver_group_metadata_t (32 bytes, x64)

Verified from C header `include/quiver/c/database.h` lines 216-221: [VERIFIED: source code inspection]

```
Offset  Size  Type        Field                     Notes
------  ----  ----------  ------------------------  -------------------------
 0       8    pointer     group_name                const char*
 8       8    pointer     dimension_column          const char*, NULL for vec/set
16       8    pointer     value_columns             quiver_scalar_metadata_t*
24       8    uint64      value_column_count        size_t on x64
------
32 bytes total
```

### Pattern 1: Struct Field Reading via UnsafePointerView

**What:** Replace koffi.decode() calls with Deno.UnsafePointerView methods at computed byte offsets
**When to use:** Reading C struct fields from native memory returned by FFI calls

The key insight: the `get_*_metadata()` C API functions take a _pointer_ to an output struct (the caller allocates the struct buffer, the C function fills it). For `list_*()` functions, the C API returns a _pointer_ to a heap-allocated array of structs (the caller passes a pointer-out-parameter).

**For get functions (single struct):**
The caller must allocate a buffer of the struct's size, pass it to the C function, then read fields from the buffer using DataView/UnsafePointerView.

```typescript
// Source: Deno FFI docs + existing Phase 2/3 patterns
// Allocate struct buffer, get pointer to it
const buf = new Uint8Array(SCALAR_METADATA_SIZE);  // 56 bytes
const bufPtr = Deno.UnsafePointer.of(buf)!;
check(lib.quiver_database_get_scalar_metadata(handle, collBuf, attrBuf, bufPtr));

// Read fields using UnsafePointerView on the buffer
const view = new Deno.UnsafePointerView(bufPtr);
const namePtr = view.getPointer(0);           // offset 0: name
const name = namePtr ? new Deno.UnsafePointerView(namePtr).getCString() : "";
const dataType = view.getInt32(8);            // offset 8: data_type
const notNull = view.getInt32(12) !== 0;      // offset 12: not_null
const primaryKey = view.getInt32(16) !== 0;   // offset 16: primary_key
```

**For list functions (array of structs):**
The C API returns a pointer to a heap-allocated array. Use `readPtrOut()` to get the pointer, then construct `UnsafePointerView` and read fields at `i * STRUCT_SIZE + fieldOffset`.

```typescript
// Source: existing listMetadata pattern + UnsafePointerView
const outArray = allocPtrOut();
const outCount = allocUint64Out();
check(lib.quiver_database_list_scalar_attributes(handle, collBuf, outArray.ptr, outCount.ptr));
const count = readUint64Out(outCount);
const arrPtr = readPtrOut(outArray);

const view = new Deno.UnsafePointerView(arrPtr!);
for (let i = 0; i < count; i++) {
  const base = i * SCALAR_METADATA_SIZE;
  const namePtr = view.getPointer(base + 0);
  const name = namePtr ? new Deno.UnsafePointerView(namePtr).getCString() : "";
  // ... read other fields at base + offset
}
```

### Pattern 2: Nullable String Pointer Reading

**What:** Read a `const char*` field that may be NULL
**When to use:** For optional string fields (default_value, references_collection, references_column, dimension_column)

```typescript
// Source: pattern from existing ffi-helpers.ts decodeStringArray
function readNullableString(view: Deno.UnsafePointerView, offset: number): string | null {
  const ptr = view.getPointer(offset);
  if (!ptr) return null;
  return new Deno.UnsafePointerView(ptr).getCString();
}
```

### Pattern 3: Import Extension Fix (.js -> .ts)

**What:** Change all `from "./module.js"` imports to `from "./module.ts"` across all 16 source files
**When to use:** Deno requires exact file extensions; `.js` extensions resolve to `.js` files, not `.ts` files [VERIFIED: `deno check` output shows TS2307 errors]

This is a known pre-existing issue documented in Phase 3 summary. Deno's `--sloppy-imports` flag exists but is unstable, prints warnings, and has performance penalties [CITED: docs.deno.com/lint/rules/no-sloppy-imports]. The correct fix is to use `.ts` extensions.

**Scope:** All 16 files in `bindings/js/src/` have `.js` imports that must become `.ts`. This includes both regular imports AND `declare module` augmentation strings.

### Pattern 4: Struct Buffer Allocation for Out-Parameters

**What:** Allocate a Uint8Array of the struct's size, get a pointer via `Deno.UnsafePointer.of()`, pass as the out-parameter to the C function
**When to use:** For `get_*_metadata()` calls where C fills a caller-provided struct

```typescript
// Source: existing Phase 2 pattern (makeDefaultOptions) adapted for metadata
const buf = new Uint8Array(SCALAR_METADATA_SIZE);
const alloc: Allocation = { ptr: Deno.UnsafePointer.of(buf)!, buf };
check(lib.quiver_database_get_scalar_metadata(handle, collBuf, attrBuf, alloc.ptr));
// buf now contains the struct data, read via UnsafePointerView
```

### Pattern 5: Nested Struct Array Reading (GroupMetadata -> ScalarMetadata[])

**What:** GroupMetadata contains a pointer to a heap-allocated array of ScalarMetadata structs. Read the pointer, then iterate using stride-based offsets.
**When to use:** For `readGroupMetadataAt()` where `value_columns` is a pointer to N ScalarMetadata structs

```typescript
// Source: derived from C API database_helpers.h convert_group_to_c
const valueColumnsPtr = view.getPointer(byteOffset + 16);  // offset 16 in GroupMetadata
const count = Number(view.getBigUint64(byteOffset + 24));   // offset 24 in GroupMetadata
if (valueColumnsPtr && count > 0) {
  const colView = new Deno.UnsafePointerView(valueColumnsPtr);
  for (let i = 0; i < count; i++) {
    // Read ScalarMetadata at stride i * 56
    readScalarMetadataAt(colView, i * SCALAR_METADATA_SIZE);
  }
}
```

### Anti-Patterns to Avoid

- **Using DataView for pointer fields:** DataView can read uint64 values but cannot convert them to Deno.PointerValue. Use `UnsafePointerView.getPointer(offset)` for pointer fields instead of reading raw bytes and calling `Deno.UnsafePointer.create()`. `getPointer()` handles null detection natively.
- **Forgetting padding bytes:** The ScalarMetadata struct has 4-byte padding after `primary_key` (offset 20) and after `is_foreign_key` (offset 36). Reading at wrong offsets will produce garbage.
- **Passing typed arrays to pointer parameters:** Deno FFI pointer params expect `Deno.PointerValue`, not `Uint8Array`. Use `Deno.UnsafePointer.of(buf)` to get a pointer from a buffer. (Lesson from Phase 3.)
- **Mixing Allocation and raw Uint8Array for struct out-params:** For consistency with the codebase's established pattern, wrap struct buffers in the `Allocation` type so callers maintain GC-safe buffer references.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Struct field reading | Manual byte shifting | `UnsafePointerView.getInt32/getPointer/getBigUint64` | Built-in methods handle endianness and pointer semantics correctly |
| Nullable string reading | Raw pointer math | `view.getPointer(offset)` with null check + `getCString()` | `getPointer()` returns null for zero pointers natively |
| Import path fixing | Regex-based sed | Manual find-and-replace per file | Only 16 files, each with a handful of imports; mechanical and safe |

## Common Pitfalls

### Pitfall 1: Wrong Byte Offsets Due to Struct Padding

**What goes wrong:** x64 C compilers insert padding bytes to align fields to their natural boundary. A `const char*` (8 bytes) after three `int` fields (12 bytes) gets 4 bytes of padding inserted.
**Why it happens:** The C standard requires natural alignment; the existing metadata.ts already accounts for this correctly (offsets 24, 40, 48 skip padding), but it is easy to get wrong if re-deriving.
**How to avoid:** Use the offset table from this research document. Cross-check against the existing koffi code which has the correct offsets already computed.
**Warning signs:** `getCString()` on a pointer field returns garbage or crashes.

### Pitfall 2: UnsafePointerView on Caller-Allocated vs C-Allocated Buffers

**What goes wrong:** For `get_*_metadata()` the JS side allocates the struct buffer (Uint8Array). For `list_*()` the C side allocates the array. The reading approach differs.
**Why it happens:** `get` functions write into a caller-provided struct pointer. `list` functions return a pointer to a new heap allocation via out-parameter.
**How to avoid:** For `get`: allocate `Uint8Array(structSize)`, pass `Deno.UnsafePointer.of(buf)` as the out-param, then create `UnsafePointerView` from the same pointer. For `list`: use `allocPtrOut()`/`readPtrOut()` pattern, then create `UnsafePointerView` from the returned pointer.
**Warning signs:** Attempting to use `readPtrOut()` on a `get` call's buffer (the buffer IS the struct, not a pointer to a pointer).

### Pitfall 3: declare module Augmentation Paths Must Match Imports

**What goes wrong:** After changing imports from `.js` to `.ts`, the `declare module "./database.js"` augmentation strings must also change to `"./database.ts"`, otherwise TypeScript cannot find the module to augment.
**Why it happens:** TypeScript's module augmentation uses the module specifier as a key. If the actual import uses `.ts` but the augmentation says `.js`, TypeScript sees them as different modules.
**How to avoid:** When fixing import extensions, also fix ALL `declare module "..."` strings in the same file. Check: create.ts, read.ts, query.ts, transaction.ts, metadata.ts, time-series.ts, csv.ts, introspection.ts, composites.ts.
**Warning signs:** TS2664 "Invalid module name in augmentation" errors.

### Pitfall 4: Free Functions Still Need the Original Pointer

**What goes wrong:** After reading struct data from a buffer, the `free_scalar_metadata`/`free_group_metadata` functions need the original pointer (not a new one). For `get` functions the buffer pointer is the struct pointer. For `list` functions the out-parameter pointer is the array pointer.
**Why it happens:** The C API's free functions deallocate the string fields inside the struct, and for arrays, also `delete[]` the array itself.
**How to avoid:** For `get` calls: pass the same Allocation.ptr to the free function. For `list` calls: pass the `readPtrOut()` result to the free function.

### Pitfall 5: Int32Array/BigInt64Array/Float64Array as FFI Out-Parameters

**What goes wrong:** Some indirect modules (introspection.ts, read.ts, transaction.ts, create.ts) pass typed arrays directly to FFI pointer parameters: `new Int32Array(1)`, `new BigInt64Array(1)`, `new Float64Array(1)`.
**Why it happens:** Under koffi, typed arrays were valid as pointer parameters. Under Deno FFI, `"pointer"` params expect `Deno.PointerValue`, not typed arrays. However, the current code may work via implicit buffer coercion if the Deno FFI parameter type is `"buffer"` or `"pointer"` -- need to verify.
**How to avoid:** Convert typed array out-params to `Uint8Array + DataView` pattern (as done in Phase 3 for query.ts and time-series.ts). Use `Deno.UnsafePointer.of(buf)` for pointer params.
**Warning signs:** Type errors like "Type 'Int32Array' is not assignable to parameter of type..." or runtime crashes.

### Pitfall 6: String Arguments to Buffer Parameters

**What goes wrong:** Several indirect modules pass raw JS strings to FFI `"buffer"` parameters (collection names, attribute names, etc.) without encoding via `toCString()`.
**Why it happens:** Under koffi, strings were auto-encoded. Under Deno FFI, `"buffer"` params expect `Uint8Array`.
**How to avoid:** Audit every FFI call in each module. If the symbol definition uses `BUF` for a parameter and the code passes a raw string, wrap it in `toCString().buf`.
**Warning signs:** Type errors about string not being assignable to Uint8Array. This issue exists in introspection.ts, create.ts, read.ts, and database.ts.

## Code Examples

### Reading ScalarMetadata from UnsafePointerView

```typescript
// Source: derived from C header struct layout + Deno UnsafePointerView API
const SCALAR_METADATA_SIZE = 56;

function readScalarMetadataAt(view: Deno.UnsafePointerView, byteOffset: number): ScalarMetadata {
  const namePtr = view.getPointer(byteOffset + 0);
  const defaultPtr = view.getPointer(byteOffset + 24);
  const refCollPtr = view.getPointer(byteOffset + 40);
  const refColPtr = view.getPointer(byteOffset + 48);

  return {
    name: namePtr ? new Deno.UnsafePointerView(namePtr).getCString() : "",
    dataType: view.getInt32(byteOffset + 8),
    notNull: view.getInt32(byteOffset + 12) !== 0,
    primaryKey: view.getInt32(byteOffset + 16) !== 0,
    defaultValue: defaultPtr ? new Deno.UnsafePointerView(defaultPtr).getCString() : null,
    isForeignKey: view.getInt32(byteOffset + 32) !== 0,
    referencesCollection: refCollPtr ? new Deno.UnsafePointerView(refCollPtr).getCString() : null,
    referencesColumn: refColPtr ? new Deno.UnsafePointerView(refColPtr).getCString() : null,
  };
}
```

### Reading GroupMetadata from UnsafePointerView

```typescript
// Source: derived from C header struct layout + Deno UnsafePointerView API
const GROUP_METADATA_SIZE = 32;

function readGroupMetadataAt(view: Deno.UnsafePointerView, byteOffset: number): GroupMetadata {
  const groupNamePtr = view.getPointer(byteOffset + 0);
  const dimColPtr = view.getPointer(byteOffset + 8);
  const valueColumnsPtr = view.getPointer(byteOffset + 16);
  const valueColumnCount = Number(view.getBigUint64(byteOffset + 24));

  const valueColumns: ScalarMetadata[] = new Array(valueColumnCount);
  if (valueColumnsPtr && valueColumnCount > 0) {
    const colView = new Deno.UnsafePointerView(valueColumnsPtr);
    for (let i = 0; i < valueColumnCount; i++) {
      valueColumns[i] = readScalarMetadataAt(colView, i * SCALAR_METADATA_SIZE);
    }
  }

  return {
    groupName: groupNamePtr ? new Deno.UnsafePointerView(groupNamePtr).getCString() : "",
    dimensionColumn: dimColPtr ? new Deno.UnsafePointerView(dimColPtr).getCString() : null,
    valueColumns,
  };
}
```

### getScalarMetadata with Uint8Array Struct Buffer

```typescript
// Source: existing Phase 2/3 Allocation pattern + metadata-specific adaptation
Database.prototype.getScalarMetadata = function (this: Database, collection: string, attribute: string): ScalarMetadata {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const outBuf = new Uint8Array(SCALAR_METADATA_SIZE);
  const outPtr = Deno.UnsafePointer.of(outBuf)!;
  check(lib.quiver_database_get_scalar_metadata(this._handle, collBuf.buf, attrBuf.buf, outPtr));
  const view = new Deno.UnsafePointerView(outPtr);
  const result = readScalarMetadataAt(view, 0);
  lib.quiver_database_free_scalar_metadata(outPtr);
  return result;
};
```

### Fixing Typed Array Out-Parameters in Indirect Modules

```typescript
// BEFORE (introspection.ts -- incompatible with Deno FFI pointer params):
const outHealthy = new Int32Array(1);
check(lib.quiver_database_is_healthy(this._handle, outHealthy));
return outHealthy[0] !== 0;

// AFTER (Deno-compatible):
const outHealthy = new Uint8Array(4);
const outHealthyPtr = Deno.UnsafePointer.of(outHealthy)!;
check(lib.quiver_database_is_healthy(this._handle, outHealthyPtr));
return new DataView(outHealthy.buffer).getInt32(0, true) !== 0;
```

## File-by-File Analysis

### Files Requiring Logic Changes

| File | koffi? | Buffer? | Typed Array Out-Params? | String Args to BUF? | .js imports? |
|------|--------|---------|------------------------|---------------------|-------------|
| metadata.ts | YES (1 import, 9 koffi.decode calls) | YES (4 Buffer.alloc) | No | No (uses allocPtrOut) | YES (5 imports + 1 declare module) |
| introspection.ts | No | No | YES (Int32Array, BigInt64Array) | YES (raw strings to BUF params) | YES (4 imports + 1 declare module) |
| create.ts | No | No | YES (BigInt64Array) | YES (raw strings to BUF params) | YES (4 imports + 1 declare module) |
| read.ts | No | No | YES (BigInt64Array, Int32Array, Float64Array) | YES (raw strings to BUF params) | YES (2 imports + 1 declare module) |
| database.ts | No | No | No (uses allocPtrOut correctly) | YES (raw strings to BUF params) | YES (4 imports) |
| transaction.ts | No | No | YES (Int32Array) | No (handle-only calls) | YES (3 imports + 1 declare module) |
| composites.ts | No | No | No (calls other methods) | No (calls other methods) | YES (1 import + 1 declare module) |
| lua-runner.ts | No | No | No (uses allocPtrOut) | YES (raw strings to BUF params) | YES (5 imports) |
| errors.ts | No | No | No | No | YES (1 import) |
| types.ts | No | No | No | No | No (no imports) |
| index.ts | No | No | No | No | YES (17 imports/exports) |
| loader.ts | No | No | No | No | YES (1 import) |
| ffi-helpers.ts | No | No | No | No | YES (1 import) |
| csv.ts | No | No | No | No (already uses toCString) | YES (4 imports + 1 declare module) |
| query.ts | No | No | No (already uses Uint8Array+DataView) | No (already uses toCString) | YES (4 imports + 1 declare module) |
| time-series.ts | No | No | No (already uses Uint8Array+DataView) | No (already uses toCString) | YES (4 imports + 1 declare module) |

### Summary of Changes Needed

1. **metadata.ts** -- Full rewrite: remove koffi import, remove Buffer.alloc, rewrite struct readers with UnsafePointerView, add toCString for string args, fix imports
2. **introspection.ts** -- Convert Int32Array/BigInt64Array to Uint8Array+DataView, add toCString for string args, fix imports
3. **create.ts** -- Convert BigInt64Array to Uint8Array+DataView, add toCString for string args, add BigInt() for i64 params, fix imports
4. **read.ts** -- Convert Int32Array/BigInt64Array/Float64Array to Uint8Array+DataView, add toCString for string args, add BigInt() for i64 params, fix imports
5. **database.ts** -- Add toCString for string args, fix imports
6. **transaction.ts** -- Convert Int32Array to Uint8Array+DataView, fix imports
7. **lua-runner.ts** -- Add toCString for string args, fix imports
8. **composites.ts, errors.ts, types.ts, index.ts, loader.ts, ffi-helpers.ts, csv.ts, query.ts, time-series.ts** -- Fix .js -> .ts imports only

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| koffi.decode(ptr, offset, type) | Deno.UnsafePointerView.getInt32/getPointer/getCString at offsets | Phase 4 (this phase) | Direct memory access without koffi dependency |
| Buffer.alloc(size) for struct buffers | new Uint8Array(size) + Deno.UnsafePointer.of() | Phase 2-3 pattern | No Node.js Buffer dependency |
| .js import extensions (Node.js convention) | .ts import extensions (Deno convention) | Phase 4 (this phase) | Proper Deno module resolution without --sloppy-imports |
| Int32Array/BigInt64Array as FFI out-params | Uint8Array + DataView + UnsafePointer.of() | Phase 3 pattern | Type-safe Deno FFI pointer parameters |

## Assumptions Log

> List all claims tagged [ASSUMED] in this research.

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Typed array out-params (Int32Array, BigInt64Array, Float64Array) in indirect modules will fail as Deno FFI pointer arguments | Pitfall 5, File Analysis | If Deno coerces them automatically, no conversion needed -- but Phase 3 had to fix this explicitly, so conversion is likely required |
| A2 | Raw string arguments to BUF-typed FFI params in indirect modules will fail | Pitfall 6, File Analysis | If Deno auto-encodes strings for buffer params, no toCString needed -- but Phase 3 had to fix this explicitly |

**Note:** Both assumptions are strongly supported by Phase 3 experience where identical patterns required the same fixes. The risk of being wrong is low (it would mean less work, not broken code).

## Open Questions

1. **UnsafePointerView constructor from Uint8Array pointer**
   - What we know: For `get_*_metadata()` calls, we allocate a `Uint8Array(structSize)` and pass `Deno.UnsafePointer.of(buf)` as the out-param. We then need to read fields from this buffer.
   - What's unclear: Whether we should create `new Deno.UnsafePointerView(ptr)` from the pointer, or read directly from the buffer via `DataView`. Both should work since the buffer memory is the same.
   - Recommendation: Use `UnsafePointerView` for pointer fields (it has `getPointer()`), and `DataView` for int32 fields. Alternatively, use `UnsafePointerView` for everything since it provides all needed methods (getInt32, getPointer, getBigUint64, getCString).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Deno runtime | All FFI operations | Yes | 2.7.10 | -- |
| libquiver_c.dll | FFI symbol calls | Yes (build/bin) | 0.6.0 | -- |

No missing dependencies.

## Sources

### Primary (HIGH confidence)
- C header `include/quiver/c/database.h` -- struct definitions for quiver_scalar_metadata_t (lines 205-214) and quiver_group_metadata_t (lines 216-221)
- C header `include/quiver/c/options.h` -- struct definitions for quiver_database_options_t and quiver_csv_options_t
- C implementation `src/c/database_helpers.h` -- convert_scalar_to_c and convert_group_to_c showing field initialization order
- C implementation `src/c/database_metadata.cpp` -- get/list/free function implementations confirming out-parameter semantics
- Existing `bindings/js/src/metadata.ts` -- current koffi-based implementation with verified byte offsets (56/32 sizes, field offsets)
- Existing `bindings/dart/lib/src/metadata.dart` -- Dart binding cross-reference for struct field reading
- Existing `bindings/julia/src/database_metadata.jl` -- Julia binding cross-reference for struct parsing
- Phase 2 and Phase 3 SUMMARY files -- established patterns for Allocation type, Uint8Array+DataView, toCString
- `deno check` output on all files -- verified current type error state

### Secondary (MEDIUM confidence)
- [Deno FFI docs](https://docs.deno.com/api/deno/ffi) -- UnsafePointerView API methods
- [Denonomicon structs guide](https://denonomicon.deno.dev/types/structs) -- Deno FFI struct handling patterns
- [Denonomicon pointers guide](https://denonomicon.deno.dev/types/pointers) -- pointer reading patterns
- [Deno sloppy imports lint rule](https://docs.deno.com/lint/rules/no-sloppy-imports) -- confirmation that .ts extensions are preferred over --sloppy-imports

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Deno 2.7.10 built-in FFI, no external libraries
- Architecture: HIGH - struct layouts verified from C headers and cross-referenced against 3 binding implementations (JS/koffi, Dart, Julia)
- Pitfalls: HIGH - all pitfalls derived from verified Phase 3 experience with identical patterns
- Import fixes: HIGH - `deno check` output confirms every TS2307 error from .js extensions

**Research date:** 2026-04-17
**Valid until:** 2026-05-17 (stable -- Deno FFI API unlikely to change, C struct layouts fixed)
