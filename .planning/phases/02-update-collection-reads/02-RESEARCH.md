# Phase 2: Update & Collection Reads - Research

**Researched:** 2026-03-09
**Domain:** Bun FFI binding -- update_element + vector/set bulk reads + by-ID reads
**Confidence:** HIGH

## Summary

Phase 2 adds `updateElement`, bulk vector/set reads, and by-ID vector/set reads to the JS binding. All C API functions needed already exist and are battle-tested. The existing JS binding (Phase 1) established clear patterns for FFI symbol declaration, pointer marshaling, array reading, and memory freeing. This phase is mechanical: declare new FFI symbols in `loader.ts`, write wrapper methods using the same patterns, and add tests.

The key complexity is the **vector/set bulk read return type**: the C API returns a `T***` (array of arrays) plus `size_t*` (sizes per element) plus `size_t` (element count). This requires double pointer dereferencing in bun:ffi, which is straightforward but must be done carefully. The by-ID variants are simpler -- they return flat `T**`/`size_t*` like scalar reads.

**Primary recommendation:** Follow the exact patterns from `create.ts` (for `updateElement`) and `read.ts` (for all read functions), adding new source files `update.ts` and extending `read.ts` with vector/set methods.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| JSUP-01 | User can update an element via `updateElement(collection, id, data)` | Reuse `setElementField`/`setElementArray` from `create.ts` + call `quiver_database_update_element` |
| JSRD-01 | User can read vector integers/floats/strings (bulk) | New FFI symbols for `read_vector_*` + `free_integer/float/string_vectors` + double-deref pattern |
| JSRD-02 | User can read vector integers/floats/strings by element ID | New FFI symbols for `read_vector_*_by_id` -- same pattern as existing `read_scalar_*_by_id` |
| JSRD-03 | User can read set integers/floats/strings (bulk) | New FFI symbols for `read_set_*` -- identical signatures to vector bulk reads |
| JSRD-04 | User can read set integers/floats/strings by element ID | New FFI symbols for `read_set_*_by_id` -- identical signatures to vector by-ID reads |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | built-in | FFI calls to libquiver_c | Already used; project targets Bun only |
| bun:test | built-in | Test framework | Already used in Phase 1 tests |
| TypeScript | ^5.9.3 | Type safety | Already configured in project |
| Biome | ^2.4.6 | Lint + format | Already configured in project |

### Supporting
No additional libraries needed. Everything required is already in the project.

## Architecture Patterns

### File Organization
```
bindings/js/src/
  update.ts          # NEW - updateElement (augments Database)
  read.ts            # EXTEND - add vector/set bulk + by-ID methods
  loader.ts          # EXTEND - add 18 new FFI symbol declarations
  index.ts           # EXTEND - import "./update"
  database.ts        # UNCHANGED
  create.ts          # UNCHANGED (but updateElement reuses its helpers)
  types.ts           # UNCHANGED
  ffi-helpers.ts     # UNCHANGED
  errors.ts          # UNCHANGED
  query.ts           # UNCHANGED
  transaction.ts     # UNCHANGED
bindings/js/test/
  update.test.ts     # NEW - updateElement tests
  read.test.ts       # EXTEND - add vector/set read tests
```

### Pattern 1: Module Augmentation (Declaration Merging)
**What:** Each feature file augments the `Database` class via TypeScript module augmentation (`declare module`) and prototype assignment.
**When to use:** Always -- this is the established pattern in the codebase.
**Example:**
```typescript
// Source: bindings/js/src/create.ts (existing code)
declare module "./database" {
  interface Database {
    updateElement(collection: string, id: number, data: ElementData): void;
  }
}

Database.prototype.updateElement = function (
  this: Database,
  collection: string,
  id: number,
  data: ElementData,
): void {
  // implementation
};
```

### Pattern 2: FFI Symbol Declaration
**What:** All C API functions declared in `loader.ts` with exact FFI types.
**When to use:** Every new C API function needs a symbol entry.
**Example:**
```typescript
// Source: bindings/js/src/loader.ts (existing pattern)
quiver_database_read_vector_integers: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
```

### Pattern 3: Bulk Vector/Set Read (Double Pointer Dereference)
**What:** C API returns `T***` (array of T* pointers) + `size_t**` (array of sizes) + `size_t*` (element count). JS must dereference two levels.
**When to use:** All 6 bulk read functions (3 vector + 3 set).
**Example:**
```typescript
// Read array of arrays pattern for integers
Database.prototype.readVectorIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): number[][] {
  const lib = getSymbols();
  const handle = this._handle;

  const outVectors = allocPointerOut();   // T*** -> pointer to array of pointers
  const outSizes = allocPointerOut();     // size_t** -> pointer to sizes array
  const outCount = new BigUint64Array(1); // size_t*

  check(lib.quiver_database_read_vector_integers(
    handle, toCString(collection), toCString(attribute),
    ptr(outVectors), ptr(outSizes), ptr(outCount),
  ));

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const vectorsPtr = readPointerOut(outVectors); // int64_t**
  const sizesPtr = readPointerOut(outSizes);     // size_t*

  // Read sizes array
  const sizesBuf = toArrayBuffer(sizesPtr, 0, count * 8);
  const sizes = new BigUint64Array(sizesBuf);

  const result: number[][] = new Array(count);
  for (let i = 0; i < count; i++) {
    const size = Number(sizes[i]);
    if (size === 0) {
      result[i] = [];
    } else {
      const vecPtr = readPtrAt(vectorsPtr, i * 8); // int64_t*
      const buf = toArrayBuffer(vecPtr, 0, size * 8);
      const bigInts = new BigInt64Array(buf);
      result[i] = new Array(size);
      for (let j = 0; j < size; j++) {
        result[i][j] = Number(bigInts[j]);
      }
    }
  }

  lib.quiver_database_free_integer_vectors(vectorsPtr, sizesPtr, count);
  return result;
};
```

### Pattern 4: By-ID Vector/Set Read (Flat Array)
**What:** Same signature as scalar bulk reads: `T**` + `size_t*`.
**When to use:** All 6 by-ID read functions.
**Example:**
```typescript
// By-ID returns flat array, same pattern as readScalarIntegers
Database.prototype.readVectorIntegersById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(lib.quiver_database_read_vector_integers_by_id(
    handle, toCString(collection), toCString(attribute),
    id, ptr(outValues), ptr(outCount),
  ));

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  lib.quiver_database_free_integer_array(valuesPtr);
  return result;
};
```

### Pattern 5: updateElement (Reuse createElement Helpers)
**What:** `updateElement` uses the exact same `setElementField`/`setElementArray` helpers as `createElement`, but calls `quiver_database_update_element` instead of `quiver_database_create_element`.
**When to use:** For JSUP-01.
**Key difference from createElement:** `updateElement` takes `id` parameter, does not return an ID, and the C API signature is `(db, collection, id, element)`.

### Anti-Patterns to Avoid
- **Duplicating `setElementField`/`setElementArray`:** Extract these from `create.ts` into a shared location (or import from `create.ts` module scope). Do NOT copy-paste.
- **Forgetting to free vectors:** Every `read_vector_*` / `read_set_*` bulk call requires calling the matching `free_*_vectors` function. The by-ID calls use `free_integer/float/string_array`.
- **Using `size_t` as `BigUint64Array` vs `BigInt64Array`:** The C API returns `size_t` (unsigned). Use `BigUint64Array` for counts/sizes consistently, matching the existing pattern in `read.ts`.
- **Mixing up vector and set free functions:** Sets reuse vector free functions in the C API (`quiver_database_free_integer_vectors`, etc.). This is intentional -- sets and vectors have identical memory layout.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Element field marshaling | New setters for update | Reuse `setElementField`/`setElementArray` from create.ts | Identical logic, code duplication risk |
| Array-of-arrays memory layout | Custom buffer parsing | `readPtrAt` + `toArrayBuffer` from bun:ffi | Existing helpers handle pointer arithmetic correctly |
| Set free functions | New free calls | `free_integer/float/string_vectors` | C API uses same free functions for both vectors and sets |

## Common Pitfalls

### Pitfall 1: Double Pointer Dereference for Bulk Reads
**What goes wrong:** Treating `T***` like `T**` -- reading data directly from the outer pointer instead of dereferencing to get each inner array pointer.
**Why it happens:** Scalar bulk reads are `T**` (one level), vector bulk reads are `T***` (two levels). Easy to confuse.
**How to avoid:** The outer pointer gives you `T**` (array of pointers). Each `T*` at index `i` (accessed via `readPtrAt(vectorsPtr, i * 8)`) points to the actual data array for element `i`.
**Warning signs:** Getting garbage data or crashes when reading vector values.

### Pitfall 2: Sizes Array is Also Heap-Allocated
**What goes wrong:** Forgetting that `out_sizes` is a `size_t**` (pointer to heap-allocated array), not `size_t*`.
**Why it happens:** The sizes array is allocated by the C API alongside the vectors array. Both must be dereferenced.
**How to avoid:** `readPointerOut(outSizes)` gives the `size_t*` pointer. Then `toArrayBuffer(sizesPtr, 0, count * 8)` gives the sizes. The free function handles deallocation.

### Pitfall 3: Empty Inner Arrays
**What goes wrong:** Trying to dereference a null pointer for an element that has no vector/set entries.
**Why it happens:** C API returns `nullptr` for empty inner arrays (size = 0).
**How to avoid:** Always check `size === 0` before calling `readPtrAt` and `toArrayBuffer` for each inner array.

### Pitfall 4: String Vectors Need Two-Level String Dereference
**What goes wrong:** Reading string vector data incorrectly because it's `char****` (pointer to array of `char**` arrays).
**Why it happens:** Each element has a `char**` (array of `char*` strings), and the outer array holds pointers to these.
**How to avoid:** First dereference to get `char***`, then for each element dereference to get `char**`, then for each string dereference to get `char*`. Use `readPtrAt` at each level.

### Pitfall 5: updateElement Shares Element Lifecycle with createElement
**What goes wrong:** Not destroying the element handle after update, or destroying it before the FFI call completes.
**Why it happens:** Copy-paste error from createElement without the `try/finally`.
**How to avoid:** Always use `try/finally` with `quiver_element_destroy` in the finally block.

## Code Examples

### updateElement Implementation
```typescript
// Source: Derived from existing create.ts pattern + C API quiver_database_update_element
Database.prototype.updateElement = function (
  this: Database,
  collection: string,
  id: number,
  data: ElementData,
): void {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPointerOut();
  check(lib.quiver_element_create(ptr(outElem)));
  const elemPtr = readPointerOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }

    check(lib.quiver_database_update_element(handle, toCString(collection), id, elemPtr));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};
```

### FFI Symbols Needed (18 new entries)
```typescript
// Update element
quiver_database_update_element: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr],
  returns: FFIType.i32,
},

// Read vector bulk (3)
quiver_database_read_vector_integers: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_vector_floats: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_vector_strings: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},

// Read set bulk (3)
quiver_database_read_set_integers: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_set_floats: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_set_strings: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},

// Read vector by ID (3)
quiver_database_read_vector_integers_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_vector_floats_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_vector_strings_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},

// Read set by ID (3)
quiver_database_read_set_integers_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_set_floats_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_set_strings_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},

// Free vector arrays (3) - used by both vector and set bulk reads
quiver_database_free_integer_vectors: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
quiver_database_free_float_vectors: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
quiver_database_free_string_vectors: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
  returns: FFIType.i32,
},
```

### Return Types Summary
| JS Method | Return Type | C API Out-Params |
|-----------|------------|------------------|
| `readVectorIntegers(c, a)` | `number[][]` | `int64_t***`, `size_t**`, `size_t*` |
| `readVectorFloats(c, a)` | `number[][]` | `double***`, `size_t**`, `size_t*` |
| `readVectorStrings(c, a)` | `string[][]` | `char****`, `size_t**`, `size_t*` |
| `readSetIntegers(c, a)` | `number[][]` | `int64_t***`, `size_t**`, `size_t*` |
| `readSetFloats(c, a)` | `number[][]` | `double***`, `size_t**`, `size_t*` |
| `readSetStrings(c, a)` | `string[][]` | `char****`, `size_t**`, `size_t*` |
| `readVectorIntegersById(c, a, id)` | `number[]` | `int64_t**`, `size_t*` |
| `readVectorFloatsById(c, a, id)` | `number[]` | `double**`, `size_t*` |
| `readVectorStringsById(c, a, id)` | `string[]` | `char***`, `size_t*` |
| `readSetIntegersById(c, a, id)` | `number[]` | `int64_t**`, `size_t*` |
| `readSetFloatsById(c, a, id)` | `number[]` | `double**`, `size_t*` |
| `readSetStringsById(c, a, id)` | `string[]` | `char***`, `size_t*` |
| `updateElement(c, id, data)` | `void` | (none) |

### Naming Convention (from CLAUDE.md)
| C++ | C API | JS (camelCase) |
|-----|-------|----------------|
| `update_element` | `quiver_database_update_element` | `updateElement` |
| `read_vector_integers` | `quiver_database_read_vector_integers` | `readVectorIntegers` |
| `read_vector_integers_by_id` | `quiver_database_read_vector_integers_by_id` | `readVectorIntegersById` |
| `read_set_strings` | `quiver_database_read_set_strings` | `readSetStrings` |

### Test Schema
The existing `tests/schemas/valid/all_types.sql` already has all needed tables:
- `AllTypes_vector_labels` (string vector)
- `AllTypes_vector_counts` (integer vector)
- `AllTypes_set_codes` (integer set)
- `AllTypes_set_weights` (float set)
- `AllTypes_set_tags` (string set)

Note: There is no float vector table in `all_types.sql`. For `readVectorFloats`, we can use `AllTypes_set_weights` is a set not vector. We need to either: (a) use a different schema that has float vectors, or (b) add a float vector table to `all_types.sql`. The existing `AllTypes_vector_counts` has integers, `AllTypes_vector_labels` has strings. A float vector table is missing. Checking other schemas may help.

### Helper Extraction Strategy
`setElementField` and `setElementArray` in `create.ts` are module-scoped functions (not methods on Database). For `updateElement` to reuse them, either:
1. **Move helpers to a shared module** (e.g., `element-helpers.ts`) and import from both `create.ts` and `update.ts`
2. **Put updateElement in `create.ts`** since it shares all the element-building logic
3. **Import helpers from `create.ts`** by exporting them

Option 2 is simplest and follows the pattern that `create.ts` already has `createElement` and `deleteElement` together (both use Element lifecycle). Adding `updateElement` there keeps all element-mutation operations co-located.

**Recommendation:** Add `updateElement` to `create.ts` (rename mental model to "element mutations") rather than creating a separate `update.ts`. This avoids helper extraction complexity.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| N/A | bun:ffi with typed arrays | Project inception | All FFI calls use this pattern |

No deprecated approaches. The codebase is new (initial JS binding commit `86852b7`).

## Open Questions

1. **Float vector test coverage**
   - What we know: `all_types.sql` has integer and string vectors but no float vector table.
   - What's unclear: Whether to add a float vector table to the existing schema or use a different schema.
   - Recommendation: Add `AllTypes_vector_weights` (float vector) to `all_types.sql` -- this is a test-only schema designed for "all type combinations for gap coverage" per its header comment.

2. **updateElement placement**
   - What we know: `setElementField`/`setElementArray` are private to `create.ts`. updateElement needs them.
   - What's unclear: Whether to create `update.ts` or add to `create.ts`.
   - Recommendation: Add `updateElement` to `create.ts` to avoid helper extraction. The file already contains both `createElement` and `deleteElement`.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in) |
| Config file | none -- bun:test auto-discovers `*.test.ts` |
| Quick run command | `cd bindings/js && bun test test/update.test.ts test/read.test.ts` |
| Full suite command | `bindings/js/test/test.bat` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| JSUP-01 | updateElement modifies scalar/vector/set data | unit | `cd bindings/js && bun test test/update.test.ts` | No -- Wave 0 |
| JSRD-01 | readVectorIntegers/Floats/Strings bulk | unit | `cd bindings/js && bun test test/read.test.ts` | Partial -- file exists, needs new tests |
| JSRD-02 | readVectorIntegers/Floats/StringsById | unit | `cd bindings/js && bun test test/read.test.ts` | Partial -- file exists, needs new tests |
| JSRD-03 | readSetIntegers/Floats/Strings bulk | unit | `cd bindings/js && bun test test/read.test.ts` | Partial -- file exists, needs new tests |
| JSRD-04 | readSetIntegers/Floats/StringsById | unit | `cd bindings/js && bun test test/read.test.ts` | Partial -- file exists, needs new tests |

### Sampling Rate
- **Per task commit:** `cd bindings/js && bun test`
- **Per wave merge:** `bindings/js/test/test.bat`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/update.test.ts` -- covers JSUP-01
- [ ] Add vector/set `describe` blocks to `bindings/js/test/read.test.ts` -- covers JSRD-01 through JSRD-04
- [ ] May need float vector table added to `tests/schemas/valid/all_types.sql`

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- All C API function signatures for vector/set reads, update_element
- `include/quiver/c/element.h` -- Element lifecycle API
- `src/c/database_read.cpp` -- C API implementation of all read functions
- `src/c/database_update.cpp` -- C API implementation of update_element
- `src/c/database_helpers.h` -- `read_vectors_impl`, `free_vectors_impl` templates
- `bindings/js/src/*.ts` -- All existing JS binding source (Phase 1)
- `bindings/js/test/*.test.ts` -- All existing JS tests
- `bindings/dart/lib/src/database_read.dart` -- Dart binding read patterns (reference implementation)
- `bindings/dart/lib/src/database_update.dart` -- Dart binding update patterns (reference implementation)
- `tests/schemas/valid/all_types.sql` -- Test schema with vector/set tables

### Secondary (MEDIUM confidence)
- CLAUDE.md cross-layer naming conventions -- mechanical camelCase transformation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new libraries, all existing tools
- Architecture: HIGH - all patterns directly observed in existing codebase
- Pitfalls: HIGH - identified from actual C API memory layout and pointer levels
- Code examples: HIGH - derived from working Phase 1 code + verified C API signatures

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable -- internal project, no external dependencies changing)
